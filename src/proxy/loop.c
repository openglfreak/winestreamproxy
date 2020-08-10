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
#include <winestreamproxy/winestreamproxy.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

static DWORD CALLBACK pipe_thread_proc(LPVOID const lpvoid_conn)
{
    HANDLE wait_handles[2];
    DWORD wait_result;

#   define conn ((connection_data*)lpvoid_conn)

    LOG_TRACE(conn->proxy->logger, (_T("Started prepared pipe thread")));

    wait_handles[0] = conn->start_events.pipe;
    wait_handles[1] = conn->proxy->exit_event;

    do {
        wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
    } while (wait_result == WAIT_TIMEOUT);

    CloseHandle(conn->start_events.pipe);

    switch (wait_result)
    {
        case WAIT_OBJECT_0:
            if (!conn->start)
            {
                LOG_TRACE(conn->proxy->logger, (_T("Stopping prepared pipe thread")));
                return 0;
            }
            LOG_TRACE(conn->proxy->logger, (_T("Starting pipe handling loop")));
            return !pipe_handler(conn);
        case WAIT_OBJECT_0 + 1:
            LOG_TRACE(conn->proxy->logger, (_T("Stopping prepared pipe thread")));
            return 0;
        case WAIT_FAILED:
            LOG_ERROR(conn->proxy->logger, (
                _T("Waiting for pipe thread start event failed: Error %d"),
                GetLastError()
            ));
            break;
        default:
            LOG_ERROR(conn->proxy->logger, (
                _T("Unexpected return value from WaitForMultipleObjects: Ret %d Error %d"),
                wait_result,
                GetLastError()
            ));
            break;
    }

    return 1;

#   undef conn
}

static DWORD CALLBACK socket_thread_proc(LPVOID const lpvoid_conn)
{
    HANDLE wait_handles[2];
    DWORD wait_result;

#   define conn ((connection_data*)lpvoid_conn)

    LOG_TRACE(conn->proxy->logger, (_T("Started prepared socket thread")));

    wait_handles[0] = conn->start_events.socket;
    wait_handles[1] = conn->proxy->exit_event;

    do {
        wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
    } while (wait_result == WAIT_TIMEOUT);

    CloseHandle(conn->start_events.socket);

    switch (wait_result)
    {
        case WAIT_OBJECT_0:
            if (!conn->start)
            {
                LOG_TRACE(conn->proxy->logger, (_T("Stopping prepared socket thread")));
                return 0;
            }
            LOG_TRACE(conn->proxy->logger, (_T("Starting socket handling loop")));
            return !socket_handler(conn);
        case WAIT_OBJECT_0 + 1:
            LOG_TRACE(conn->proxy->logger, (_T("Stopping prepared socket thread")));
            return 0;
        case WAIT_FAILED:
            LOG_ERROR(conn->proxy->logger, (
                _T("Waiting for socket thread start event failed: Error %d"),
                GetLastError()
            ));
            break;
        default:
            LOG_ERROR(conn->proxy->logger, (
                _T("Unexpected return value from WaitForMultipleObjects: Ret %d Error %d"),
                wait_result,
                GetLastError()
            ));
            break;
    }

    return 1;

#   undef conn
}

