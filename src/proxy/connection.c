/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "connection.h"
#include "pipe.h"
#include "proxy.h"
#include "socket.h"
#include <winestreamproxy/logger.h>

#include <tchar.h>
#include <unistd.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

static DWORD thread_proc(connection_data* const conn, TCHAR const* const type, HANDLE const trigger_event,
                         LONG volatile const* const action_ptr, LONG volatile* const status_ptr,
                         BOOL (* const handler)(connection_data*))
{
    DWORD wait_result;
    DWORD ret;

    LOG_TRACE(conn->proxy->logger, (_T("Started prepared %s thread"), type));

    *status_ptr = 1;

    do {
        wait_result = WaitForSingleObject(trigger_event, INFINITE);
    } while (wait_result == WAIT_TIMEOUT);

    switch (wait_result)
    {
        case WAIT_OBJECT_0:
            switch (*action_ptr)
            {
                case 0:
                    LOG_TRACE(conn->proxy->logger, (_T("Stopping prepared %s thread"), type));
                    ret = 0;
                    break;
                case 1:
                    LOG_TRACE(conn->proxy->logger, (_T("Starting %s handling loop"), type));
                    *status_ptr = 2;
                    ret = !handler(conn);
                    *status_ptr = 3;
                    break;
                default:
                    LOG_ERROR(conn->proxy->logger, (
                        _T("Invalid action for %s thread: %d"),
                        type,
                        *action_ptr
                    ));
                    ret = 1;
            }
            break;
        case WAIT_FAILED:
            ret = GetLastError();
            LOG_ERROR(conn->proxy->logger, (
                _T("Error while waiting for %s thread trigger event: Error %d"),
                type,
                ret
            ));
            if (!ret) ret = 1;
            break;
        default:
            ret = GetLastError();
            LOG_ERROR(conn->proxy->logger, (
                _T("Unexpected return value from WaitForMultipleObjects: Ret %d Error %d"),
                wait_result,
                ret
            ));
            if (!ret) ret = 1;
    }

    *status_ptr = 4;

    return ret;
}

static DWORD CALLBACK pipe_thread_proc(LPVOID const lpvoid_conn)
{
    connection_data* conn;
    DWORD ret;

    conn = (connection_data*)lpvoid_conn;
    ret = thread_proc(
        conn,
        _T("pipe"),
        conn->pipe.trigger_event,
        &conn->pipe.action,
        &conn->pipe.status,
        pipe_handler
    );

    if (conn->pipe.read_is_overlapped)
        CancelIoEx(conn->pipe.handle, &conn->pipe.read_overlapped);
    CloseHandle(conn->pipe.read_overlapped.hEvent);
    if (conn->pipe.write_is_overlapped)
        CancelIoEx(conn->pipe.handle, &conn->pipe.write_overlapped);
    CloseHandle(conn->pipe.write_overlapped.hEvent);
    DisconnectNamedPipe(conn->pipe.handle);
    CloseHandle(conn->pipe.handle);

    CloseHandle(conn->pipe.trigger_event);
    CloseHandle(conn->pipe.thread);
    return ret;
}

static DWORD CALLBACK socket_thread_proc(LPVOID const lpvoid_conn)
{
    connection_data* conn;
    DWORD ret;

    conn = (connection_data*)lpvoid_conn;
    ret = thread_proc(
        conn,
        _T("socket"),
        conn->socket.trigger_event,
        &conn->socket.action,
        &conn->socket.status,
        socket_handler
    );

    close(conn->socket.fd);
    close(conn->socket.thread_exit_eventfd);

    CloseHandle(conn->socket.trigger_event);
    CloseHandle(conn->socket.thread);
    return ret;
}

