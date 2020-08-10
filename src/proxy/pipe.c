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
#include "wait_thread.h"
#include <winestreamproxy/logger.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

#define BUFSIZE 4096

BOOL create_pipe(logger_instance* const logger, TCHAR const* const path, HANDLE* const pipe)
{
    LOG_TRACE(logger, (_T("Creating named pipe %s"), path));

    *pipe = CreateNamedPipe(path, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                            PIPE_UNLIMITED_INSTANCES, BUFSIZE, BUFSIZE, 0, NULL);
    if (*pipe == INVALID_HANDLE_VALUE)
    {
        LOG_CRITICAL(logger, (_T("Could not create named pipe %s: Error %d"), path, GetLastError()));
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Created named pipe")));
    return TRUE;
}

BOOL start_connecting_pipe(logger_instance* const logger, HANDLE const pipe,
                           BOOL* const is_async, LPOVERLAPPED const overlapped)
{
    DWORD last_error;

    LOG_TRACE(logger, (_T("Starting asynchronous wait for pipe client connection")));

    if (ConnectNamedPipe(pipe, overlapped) || (last_error = GetLastError()) == ERROR_PIPE_CONNECTED)
    {
        *is_async = FALSE;
        LOG_TRACE(logger, (_T("Pipe connection finished synchronously")));
        return TRUE;
    }
    else if (last_error == ERROR_IO_PENDING)
    {
        *is_async = TRUE;
        LOG_TRACE(logger, (_T("Continuing waiting for pipe connection asynchronously")));
        return TRUE;
    }

    LOG_CRITICAL(logger, (_T("Error %d while waiting for connection"), last_error));
    return FALSE;
}

BOOL wait_for_pipe_connection(logger_instance* const logger, HANDLE const exit_event, LPOVERLAPPED const overlapped)
{
    HANDLE wait_handles[2];
    DWORD wait_result;

    LOG_TRACE(logger, (_T("Waiting for asynchronous pipe connection to finish")));

    wait_handles[0] = overlapped->hEvent;
    wait_handles[1] = exit_event;

    do {
        wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
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

BOOL prepare_pipe_data(logger_instance* const logger, HANDLE const pipe, pipe_data* const pipe_data)
{
    LOG_TRACE(logger, (_T("Preparing pipe data")));

    pipe_data->handle = pipe;
    pipe_data->write_is_overlapped = FALSE;

    pipe_data->thread_exit_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!pipe_data->thread_exit_event)
    {
        LOG_CRITICAL(logger, (_T("Could not create a pipe thread exit event: Error %d"), GetLastError()));
        return FALSE;
    }

    pipe_data->read_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!pipe_data->read_overlapped.hEvent)
    {
        LOG_CRITICAL(logger, (_T("Could not create a pipe read overlapped event: Error %d"), GetLastError()));
        CloseHandle(pipe_data->thread_exit_event);
        return FALSE;
    }

    pipe_data->write_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!pipe_data->write_overlapped.hEvent)
    {
        LOG_CRITICAL(logger, (_T("Could not create a pipe write overlapped event: Error %d"), GetLastError()));
        CloseHandle(pipe_data->write_overlapped.hEvent);
        CloseHandle(pipe_data->thread_exit_event);
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Prepared pipe data")));
    return TRUE;
}

void discard_prepared_pipe_data(logger_instance* const logger, pipe_data* const pipe_data)
{
    LOG_TRACE(logger, (_T("Discarding prepared pipe data")));

    CloseHandle(pipe_data->read_overlapped.hEvent);
    CloseHandle(pipe_data->write_overlapped.hEvent);
    CloseHandle(pipe_data->thread_exit_event);

    LOG_TRACE(logger, (_T("Discarded prepared pipe data")));
}

void close_pipe(connection_data* const conn, BOOL const exit_thread)
{
    if (conn->closing.pipe || conn->closed.pipe)
        return;

    LOG_TRACE(conn->proxy->logger, (_T("Closing pipe")));

    if (exit_thread)
    {
        if (SetEvent(conn->data.pipe.thread_exit_event))
            wait_for_thread(conn->proxy->logger, _T("pipe"), conn->threads.socket);
        else
            LOG_ERROR(conn->proxy->logger, (_T("Could not send exit signal to pipe thread")));
    }

    CloseHandle(conn->data.pipe.read_overlapped.hEvent);
    CloseHandle(conn->data.pipe.write_overlapped.hEvent);
    CloseHandle(conn->data.pipe.thread_exit_event);
    CloseHandle(conn->data.pipe.handle);

    conn->closed.pipe = TRUE;

    LOG_TRACE(conn->proxy->logger, (_T("Closed pipe")));
}
