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

#include <stddef.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

#define InterlockedRead(x) InterlockedCompareExchange((x), 0, 0)

#define STARTING_BUFFER_SIZE 1024

BOOL pipe_create_server(logger_instance* const logger, pipe_data* const pipe, TCHAR const* const pipe_path)
{
    LOG_TRACE(logger, (_T("Creating named pipe server %s"), pipe_path));

    pipe->handle = CreateNamedPipe(pipe_path, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                   PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                   PIPE_UNLIMITED_INSTANCES, STARTING_BUFFER_SIZE, STARTING_BUFFER_SIZE, 0, NULL);
    if (pipe->handle == INVALID_HANDLE_VALUE)
    {
        LOG_CRITICAL(logger, (_T("Could not create named pipe %s: Error %d"), pipe_path, GetLastError()));
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Created named pipe server")));

    return TRUE;
}

BOOL pipe_server_start_accept(logger_instance* const logger, pipe_data* const pipe, BOOL* const out_is_async,
                              OVERLAPPED* const inout_accept_overlapped)
{
    DWORD last_error;

    LOG_TRACE(logger, (_T("Starting asynchronous wait for pipe client connection")));

    if (ConnectNamedPipe(pipe->handle, inout_accept_overlapped) != 0 ||
        (last_error = GetLastError()) == ERROR_PIPE_CONNECTED)
    {
        *out_is_async = FALSE;
        LOG_TRACE(logger, (_T("Pipe connection finished synchronously")));
        return TRUE;
    }
    else if (last_error == ERROR_IO_PENDING)
    {
        *out_is_async = TRUE;
        LOG_TRACE(logger, (_T("Continuing waiting for pipe connection asynchronously")));
        return TRUE;
    }

    LOG_CRITICAL(logger, (_T("Error %d while waiting for connection"), last_error));

    return FALSE;
}

BOOL pipe_prepare(logger_instance* const logger, pipe_data* const pipe)
{
    LOG_TRACE(logger, (_T("Preparing pipe data")));

    pipe->read_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!pipe->read_overlapped.hEvent)
    {
        LOG_CRITICAL(logger, (_T("Could not create a pipe read overlapped event: Error %d"), GetLastError()));
        return FALSE;
    }

    pipe->write_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!pipe->write_overlapped.hEvent)
    {
        LOG_CRITICAL(logger, (_T("Could not create a pipe write overlapped event: Error %d"), GetLastError()));
        CloseHandle(pipe->write_overlapped.hEvent);
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Prepared pipe data")));

    return TRUE;
}

BOOL pipe_server_wait_accept(logger_instance* const logger, pipe_data* const pipe, HANDLE const exit_event,
                             OVERLAPPED* const inout_accept_overlapped)
{
    HANDLE wait_handles[2];
    DWORD wait_result;

    (void)pipe;

    LOG_TRACE(logger, (_T("Waiting for asynchronous pipe connection to finish")));

    wait_handles[0] = inout_accept_overlapped->hEvent;
    wait_handles[1] = exit_event;

    do {
        wait_result = \
            WaitForMultipleObjects(sizeof(wait_handles) / sizeof(wait_handles[0]), wait_handles, FALSE, INFINITE);
    } while (wait_result == WAIT_TIMEOUT);

    switch (wait_result)
    {
        case WAIT_OBJECT_0:
            LOG_INFO(logger, (_T("Pipe client connected")));
            return TRUE;
        case WAIT_OBJECT_0 + 1:
            LOG_TRACE(logger, (_T("Received exit signal")));
            break;
        case WAIT_FAILED:
            LOG_ERROR(logger, (_T("WaitForMultipleObjects failed: Error %d"), GetLastError()));
            break;
        default:
            LOG_ERROR(logger, (_T("Unexpected WaitForMultipleObjects return value: %d"), wait_result));
    }

    return FALSE;
}

BOOL pipe_close_server(logger_instance* const logger, pipe_data* const pipe)
{
    LOG_TRACE(logger, (_T("Closing pipe server")));

    if (pipe->read_is_overlapped)
        CancelIoEx(pipe->handle, &pipe->read_overlapped);
    CloseHandle(pipe->read_overlapped.hEvent);
    if (pipe->write_is_overlapped)
        CancelIoEx(pipe->handle, &pipe->write_overlapped);
    CloseHandle(pipe->write_overlapped.hEvent);
    FlushFileBuffers(pipe->handle);
    DisconnectNamedPipe(pipe->handle);
    CloseHandle(pipe->handle);

    LOG_TRACE(logger, (_T("Closed pipe server")));

    return TRUE;
}

