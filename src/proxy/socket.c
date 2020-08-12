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
#include "socket.h"
#include "thread.h"
#include <winestreamproxy/logger.h>

#include <errno.h>
#include <stddef.h>

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <tchar.h>
#include <unistd.h>
#include <windef.h>
#include <winnt.h>

#define InterlockedRead(x) InterlockedCompareExchange((x), 0, 0)

BOOL socket_prepare(logger_instance* const logger, char const* const unix_socket_path, socket_data* const _socket)
{
    size_t socket_path_len;

    LOG_TRACE(logger, (_T("Preparing socket")));

    socket_path_len = strlen(unix_socket_path);
    if (socket_path_len > sizeof(_socket->addr.sun_path) - 1)
    {
        LOG_CRITICAL(logger, (_T("Socket path too long")));
        return FALSE;
    }

    RtlZeroMemory(&_socket->addr, sizeof(_socket->addr));
    _socket->addr.sun_family = AF_UNIX;
    RtlCopyMemory(_socket->addr.sun_path, unix_socket_path, socket_path_len);

    _socket->thread_exit_eventfd = eventfd(0, EFD_CLOEXEC);
    if (_socket->thread_exit_eventfd == -1)
    {
        LOG_CRITICAL(logger, (_T("Failed to create eventfd: Error %d"), errno));
        return FALSE;
    }

    _socket->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_socket->fd == -1)
    {
        LOG_CRITICAL(logger, (_T("Failed to create socket: Error %d"), errno));
        close(_socket->thread_exit_eventfd);
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Prepared socket")));

    return TRUE;
}

