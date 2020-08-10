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
#include "dbg_output_bytes.h"
#include "pipe.h"
#include "proxy.h"
#include "socket.h"
#include <winestreamproxy/logger.h>

#include <errno.h>
#include <stddef.h>

#include <poll.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

#define BUFSIZE 4096

static BOOL receive_socket_message(connection_data* const conn, char** const buffer,
                                   size_t* const buffer_size, size_t* const out_message_length,
                                   BOOL* out_exit)
{
    struct pollfd fds[2];
    int nfds;
    ssize_t message_length;

    LOG_TRACE(conn->proxy->logger, (_T("Reading message from socket")));

    if (!*buffer)
    {
        *buffer_size = BUFSIZE;
        *buffer = (char*)HeapAlloc(GetProcessHeap(), 0, sizeof(char) * BUFSIZE);

        if (!*buffer)
        {
            LOG_ERROR(conn->proxy->logger, (_T("Failed to allocate %lu bytes"), sizeof(char) * BUFSIZE));
            return FALSE;
        }
    }

    fds[0].fd = conn->data.socket.socketfd;
    fds[0].events = POLLIN | POLLPRI | POLLHUP;
    fds[0].revents = 0;
    fds[1].fd = conn->data.socket.eventfd;
    fds[1].events = POLLIN | POLLPRI | POLLHUP;
    fds[1].revents = 0;
    while ((nfds = poll(fds, 2, -1)) == 0 || nfds == EAGAIN || nfds == EINTR);
    if (nfds == -1)
    {
        LOG_ERROR(conn->proxy->logger, (_T("poll failed: Error %d"), errno));
        return FALSE;
    }

    if (fds[1].revents)
    {
        LOG_DEBUG(conn->proxy->logger, (_T("Socket thread: Received exit event")));
        *out_exit = TRUE;
        return TRUE;
    }

    if (fds[0].revents & POLLHUP)
    {
        LOG_INFO(conn->proxy->logger, (_T("Server closed connection")));
        *out_exit = TRUE;
        return TRUE;
    }

    if (!(fds[0].revents & (POLLIN | POLLPRI)))
    {
        LOG_ERROR(conn->proxy->logger, (_T("Unknown flags returned from poll: %d"), (int)fds[0].revents));
        return FALSE;
    }

    while (TRUE)
    {
        size_t new_buffer_size;
        char* new_buffer;

        message_length = recv(conn->data.socket.socketfd, *buffer, *buffer_size, MSG_PEEK);
        if (message_length == -1)
        {
            LOG_ERROR(conn->proxy->logger, (_T("Reading from socket failed: Error %d"), errno));
            return FALSE;
        }

        if ((size_t)message_length < *buffer_size)
        {
            ssize_t bytes_read;

            bytes_read = recv(conn->data.socket.socketfd, *buffer, (size_t)message_length, 0);
            if (bytes_read == message_length)
                break;
            else if (bytes_read == -1)
                LOG_ERROR(conn->proxy->logger, (_T("Reading from socket failed: Error %d"), errno));
            else
                LOG_ERROR(conn->proxy->logger, (_T("Discarded socket data")));
            return FALSE;
        }

        new_buffer_size = message_length * 2 * sizeof(char);
        new_buffer = (char*)HeapReAlloc(GetProcessHeap(), 0, *buffer, new_buffer_size);
        if (!new_buffer)
        {
            LOG_ERROR(conn->proxy->logger, (
                _T("Failed to resize incoming socket data buffer from %lu to %lu bytes"),
                (unsigned long)*buffer_size,
                (unsigned long)new_buffer_size
            ));
            return FALSE;
        }
        *buffer = new_buffer;
        *buffer_size = new_buffer_size;
    }

    LOG_TRACE(conn->proxy->logger, (_T("Read message from socket")));

    *out_message_length = (size_t)message_length;
    *out_exit = FALSE;
    return TRUE;
}

static BOOL send_pipe_message(connection_data* const conn, char const* const message, size_t const message_length)
{
    DWORD bytes_written;

    LOG_TRACE(conn->proxy->logger, (_T("Sending message to pipe")));

    if ((DWORD)message_length != message_length)
    {
        LOG_ERROR(conn->proxy->logger, (
            _T("Message size too big: %lu > %lu"),
            (unsigned long)message_length,
            (unsigned long)(DWORD)-1
        ));
        return FALSE;
    }

    if (conn->data.pipe.write_is_overlapped &&
        !GetOverlappedResult(conn->data.pipe.handle, (LPOVERLAPPED)&conn->data.pipe.write_overlapped,
                             &bytes_written, FALSE))
    {
        LOG_ERROR(conn->proxy->logger, (_T("Error in previous write: Error %d"), GetLastError()));
        return FALSE;
    }

    if (!WriteFile(conn->data.pipe.handle, (LPCVOID)message, (DWORD)message_length, NULL,
                   (LPOVERLAPPED)&conn->data.pipe.write_overlapped))
    {
        DWORD last_error;

        last_error = GetLastError();
        if (last_error == ERROR_IO_PENDING)
        {
            LOG_TRACE(conn->proxy->logger, (_T("Continuing sending message to pipe client asynchronously")));

            conn->data.pipe.write_is_overlapped = TRUE;
            return TRUE;
        }
        if (last_error == ERROR_NO_DATA || last_error == ERROR_BROKEN_PIPE)
            LOG_ERROR(conn->proxy->logger, (_T("Could not send data to pipe: Pipe disconnected")));
        else
            LOG_ERROR(conn->proxy->logger, (_T("Sending message to pipe failed with error %d"), last_error));
        return FALSE;
    }

    LOG_TRACE(conn->proxy->logger, (_T("Sent message to pipe")));

    conn->data.pipe.write_is_overlapped = FALSE;
    return TRUE;
}

BOOL socket_handler(connection_data* const conn)
{
    char* buffer;
    size_t buffer_size;
    BOOL ret;

    LOG_TRACE(conn->proxy->logger, (_T("Entering socket handler loop")));

    buffer = 0;
    buffer_size = 0;
    ret = FALSE;
    while (TRUE)
    {
        size_t message_length;
        BOOL exit;

        if (!receive_socket_message(conn, &buffer, &buffer_size, &message_length, &exit))
            break;

        if (exit)
        {
            ret = TRUE;
            break;
        }

        if (LOG_IS_ENABLED(conn->proxy->logger, LOG_LEVEL_DEBUG))
        {
            LOG_DEBUG(conn->proxy->logger, (_T("Passing %lu bytes from socket to pipe"), message_length));
            dbg_output_bytes(conn->proxy->logger, _T("Message from socket: "), buffer, message_length);
        }

        if (!send_pipe_message(conn, buffer, message_length))
            break;
    }

    if (buffer)
        HeapFree(GetProcessHeap(), 0, buffer);
    conn->closing.socket = TRUE;
    close_pipe(conn, TRUE);
    close_socket(conn, FALSE);

    LOG_TRACE(conn->proxy->logger, (_T("Exited socket handler loop")));

    return ret;
}
