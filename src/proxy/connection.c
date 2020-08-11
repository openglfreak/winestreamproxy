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
#include "misc.h"
#include "pipe.h"
#include "proxy.h"
#include "socket.h"
#include "thread.h"
#include <winestreamproxy/logger.h>

#include <tchar.h>
#include <unistd.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

BOOL pipe_handler_thunk(logger_instance* const logger, void* const void_conn)
{
    return pipe_handler(logger, (connection_data*)void_conn);
}

void pipe_cleanup_thunk(logger_instance* const logger, thread_data* const data, void* const void_conn)
{
    (void)data;

    pipe_cleanup(logger, (connection_data*)void_conn);
}

BOOL pipe_stop_thread_thunk(logger_instance* const logger, thread_data* const data)
{
    return pipe_stop_thread(logger, container_of(data, pipe_data, thread));
}

thread_description pipe_thread_description = {
    pipe_handler_thunk,
    pipe_cleanup_thunk,
    pipe_stop_thread_thunk
};

BOOL socket_handler_thunk(logger_instance* const logger, void* const void_conn)
{
    return socket_handler(logger, (connection_data*)void_conn);
}

void socket_cleanup_thunk(logger_instance* const logger, thread_data* const data, void* const void_conn)
{
    (void)data;

    socket_cleanup(logger, (connection_data*)void_conn);
}

BOOL socket_stop_thread_thunk(logger_instance* const logger, thread_data* const data)
{
    return socket_stop_thread(logger, container_of(data, socket_data, thread));
}

thread_description socket_thread_description = {
    socket_handler_thunk,
    socket_cleanup_thunk,
    socket_stop_thread_thunk
};

void connection_initialize(proxy_data* const proxy, connection_data* const conn)
{
    conn->proxy = proxy;
}

BOOL connection_prepare_threads(connection_data* const conn)
{
    LOG_TRACE(conn->proxy->logger, (_T("Preparing connection threads")));

    if (!thread_prepare(conn->proxy->logger, &pipe_thread_description, &conn->pipe.thread, conn))
        return FALSE;

    if (!thread_prepare(conn->proxy->logger, &socket_thread_description, &conn->socket.thread, conn))
    {
        thread_dispose(conn->proxy->logger, &pipe_thread_description, &conn->pipe.thread);
        return TRUE;
    }

    LOG_TRACE(conn->proxy->logger, (_T("Prepared connection threads")));

    return TRUE;
}

BOOL connection_launch_threads(connection_data* const conn)
{
    LOG_TRACE(conn->proxy->logger, (_T("Launching connection threads")));

    if (thread_run(conn->proxy->logger, &pipe_thread_description, &conn->pipe.thread) == THREAD_RUN_ERROR_OTHER)
        return FALSE;

    if (thread_run(conn->proxy->logger, &socket_thread_description, &conn->socket.thread) == THREAD_RUN_ERROR_OTHER)
    {
        thread_stop(conn->proxy->logger, &pipe_thread_description, &conn->pipe.thread);
        return FALSE;
    }

    LOG_TRACE(conn->proxy->logger, (_T("Launched connection threads")));

    return TRUE;
}

void connection_close(connection_data* const conn)
{
    LOG_TRACE(conn->proxy->logger, (_T("Closing connection")));

    thread_dispose(conn->proxy->logger, &pipe_thread_description, &conn->pipe.thread);
    thread_dispose(conn->proxy->logger, &socket_thread_description, &conn->socket.thread);

    LOG_TRACE(conn->proxy->logger, (_T("Closed connection")));
}