static BOOL prepare_connection(proxy_data* proxy, char const* socket_path,
                               HANDLE const pipe, connection_data* const conn)
{
    LOG_TRACE(proxy->logger, (_T("Preparing connection data")));

    conn->proxy = proxy;
    conn->start = FALSE;

    if (!prepare_pipe_data(proxy->logger, pipe, &conn->data.pipe))
        return FALSE;

    conn->start_events.pipe = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!conn->start_events.pipe)
    {
        LOG_CRITICAL(proxy->logger, (
            _T("Could not create a thread start event: Error %d"),
            GetLastError()
        ));
        discard_prepared_pipe_data(proxy->logger, &conn->data.pipe);
        HeapFree(GetProcessHeap(), 0, conn);
        return FALSE;
    }

    if (!DuplicateHandle(GetCurrentProcess(), conn->start_events.pipe,
                         GetCurrentProcess(), &conn->start_events.socket,
                         0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        LOG_CRITICAL(proxy->logger, (
            _T("Could not duplicate thread start event: Error %d"),
            GetLastError()
        ));
        CloseHandle(conn->start_events.pipe);
        discard_prepared_pipe_data(proxy->logger, &conn->data.pipe);
        HeapFree(GetProcessHeap(), 0, conn);
        return FALSE;
    }

    if (!prepare_socket(proxy->logger, socket_path, &conn->data.socket))
    {
        CloseHandle(conn->start_events.socket);
        CloseHandle(conn->start_events.pipe);
        discard_prepared_pipe_data(proxy->logger, &conn->data.pipe);
        HeapFree(GetProcessHeap(), 0, conn);
        return FALSE;
    }

    conn->threads.pipe = CreateThread(NULL, 0, pipe_thread_proc, (LPVOID)conn, CREATE_SUSPENDED, NULL);
    if (conn->threads.pipe == NULL)
    {
        LOG_CRITICAL(proxy->logger, (
            _T("Could not create thread for named pipe: Error %d"),
            GetLastError()
        ));
        discard_prepared_socket(proxy->logger, &conn->data.socket);
        CloseHandle(conn->start_events.socket);
        CloseHandle(conn->start_events.pipe);
        discard_prepared_pipe_data(proxy->logger, &conn->data.pipe);
        HeapFree(GetProcessHeap(), 0, conn);
        return FALSE;
    }

    conn->threads.socket = CreateThread(NULL, 0, socket_thread_proc, (LPVOID)conn, 0, NULL);
    if (conn->threads.socket == NULL)
    {
        LOG_CRITICAL(proxy->logger, (
            _T("Could not create thread for socket: Error %d"),
            GetLastError()
        ));
        SetEvent(conn->start_events.pipe);
        ResumeThread(conn->threads.pipe);
        CloseHandle(conn->threads.pipe);
        discard_prepared_socket(proxy->logger, &conn->data.socket);
        CloseHandle(conn->start_events.socket);
        CloseHandle(conn->start_events.pipe);
        discard_prepared_pipe_data(proxy->logger, &conn->data.pipe);
        HeapFree(GetProcessHeap(), 0, conn);
        return FALSE;
    }

    if (ResumeThread(conn->threads.pipe) == (DWORD)-1)
    {
        LOG_CRITICAL(proxy->logger, (_T("Could not start thread for pipe: Error %d"), GetLastError()));
        SetEvent(conn->start_events.pipe);
        CloseHandle(conn->threads.socket);
        CloseHandle(conn->threads.pipe);
        discard_prepared_socket(proxy->logger, &conn->data.socket);
        CloseHandle(conn->start_events.socket);
        CloseHandle(conn->start_events.pipe);
        discard_prepared_pipe_data(proxy->logger, &conn->data.pipe);
        HeapFree(GetProcessHeap(), 0, conn);
        return FALSE;
    }

    LOG_TRACE(proxy->logger, (_T("Prepared connection data")));

    return TRUE;
}

static void discard_prepared_connection(logger_instance* const logger, connection_data* const conn)
{
    LOG_TRACE(logger, (_T("Discarding prepared connection object")));

    SetEvent(conn->start_events.socket);
    SetEvent(conn->start_events.pipe);
    CloseHandle(conn->threads.socket);
    CloseHandle(conn->threads.pipe);
    CloseHandle(conn->data.pipe.write_overlapped.hEvent);
    CloseHandle(conn->data.pipe.thread_exit_event);

    LOG_TRACE(logger, (_T("Discarded prepared connection object")));
}

