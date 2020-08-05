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
#include "connection_list.h"
#include "pipe.h"
#include "proxy.h"
#include "socket.h"
#include <winestreamproxy/logger.h>
#include <winestreamproxy/winestreamproxy.h>

#include <stddef.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

BOOL proxy_create(logger_instance* const logger, proxy_parameters const parameters, proxy_data** const out_proxy)
{
    proxy_data* proxy;

    LOG_TRACE(logger, (_T("Creating proxy object")));

    proxy = (proxy_data*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(proxy_data));
    if (!proxy)
    {
        LOG_CRITICAL(logger, (_T("Could not allocate proxy object (%lu bytes)"), sizeof(proxy_data)));
        return FALSE;
    }

    proxy->logger = logger;
    proxy->parameters = parameters;

    proxy->accept_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!proxy->accept_overlapped.hEvent)
    {
        LOG_CRITICAL(logger, (_T("Could not create an event for asynchronous connecting")));
        HeapFree(GetProcessHeap(), 0, proxy);
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Created proxy object")));

    *out_proxy = proxy;
    return TRUE;
}

void proxy_destroy(proxy_data* const proxy)
{
    logger_instance* logger;

    logger = proxy->logger;

    LOG_TRACE(logger, (_T("Destroying proxy object")));

    CloseHandle(proxy->accept_overlapped.hEvent);

    HeapFree(GetProcessHeap(), 0, proxy);

    LOG_TRACE(logger, (_T("Destroyed proxy object")));
}

static BOOL handle_new_connection(logger_instance* const logger, connection_data* const conn)
{
    LOG_TRACE(logger, (_T("Handling new client connection")));

    if (!socket_connect(logger, &conn->socket))
        return FALSE;

    LOG_INFO(logger, (_T("Connected to server socket")));

    if (!connection_launch_threads(conn))
        return FALSE;

    LOG_TRACE(logger, (_T("Threads signaled to start")));

    return TRUE;
}

void proxy_enter_loop(proxy_data* const proxy)
{
    PROXY_STATE state;
    connection_data* conn, * prev_conn;
    BOOL is_async, stop, first_loop;

    state = PROXY_STATE_CREATED;

    if (InterlockedCompareExchange(&proxy->is_running, TRUE, FALSE) != FALSE)
    {
        LOG_CRITICAL(proxy->logger, (_T("Refusing to start proxy loop twice")));
        return;
    }

    if (proxy->parameters.state_change_callback)
    {
        proxy->parameters.state_change_callback(proxy->logger, proxy, state, PROXY_STATE_STARTING);
        state = PROXY_STATE_STARTING;
    }

    LOG_TRACE(proxy->logger, (_T("Starting proxy loop")));

    first_loop = TRUE;

    for (prev_conn = 0; TRUE; prev_conn = conn)
    {
        stop = FALSE;

        if (!connection_list_allocate_entry(proxy->logger, &proxy->conn_list, &conn))
        {
            stop = TRUE;
        }

        if (!stop)
            connection_initialize(proxy, conn);

        if (!stop && !pipe_create_server(proxy->logger, &conn->pipe, proxy->parameters.paths.named_pipe_path))
        {
            connection_list_deallocate_entry(proxy->logger, &proxy->conn_list, conn);
            stop = TRUE;
        }

        if (!stop && !pipe_server_start_accept(proxy->logger, &conn->pipe, &is_async, &proxy->accept_overlapped))
        {
            pipe_close_server(proxy->logger, &conn->pipe);
            connection_list_deallocate_entry(proxy->logger, &proxy->conn_list, conn);
            stop = TRUE;
        }

        if (prev_conn)
        {
            if (stop || !handle_new_connection(proxy->logger, prev_conn))
            {
                connection_close(prev_conn);
                break;
            }
        }

        if (stop)
            break;

        if (!pipe_prepare(proxy->logger, &conn->pipe))
        {
            if (is_async) CancelIoEx(conn->pipe.handle, &proxy->accept_overlapped);
            pipe_close_server(proxy->logger, &conn->pipe);
            connection_list_deallocate_entry(proxy->logger, &proxy->conn_list, conn);
            break;
        }

        if (!socket_prepare(proxy->logger, proxy->parameters.paths.unix_socket_path, &conn->socket))
        {
            if (is_async) CancelIoEx(conn->pipe.handle, &proxy->accept_overlapped);
            pipe_close_server(proxy->logger, &conn->pipe);
            connection_list_deallocate_entry(proxy->logger, &proxy->conn_list, conn);
            break;
        }

        if (!connection_prepare_threads(conn))
        {
            socket_disconnect(proxy->logger, &conn->socket);
            if (is_async) CancelIoEx(conn->pipe.handle, &proxy->accept_overlapped);
            pipe_close_server(proxy->logger, &conn->pipe);
            connection_list_deallocate_entry(proxy->logger, &proxy->conn_list, conn);
            break;
        }

        if (first_loop)
        {
            first_loop = FALSE;

            if (proxy->parameters.state_change_callback)
            {
                proxy->parameters.state_change_callback(proxy->logger, proxy, state, PROXY_STATE_RUNNING);
                state = PROXY_STATE_RUNNING;
            }
            LOG_INFO(proxy->logger, (_T("Started proxy loop")));
        }

        if (is_async)
        {
            if (!pipe_server_wait_accept(proxy->logger, &conn->pipe, proxy->parameters.exit_event,
                                         &proxy->accept_overlapped))
            {
                /*if (is_async)*/ CancelIoEx(conn->pipe.handle, &proxy->accept_overlapped);
                connection_close(conn);
                break;
            }
        }
    }

    if (proxy->parameters.state_change_callback)
    {
        proxy->parameters.state_change_callback(proxy->logger, proxy, state, PROXY_STATE_STOPPING);
        state = PROXY_STATE_STOPPING;
    }

    while (proxy->conn_list.end)
    {
        SetEvent(proxy->parameters.exit_event);
        Sleep(1);
    }

    LOG_INFO(proxy->logger, (_T("Stopped proxy loop")));

    InterlockedExchange(&proxy->is_running, FALSE);

    if (proxy->parameters.state_change_callback)
        proxy->parameters.state_change_callback(proxy->logger, proxy, state, PROXY_STATE_STOPPED);
}