void pipe_cleanup(logger_instance* const logger, connection_data* const conn)
{
    LOG_TRACE(logger, (_T("Cleaning up after pipe thread")));

    if (InterlockedIncrement(&conn->do_cleanup) >= 2)
    {
        socket_disconnect(logger, &conn->socket);
        pipe_close_server(logger, &conn->pipe);
    }
    else
    {
        LOG_DEBUG(logger, (_T("Socket thread is still running, not closing pipe")));
        thread_dispose(logger, &socket_thread_description, &conn->socket.thread);
    }

    LOG_TRACE(logger, (_T("Cleaned up after pipe thread")));
}

BOOL pipe_stop_thread(logger_instance* const logger, pipe_data* const pipe)
{
    BOOL ret = TRUE;

    LOG_TRACE(logger, (_T("Stopping pipe thread")));

    if (!SetEvent(pipe->thread.trigger_event))
        ret = FALSE;
    if (!thread_wait(logger, &pipe_thread_description, &pipe->thread))
        ret = FALSE;

    LOG_TRACE(logger, (_T("Stopped pipe thread")));

    return ret;
}

typedef enum RECV_MSG_RET {
    RECV_MSG_RET_SUCCESS,
    RECV_MSG_RET_FAILURE,
    RECV_MSG_RET_SHUTDOWN,
    RECV_MSG_RET_EXIT
} RECV_MSG_RET;

static RECV_MSG_RET pipe_receive_message(logger_instance* const logger, pipe_data* const pipe,
                                         unsigned char** const inout_buffer, size_t* const inout_buffer_size,
                                         size_t* const out_message_length)
{
    HANDLE wait_handles[2];
    size_t message_length;
    DWORD bytes_read;

    LOG_TRACE(logger, (_T("Waiting for and reading message from pipe")));

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

    wait_handles[0] = pipe->read_overlapped.hEvent;
    wait_handles[1] = pipe->thread.trigger_event;

    message_length = 0;
    pipe->read_is_overlapped = FALSE;

    while (TRUE)
    {
        BOOL success;
        DWORD last_error;

        bytes_read = 0;
        if (pipe->read_is_overlapped)
        {
            success = GetOverlappedResult(pipe->handle, &pipe->read_overlapped, &bytes_read, FALSE);
            pipe->read_is_overlapped = FALSE;
        }
        else
        {
            success = ReadFile(pipe->handle, &(*inout_buffer)[message_length], *inout_buffer_size - message_length,
                               &bytes_read, &pipe->read_overlapped);
        }
        message_length += bytes_read;
        if (success)
        {
            *out_message_length = message_length;
            break;
        }

        switch (last_error = GetLastError())
        {
            case ERROR_BROKEN_PIPE:
                LOG_INFO(logger, (_T("Pipe server closed connection")));
                return RECV_MSG_RET_SHUTDOWN;
            case ERROR_IO_PENDING:
            {
                DWORD wait_result;

                pipe->read_is_overlapped = TRUE;

                do {
                    wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
                } while (wait_result == WAIT_TIMEOUT);

                switch (wait_result)
                {
                    case WAIT_OBJECT_0:
                        continue;
                    case WAIT_OBJECT_0 + 1:
                        LOG_DEBUG(logger, (_T("Pipe thread: Received exit event")));
                        CancelIoEx(pipe->handle, &pipe->read_overlapped);
                        return RECV_MSG_RET_EXIT;
                    case WAIT_FAILED:
                        LOG_ERROR(logger, (_T("Error %d while waiting for pipe handles"), GetLastError()));
                        return RECV_MSG_RET_FAILURE;
                    default:
                        LOG_ERROR(logger, (_T("Unknown return value from WaitForMultipleObjects")));
                        return RECV_MSG_RET_FAILURE;
                }
            }
            case ERROR_MORE_DATA:
            {
                DWORD remaining;
                size_t new_buffer_size;
                unsigned char* new_buffer;

                success = PeekNamedPipe(pipe->handle, NULL, 0, NULL, NULL, &remaining);
                if (!success)
                {
                    LOG_ERROR(logger, (_T("Error %d while getting remaining message length"), GetLastError()));
                    return RECV_MSG_RET_FAILURE;
                }

                new_buffer_size = message_length + remaining;
                new_buffer = (unsigned char*)HeapReAlloc(GetProcessHeap(), 0, *inout_buffer, new_buffer_size);
                if (!new_buffer)
                {
                    LOG_ERROR(logger, (
                        _T("Failed to resize incoming pipe data buffer from %lu to %lu bytes"),
                        (unsigned long)*inout_buffer_size,
                        (unsigned long)new_buffer_size
                    ));
                    return RECV_MSG_RET_FAILURE;
                }
                *inout_buffer = new_buffer;
                *inout_buffer_size = new_buffer_size;
                continue;
            }
            default:
                LOG_ERROR(logger, (_T("Reading from pipe failed: Error %d"), last_error));
                return RECV_MSG_RET_FAILURE;
        }
    }

    LOG_TRACE(logger, (_T("Read message from pipe")));

    return RECV_MSG_RET_SUCCESS;
}