static BOOL handle_new_connection(logger_instance* const logger, connection_data* const conn)
{
    LOG_TRACE(logger, (_T("Handling new client connection")));

    if (!connect_socket(logger, &conn->data.socket))
        return FALSE;

    LOG_INFO(logger, (_T("Connected to server socket")));

    conn->start = TRUE;
    if (!SetEvent(conn->start_events.pipe))
    {
        LOG_ERROR(logger, (_T("Failed setting thread start event: Error %d"), GetLastError()));
        conn->start = FALSE;
        close_socket(conn, FALSE);
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Threads signaled to start")));

    return TRUE;
}

static void close_connection(connection_data* conn)
{
    LOG_TRACE(conn->proxy->logger, (_T("Closing connection")));

    conn->start = FALSE;
    SetEvent(conn->start_events.pipe);
    close_socket(conn, TRUE);
    close_pipe(conn, TRUE);
    CloseHandle(conn->threads.pipe);
    CloseHandle(conn->threads.socket);

    LOG_TRACE(conn->proxy->logger, (_T("Closed connection")));
}

static void close_all_connections(proxy_data* proxy)
{
    connection_list_entry* entry;

    LOG_TRACE(proxy->logger, (_T("Closing all connections")));

    entry = proxy->connections_start;
    while (entry)
    {
        connection_list_entry* next;

        close_connection(&entry->connection);
        next = entry->next;
        deallocate_connection(proxy, &entry->connection);
        entry = next;
    }

    LOG_TRACE(proxy->logger, (_T("Closed all connections")));
}

void enter_proxy_loop(proxy_data* proxy)
{
    HANDLE pipe;
    BOOL is_async;
    BOOL first_loop;

#   define align_32(x) ((void*)~(~((UINT_PTR)(x)) | (((32 + CHAR_BIT - 1) / CHAR_BIT) - 1)))
#   define aligned_running ((LONG volatile*)align_32(&proxy->running[((32 + CHAR_BIT - 1) / CHAR_BIT) - 1]))

    if (InterlockedCompareExchange(aligned_running, TRUE, FALSE) != FALSE)
    {
        LOG_CRITICAL(proxy->logger, (_T("Refusing to start proxy loop twice")));
        return;
    }

    LOG_TRACE(proxy->logger, (_T("Starting pipe server loop")));

    if (!create_pipe(proxy->logger, proxy->paths.named_pipe_path, &pipe))
    {
        *aligned_running = FALSE;
        return;
    }

    if (!start_connecting_pipe(proxy->logger, pipe, &is_async, &proxy->connect_overlapped))
    {
        CloseHandle(pipe);
        *aligned_running = FALSE;
        return;
    }

    first_loop = TRUE;
    while (TRUE)
    {
        connection_data* conn;

        if (!allocate_connection(proxy, &conn))
        {
            if (is_async)
                CancelIoEx(pipe, &proxy->connect_overlapped);
            CloseHandle(pipe);
            break;
        }

        if (!prepare_connection(proxy, proxy->paths.unix_socket_path, pipe, conn))
        {
            deallocate_connection(proxy, conn);
            if (is_async)
                CancelIoEx(pipe, &proxy->connect_overlapped);
            CloseHandle(pipe);
            break;
        }

        if (first_loop)
        {
            if (proxy->running_callback)
                proxy->running_callback(proxy);
            LOG_INFO(proxy->logger, (_T("Started pipe server loop")));
            first_loop = FALSE;
        }

        if (is_async && !wait_for_pipe_connection(proxy->logger, proxy->exit_event, &proxy->connect_overlapped))
        {
            discard_prepared_connection(proxy->logger, conn);
            deallocate_connection(proxy, conn);
            CancelIoEx(conn->data.pipe.handle, &proxy->connect_overlapped);
            CloseHandle(conn->data.pipe.handle);
            break;
        }

        if (!create_pipe(proxy->logger, proxy->paths.named_pipe_path, &pipe))
        {
            discard_prepared_connection(proxy->logger, conn);
            deallocate_connection(proxy, conn);
            CloseHandle(conn->data.pipe.handle);
            break;
        }

        if (!start_connecting_pipe(proxy->logger, pipe, &is_async, &proxy->connect_overlapped))
        {
            CloseHandle(pipe);
            discard_prepared_connection(proxy->logger, conn);
            deallocate_connection(proxy, conn);
            CloseHandle(conn->data.pipe.handle);
            break;
        }

        if (!handle_new_connection(proxy->logger, conn))
        {
            discard_prepared_connection(proxy->logger, conn);
            deallocate_connection(proxy, conn);
            CloseHandle(conn->data.pipe.handle);
            continue;
        }
    }

    close_all_connections(proxy);

    LOG_TRACE(proxy->logger, (_T("Stopped pipe server loop")));

    *aligned_running = FALSE;

#   undef aligned_running
#   undef align_32
}