BOOL socket_connect(logger_instance* const logger, socket_data* const socket)
{
    LOG_TRACE(logger, (_T("Connecting socket")));

    if (connect(socket->fd, (struct sockaddr*)&socket->addr, sizeof(socket->addr)) != 0)
    {
        LOG_ERROR(logger, (_T("Failed to connect to socket: Error %d"), errno));
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Connected socket")));

    return TRUE;
}

BOOL socket_disconnect(logger_instance* const logger, socket_data* const socket)
{
    LOG_TRACE(logger, (_T("Closing socket")));

    close(socket->fd);
    close(socket->thread_exit_eventfd);

    LOG_TRACE(logger, (_T("Closed socket")));

    return TRUE;
}

void socket_cleanup(logger_instance* const logger, connection_data* const conn)
{
    LOG_TRACE(logger, (_T("Cleaning up after socket thread")));

    if (InterlockedIncrement(&conn->do_cleanup) >= 2)
    {
        pipe_close_server(logger, &conn->pipe);
        socket_disconnect(logger, &conn->socket);
    }
    else
    {
        LOG_DEBUG(logger, (_T("Pipe thread is still running, not closing socket")));
        thread_dispose(logger, &pipe_thread_description, &conn->pipe.thread);
    }

    LOG_TRACE(logger, (_T("Cleaned up after socket thread")));
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

BOOL socket_stop_thread(logger_instance* const logger, socket_data* const socket)
{
    BOOL ret = TRUE;

    LOG_TRACE(logger, (_T("Stopping socket thread")));

    if (!signal_eventfd(logger, socket->thread_exit_eventfd))
        ret = FALSE;
    else if (!thread_wait(logger, &socket_thread_description, &socket->thread))
        ret = FALSE;

    LOG_TRACE(logger, (_T("Stopped socket thread")));

    return ret;
}

#define STARTING_BUFFER_SIZE 1024

typedef enum RECV_MSG_RET {
    RECV_MSG_RET_SUCCESS,
    RECV_MSG_RET_FAILURE,
    RECV_MSG_RET_SHUTDOWN,
    RECV_MSG_RET_EXIT
} RECV_MSG_RET;

static RECV_MSG_RET socket_receive_message(logger_instance* const logger, socket_data* const socket,
                                           unsigned char** const inout_buffer, size_t* const inout_buffer_size,
                                           size_t* const out_message_length)
{
    struct pollfd fds[2];
    int nfds;

    LOG_TRACE(logger, (_T("Waiting for message from socket")));

    if (!*inout_buffer)
    {
        *inout_buffer_size = STARTING_BUFFER_SIZE;
        *inout_buffer = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, sizeof(unsigned char) * STARTING_BUFFER_SIZE);
        if (!*inout_buffer)
        {
            LOG_ERROR(logger, (
                _T("Failed to allocate %lu bytes"),
                (unsigned long)(sizeof(unsigned char) * STARTING_BUFFER_SIZE)
            ));
            return RECV_MSG_RET_FAILURE;
        }
    }

    fds[0].fd = socket->fd;
    fds[0].events = POLLIN | POLLPRI | POLLHUP;
    fds[0].revents = 0;
    fds[1].fd = socket->thread_exit_eventfd;
    fds[1].events = POLLIN | POLLPRI | POLLHUP;
    fds[1].revents = 0;
    while ((nfds = poll(fds, sizeof(fds) / sizeof(fds[0]), -1)) == 0 || nfds == EAGAIN || nfds == EINTR);
    if (nfds == -1)
    {
        LOG_ERROR(logger, (_T("Call to poll() failed: Error %d"), errno));
        return RECV_MSG_RET_FAILURE;
    }

    if (fds[1].revents)
    {
        LOG_DEBUG(logger, (_T("Socket thread: Received exit event")));
        return RECV_MSG_RET_EXIT;
    }

    if (fds[0].revents & POLLHUP)
    {
        LOG_INFO(logger, (_T("Server closed connection")));
        return RECV_MSG_RET_SHUTDOWN;
    }

    if (!(fds[0].revents & (POLLIN | POLLPRI)))
    {
        LOG_ERROR(logger, (_T("Unknown flags returned from poll: %d"), (int)fds[0].revents));
        return RECV_MSG_RET_FAILURE;
    }

    while (TRUE)
    {
        ssize_t recv_ret;
        size_t new_buffer_size;
        unsigned char* new_buffer;

        LOG_TRACE(logger, (_T("Reading message from socket")));

        recv_ret = recv(socket->fd, *inout_buffer, *inout_buffer_size, MSG_PEEK);
        if (recv_ret == -1)
        {
            LOG_ERROR(logger, (_T("Reading from socket failed: Error %d"), errno));
            return RECV_MSG_RET_FAILURE;
        }

        if ((size_t)recv_ret < *inout_buffer_size)
        {
            ssize_t recv_ret2;

            recv_ret2 = recv(socket->fd, *inout_buffer, (size_t)recv_ret, 0);
            if (recv_ret2 == recv_ret)
            {
                *out_message_length = (size_t)recv_ret;
                break;
            }
            else if (recv_ret2 == -1)
                LOG_ERROR(logger, (_T("Reading from socket failed: Error %d"), errno));
            else
                LOG_ERROR(logger, (_T("Discarded socket data")));
            return RECV_MSG_RET_FAILURE;
        }

        new_buffer_size = recv_ret * 2;
        new_buffer = (unsigned char*)HeapReAlloc(GetProcessHeap(), 0, *inout_buffer,
                                                 sizeof(unsigned char) * new_buffer_size);
        if (!new_buffer)
        {
            LOG_ERROR(logger, (
                _T("Failed to resize incoming socket data buffer from %lu to %lu bytes"),
                (unsigned long)*inout_buffer_size,
                (unsigned long)(sizeof(unsigned char) * new_buffer_size)
            ));
            return RECV_MSG_RET_FAILURE;
        }
        *inout_buffer_size = new_buffer_size;
        *inout_buffer = new_buffer;
    }

    LOG_TRACE(logger, (_T("Read message from socket")));

    return RECV_MSG_RET_SUCCESS;
}

BOOL socket_handler(logger_instance* const logger, connection_data* const conn)
{
    unsigned char* buffer;
    size_t buffer_size;
    BOOL ret;

    LOG_TRACE(logger, (_T("Entering socket handler loop")));

    buffer = 0;
    buffer_size = 0;
    while (TRUE)
    {
        size_t message_length;
        RECV_MSG_RET recv_ret;

        recv_ret = socket_receive_message(logger, &conn->socket, &buffer, &buffer_size, &message_length);
        if (recv_ret != RECV_MSG_RET_SUCCESS)
        {
            ret = recv_ret != RECV_MSG_RET_FAILURE;
            break;
        }

        if (LOG_IS_ENABLED(logger, LOG_LEVEL_DEBUG))
        {
            LOG_DEBUG(logger, (_T("Passing %lu bytes from socket to pipe"), message_length));
            dbg_output_bytes(logger, _T("Message from socket: "), buffer, message_length);
        }

        if (!pipe_send_message(logger, &conn->pipe, buffer, message_length))
        {
            ret = InterlockedRead(&conn->pipe.thread.status) >= THREAD_STATUS_STOPPING;
            break;
        }
    }

    if (buffer)
        HeapFree(GetProcessHeap(), 0, buffer);
    connection_close(conn);

    LOG_TRACE(logger, (_T("Exited socket handler loop")));

    return ret;
}

BOOL socket_send_message(logger_instance* const logger, socket_data* const socket, unsigned char const* const message,
                         size_t const message_length)
{
    ssize_t bytes_written;

    LOG_TRACE(logger, (_T("Sending message to socket")));

    if (InterlockedRead(&socket->thread.status) >= THREAD_STATUS_STOPPING)
    {
        LOG_ERROR(logger, (_T("Can't send message to closed socket")));
        return FALSE;
    }

    bytes_written = write(socket->fd, message, message_length);
    if (bytes_written == -1)
    {
        LOG_ERROR(logger, (_T("Error %d while writing to socket"), errno));
        return FALSE;
    }
    else if ((size_t)bytes_written != message_length)
    {
        LOG_ERROR(logger, (
            (size_t)bytes_written < message_length
            ? _T("Partial write to socket: %lu < %lu")
            : _T("Invalid return value from write: %lu > %lu"),
            (unsigned long)bytes_written,
            (unsigned long)message_length
        ));
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Sent message to socket")));

    return TRUE;
}
