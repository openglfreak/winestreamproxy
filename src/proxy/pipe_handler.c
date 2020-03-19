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

#include <sys/io.h>
#include <sys/types.h>
#include <unistd.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

#define BUFSIZE 4096

static BOOL receive_pipe_message(connection_data* const conn, char** const buffer,
                                 size_t* const buffer_size, size_t* const out_message_length,
                                 BOOL* const out_exit)
{
    HANDLE wait_handles[2];
    size_t message_length;
    DWORD bytes_read;
    BOOL is_async;

    LOG_TRACE(conn->proxy->logger, (TEXT("Reading message from pipe")));

    if (!*buffer)
    {
        *buffer_size = BUFSIZE;
        *buffer = (char*)HeapAlloc(GetProcessHeap(), 0, BUFSIZE * sizeof(char));

        if (!*buffer)
        {
            LOG_ERROR(conn->proxy->logger, (
                TEXT("Could not allocate %lu bytes"),
                (unsigned long)(BUFSIZE * sizeof(char))
            ));
            return FALSE;
        }
    }

    wait_handles[0] = conn->data.pipe.read_overlapped.hEvent;
    wait_handles[1] = conn->data.pipe.thread_exit_event;

    message_length = 0;
    is_async = FALSE;

    while (TRUE)
    {
        BOOL success;
        DWORD last_error;

        bytes_read = 0;
        if (is_async)
        {
            success = GetOverlappedResult(conn->data.pipe.handle, &conn->data.pipe.read_overlapped, &bytes_read, FALSE);
            is_async = FALSE;
        }
        else
            success = ReadFile(conn->data.pipe.handle, &(*buffer)[message_length], *buffer_size - message_length,
                               &bytes_read, &conn->data.pipe.read_overlapped);
        message_length += bytes_read;
        if (success)
            break;

        last_error = GetLastError();

        if (last_error == ERROR_BROKEN_PIPE)
        {
            LOG_INFO(conn->proxy->logger, (TEXT("Client disconnected")));
            *out_exit = TRUE;
            return TRUE;
        }

        else if (last_error == ERROR_IO_PENDING)
        {
            DWORD wait_result;

            is_async = TRUE;

            do {
                wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
            } while (wait_result == WAIT_TIMEOUT);

            switch (wait_result)
            {
                case WAIT_OBJECT_0:
                    break;
                case WAIT_OBJECT_0 + 1:
                    LOG_DEBUG(conn->proxy->logger, (TEXT("Pipe thread: Received exit event")));
                    CancelIoEx(conn->data.pipe.handle, &conn->data.pipe.read_overlapped);
                    *out_exit = TRUE;
                    return TRUE;
                case WAIT_FAILED:
                    LOG_ERROR(conn->proxy->logger, (TEXT("Error %d while waiting for pipe handles"), GetLastError()));
                    return FALSE;
                default:
                    LOG_ERROR(conn->proxy->logger, (TEXT("Unknown return value from WaitForMultipleObjects")));
                    return FALSE;
            }
        }

        else if (last_error == ERROR_MORE_DATA)
        {
            DWORD remaining;
            size_t new_buffer_size;
            char* new_buffer;

            success = PeekNamedPipe(conn->data.pipe.handle, NULL, 0, NULL, NULL, &remaining);
            if (!success)
            {
                LOG_ERROR(conn->proxy->logger, (
                    TEXT("Error %d while getting remaining message length"),
                    GetLastError()
                ));
                return FALSE;
            }

            new_buffer_size = message_length + remaining;
            new_buffer = (char*)HeapReAlloc(GetProcessHeap(), 0, *buffer, new_buffer_size);
            if (!new_buffer)
            {
                LOG_ERROR(conn->proxy->logger, (
                    TEXT("Failed to resize incoming pipe data buffer from %lu to %lu bytes"),
                    (unsigned long)*buffer_size,
                    (unsigned long)new_buffer_size
                ));
                return FALSE;
            }
            *buffer = new_buffer;
            *buffer_size = new_buffer_size;
        }

        else
        {
            LOG_ERROR(conn->proxy->logger, (TEXT("Error reading from pipe: Error %d"), last_error));
            return FALSE;
        }
    }

    LOG_TRACE(conn->proxy->logger, (TEXT("Read message from pipe")));

    *out_message_length = message_length;
    *out_exit = FALSE;
    return TRUE;
}

static BOOL send_socket_message(connection_data* const conn, char const* const message, size_t const message_length)
{
    ssize_t bytes_written;

    LOG_TRACE(conn->proxy->logger, (TEXT("Sending message to socket")));

    bytes_written = write(conn->data.socket.socketfd, message, message_length);
    if (bytes_written == -1)
    {
        LOG_ERROR(conn->proxy->logger, (TEXT("Error %d while writing to socket"), errno));
        return FALSE;
    }
    else if ((size_t)bytes_written != message_length)
    {
        LOG_ERROR(conn->proxy->logger, (TEXT("Partial write to socket")));
        return FALSE;
    }

    LOG_TRACE(conn->proxy->logger, (TEXT("Sent message to socket")));

    return TRUE;
}

BOOL pipe_handler(connection_data* const conn)
{
    char* buffer;
    size_t buffer_size;
    BOOL ret;

    LOG_TRACE(conn->proxy->logger, (TEXT("Entering pipe handler loop")));

    buffer = 0;
    buffer_size = 0;
    ret = FALSE;
    while (TRUE)
    {
        size_t message_length;
        BOOL exit;

        if (!receive_pipe_message(conn, &buffer, &buffer_size, &message_length, &exit))
            break;

        if (exit)
        {
            ret = TRUE;
            break;
        }

        if (LOG_IS_ENABLED(conn->proxy->logger, LOG_LEVEL_DEBUG))
        {
            LOG_DEBUG(conn->proxy->logger, (TEXT("Passing %lu bytes from pipe to socket"), message_length));
            dbg_output_bytes(conn->proxy->logger, TEXT("Message from pipe: "), buffer, message_length);
        }

        if (!send_socket_message(conn, buffer, message_length))
            break;
    }

    if (buffer)
        HeapFree(GetProcessHeap(), 0, buffer);
    conn->closing.pipe = TRUE;
    close_socket(conn, TRUE);
    close_pipe(conn, FALSE);

    LOG_TRACE(conn->proxy->logger, (TEXT("Exited pipe handler loop")));

    return ret;
}
