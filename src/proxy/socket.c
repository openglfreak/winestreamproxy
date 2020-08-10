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
#include "wait_thread.h"
#include <winestreamproxy/logger.h>

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <tchar.h>
#include <unistd.h>
#include <windef.h>
#include <winnt.h>

BOOL prepare_socket(logger_instance* const logger, char const* const socket_path, socket_data* const _socket)
{
    size_t socket_path_len;

    LOG_TRACE(logger, (_T("Preparing socket")));

    socket_path_len = strlen(socket_path);
    if (socket_path_len > sizeof(_socket->addr.sun_path) - 1)
    {
        LOG_CRITICAL(logger, (_T("Socket path too long")));
        return FALSE;
    }

    RtlZeroMemory(&_socket->addr, sizeof(_socket->addr));
    _socket->addr.sun_family = AF_UNIX;
    RtlCopyMemory(_socket->addr.sun_path, socket_path, socket_path_len);

    _socket->eventfd = eventfd(0, EFD_CLOEXEC);
    if (_socket->eventfd == -1)
    {
        LOG_CRITICAL(logger, (_T("Failed to create eventfd: Error %d"), errno));
        return FALSE;
    }

    _socket->socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_socket->socketfd == -1)
    {
        LOG_CRITICAL(logger, (_T("Failed to create socket: Error %d"), errno));
        close(_socket->eventfd);
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Prepared socket")));

    return TRUE;
}

void discard_prepared_socket(logger_instance* const logger, socket_data* const socket)
{
    LOG_TRACE(logger, (_T("Discarding prepared socket")));

    close(socket->eventfd);
    close(socket->socketfd);

    LOG_TRACE(logger, (_T("Discarded prepared socket")));
}

BOOL connect_socket(logger_instance* const logger, socket_data* const socket)
{
    LOG_TRACE(logger, (_T("Connecting socket")));

    if (connect(socket->socketfd, (struct sockaddr*)&socket->addr, sizeof(socket->addr)) != 0)
    {
        LOG_ERROR(logger, (_T("Failed to connect to socket: Error %d"), errno));
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Connected socket")));

    return TRUE;
}

static BOOL signal_eventfd(logger_instance* const logger, int const eventfd)
{
    static char one[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };

    if (write(eventfd, one, 8) != 8)
    {
        LOG_ERROR(logger, (_T("Could not send exit signal to socket thread")));
        return FALSE;
    }

    return TRUE;
}

void close_socket(connection_data* const conn, BOOL const exit_thread)
{
    if (conn->closing.socket || conn->closed.socket)
        return;

    LOG_TRACE(conn->proxy->logger, (_T("Closing socket")));

    if (exit_thread)
        if (signal_eventfd(conn->proxy->logger, conn->data.socket.eventfd))
            wait_for_thread(conn->proxy->logger, _T("socket"), conn->threads.socket);

    close(conn->data.socket.socketfd);
    close(conn->data.socket.eventfd);

    conn->closed.socket = TRUE;

    LOG_TRACE(conn->proxy->logger, (_T("Closed socket")));
}