BOOL pipe_handler(logger_instance* const logger, connection_data* const conn)
{
    unsigned char* buffer;
    size_t buffer_size;
    BOOL ret;

    LOG_TRACE(logger, (_T("Entering pipe handler loop")));

    buffer = 0;
    buffer_size = 0;
    while (TRUE)
    {
        size_t message_length;
        RECV_MSG_RET recv_ret;

        recv_ret = pipe_receive_message(logger, &conn->pipe, &buffer, &buffer_size, &message_length);
        if (recv_ret != RECV_MSG_RET_SUCCESS)
        {
            ret = recv_ret != RECV_MSG_RET_FAILURE;
            break;
        }

        if (LOG_IS_ENABLED(logger, LOG_LEVEL_DEBUG))
        {
            LOG_DEBUG(logger, (_T("Passing %lu bytes from pipe to socket"), message_length));
            dbg_output_bytes(logger, _T("Message from pipe: "), buffer, message_length);
        }

        if (!socket_send_message(logger, &conn->socket, buffer, message_length))
        {
            ret = InterlockedRead(&conn->socket.thread.status) >= THREAD_STATUS_STOPPING;
            break;
        }
    }

    if (buffer)
        HeapFree(GetProcessHeap(), 0, buffer);
    connection_close(conn);

    LOG_TRACE(logger, (_T("Exited pipe handler loop")));

    return ret;
}

BOOL pipe_send_message(logger_instance* const logger, pipe_data* const pipe, unsigned char const* const message,
                       size_t const message_length)
{
    DWORD bytes_written;

    LOG_TRACE(logger, (_T("Sending message to pipe")));

    if (InterlockedRead(&pipe->thread.status) >= THREAD_STATUS_STOPPING)
    {
        LOG_ERROR(logger, (_T("Can't send message to closed pipe")));
        return FALSE;
    }

    if ((DWORD)message_length != message_length)
    {
        LOG_ERROR(logger, (
            _T("Message size too big: %lu > %lu"),
            (unsigned long)message_length,
            (unsigned long)(DWORD)-1
        ));
        return FALSE;
    }

    if (pipe->write_is_overlapped)
    {
        BOOL prev_ret;

        prev_ret = GetOverlappedResult(pipe->handle, &pipe->write_overlapped, &bytes_written, FALSE);
        pipe->write_is_overlapped = FALSE;
        if (!prev_ret)
        {
            LOG_ERROR(logger, (_T("Error in previous pipe write: Error %d"), GetLastError()));
            return FALSE;
        }
    }

    if (!WriteFile(pipe->handle, message, (DWORD)message_length, NULL, &pipe->write_overlapped))
    {
        DWORD last_error;

        last_error = GetLastError();
        if (last_error == ERROR_IO_PENDING)
        {
            LOG_TRACE(logger, (_T("Continuing sending message to pipe client asynchronously")));

            pipe->write_is_overlapped = TRUE;
            return TRUE;
        }
        else if (last_error == ERROR_NO_DATA || last_error == ERROR_BROKEN_PIPE)
            LOG_ERROR(logger, (_T("Could not send data to pipe: Pipe disconnected")));
        else
            LOG_ERROR(logger, (_T("Sending message to pipe failed with error %d"), last_error));
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Sent message to pipe")));

    return TRUE;
}