static void stop_threads(connection_data* const conn)
{
    if (conn->pipe.status != 4)
    {
        if (conn->pipe.status != 0)
            pipe_stop_thread(conn->proxy->logger, &conn->pipe);
        else
        {
            conn->pipe.action = 0;
            if (!SetEvent(conn->pipe.trigger_event))
                LOG_ERROR(conn->proxy->logger, (
                    _T("Signaling pipe thread trigger event failed: Error %d"),
                    GetLastError()
                ));
        }
    }

    if (conn->socket.status != 4)
    {
        if (conn->socket.status != 0)
            socket_stop_thread(conn->proxy->logger, &conn->socket);
        else
        {
            conn->socket.action = 0;
            if (!SetEvent(conn->socket.trigger_event))
                LOG_ERROR(conn->proxy->logger, (
                    _T("Signaling socket thread trigger event failed: Error %d"),
                    GetLastError()
                ));
        }
    }
}

void connection_initialize(proxy_data* const proxy, connection_data* const conn)
{
    conn->proxy = proxy;
}

BOOL connection_prepare_threads(connection_data* const conn)
{
    LOG_TRACE(conn->proxy->logger, (_T("Preparing connection threads")));

    conn->pipe.trigger_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!conn->pipe.trigger_event)
    {
        LOG_CRITICAL(conn->proxy->logger, (
            _T("Could not create pipe thread trigger event: Error %d"),
            GetLastError()
        ));
        return FALSE;
    }
    conn->socket.trigger_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!conn->socket.trigger_event)
    {
        LOG_CRITICAL(conn->proxy->logger, (
            _T("Could not create socket thread trigger event: Error %d"),
            GetLastError()
        ));
        CloseHandle(conn->pipe.trigger_event);
        return FALSE;
    }

    conn->pipe.thread = CreateThread(NULL, 0, pipe_thread_proc, (LPVOID)conn, 0, NULL);
    if (conn->pipe.thread == NULL)
    {
        LOG_CRITICAL(conn->proxy->logger, (
            _T("Could not create thread for pipe: Error %d"),
            GetLastError()
        ));
        CloseHandle(conn->socket.trigger_event);
        CloseHandle(conn->pipe.trigger_event);
        return FALSE;
    }
    conn->socket.thread = CreateThread(NULL, 0, socket_thread_proc, (LPVOID)conn, 0, NULL);
    if (conn->socket.thread == NULL)
    {
        LOG_CRITICAL(conn->proxy->logger, (_T("Could not create thread for socket: Error %d"), GetLastError()));
        conn->pipe.status = 4;
        stop_threads(conn);
        CloseHandle(conn->socket.trigger_event);
        return FALSE;
    }

    LOG_TRACE(conn->proxy->logger, (_T("Prepared connection threads")));

    return TRUE;
}

BOOL connection_launch_threads(connection_data* const conn)
{
    LOG_TRACE(conn->proxy->logger, (_T("Launching connection threads")));

    if (conn->pipe.status == 0)
    {
        conn->pipe.action = 1;
        if (!SetEvent(conn->pipe.trigger_event))
        {
            LOG_CRITICAL(conn->proxy->logger, (
                _T("Signaling pipe thread trigger event failed: Error %d"),
                GetLastError()
            ));
            return FALSE;
        }
    }

    if (conn->socket.status == 0)
    {
        conn->socket.action = 1;
        if (!SetEvent(conn->socket.trigger_event))
        {
            LOG_CRITICAL(conn->proxy->logger, (
                _T("Signaling socket thread trigger event failed: Error %d"),
                GetLastError()
            ));
            return FALSE;
        }
    }

    LOG_TRACE(conn->proxy->logger, (_T("Launched connection threads")));

    return TRUE;
}

void connection_close(connection_data* const conn)
{
    LOG_TRACE(conn->proxy->logger, (_T("Closing connection")));

    if (conn->pipe.status < 2)
        pipe_close_server(conn->proxy->logger, &conn->pipe);
    if (conn->socket.status < 2)
        socket_disconnect(conn->proxy->logger, &conn->socket);
    stop_threads(conn);

    LOG_TRACE(conn->proxy->logger, (_T("Closed connection")));
}
