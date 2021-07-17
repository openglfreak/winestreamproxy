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
#include "misc.h"
#include "pipe.h"
#include "socket.h"
#include "thread.h"
#include "../proxy_unixlib/socket.h"
#include <winestreamproxy/logger.h>

#include <assert.h>
#include <errno.h>
#include <stddef.h>

#include <tchar.h>
#include <unistd.h>
#include <windef.h>
#include <winnt.h>

#define InterlockedRead(x) InterlockedCompareExchange((x), 0, 0)

static HMODULE unixlib_mod = NULL;
static socket_unix_funcs unixlib_funcs;
static INIT_ONCE unixlib_initonce = INIT_ONCE_STATIC_INIT;
#ifdef NDEBUG
#define SOCKET_UNIXLIB_NAME "winestreamproxy_unixlib.dll.so"
#else
#define SOCKET_UNIXLIB_NAME "winestreamproxy_unixlib-debug.dll.so"
#endif

BOOL CALLBACK socket_init_unixlib_once(PINIT_ONCE const init_once, PVOID const param, PVOID* const ctx)
{
    socket_unix_init_t p_socket_unix_init;

    (void)init_once;
    (void)param;
    (void)ctx;

    unixlib_mod = LoadLibrary(_T(SOCKET_UNIXLIB_NAME));
    if (!unixlib_mod)
    {
        return FALSE;
    }

    p_socket_unix_init = (socket_unix_init_t)(ULONG_PTR)GetProcAddress(unixlib_mod, "socket_unix_init");
    if (!p_socket_unix_init || p_socket_unix_init(&unixlib_funcs))
    {
        FreeLibrary(unixlib_mod);
        return FALSE;
    }

    return TRUE;
}

BOOL socket_init_unixlib(void)
{
    return !!InitOnceExecuteOnce(&unixlib_initonce, socket_init_unixlib_once, 0, 0);
}

BOOL socket_prepare(logger_instance* const logger, char const* const unix_socket_path, socket_data* const _socket)
{
    size_t socket_path_len;
    int error;

    LOG_TRACE(logger, (_T("Preparing socket")));

    socket_path_len = strlen(unix_socket_path);
    if (socket_path_len > unixlib_funcs.get_max_path_length())
    {
        LOG_CRITICAL(logger, (_T("Socket path too long")));
        return FALSE;
    }

    _socket->address = HeapAlloc(GetProcessHeap(), 0, unixlib_funcs.get_address_struct_size());
    if (!_socket->address)
    {
        LOG_CRITICAL(logger, (_T("Failed to allocate %lu bytes"), (unsigned long)unixlib_funcs.get_address_struct_size()));
        return FALSE;
    }
    unixlib_funcs.init_address(_socket->address, unix_socket_path, socket_path_len);

    error = unixlib_funcs.create_thread_exit_event(&_socket->event);
    if (error)
    {
        LOG_CRITICAL(logger, (_T("Failed to create thread exit event: Error %d"), error));
        HeapFree(GetProcessHeap(), 0, _socket->address);
        return FALSE;
    }

    error = unixlib_funcs.create(&_socket->fd);
    if (error)
    {
        LOG_CRITICAL(logger, (_T("Failed to create socket: Error %d"), error));
        unixlib_funcs.close_thread_exit_event(_socket->event);
        HeapFree(GetProcessHeap(), 0, _socket->address);
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Prepared socket")));

    return TRUE;
}

BOOL socket_connect(logger_instance* const logger, socket_data* const socket)
{
    int error;

    LOG_TRACE(logger, (_T("Connecting socket")));

    error = unixlib_funcs.connect(socket->fd, socket->address);
    if (error)
    {
        LOG_ERROR(logger, (_T("Failed to connect to socket: Error %d"), error));
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Connected socket")));

    return TRUE;
}

BOOL socket_disconnect(logger_instance* const logger, socket_data* const socket)
{
    LOG_TRACE(logger, (_T("Closing socket")));

    unixlib_funcs.close(socket->fd);
    unixlib_funcs.close_thread_exit_event(socket->event);
    HeapFree(GetProcessHeap(), 0, socket->address);

    LOG_TRACE(logger, (_T("Closed socket")));

    return TRUE;
}

void socket_cleanup(logger_instance* const logger, connection_data* const conn)
{
    LOG_TRACE(logger, (_T("Cleaning up after socket thread")));

    if (InterlockedIncrement(&conn->do_cleanup) >= 2)
    {
        LOG_DEBUG(logger, (_T("Pipe thread exited, closing socket")));
        pipe_close_server(logger, &conn->pipe);
        socket_disconnect(logger, &conn->socket);
        connection_list_deallocate_entry(logger, &conn->proxy->conn_list, conn);
    }
    else
    {
        LOG_DEBUG(logger, (_T("Pipe thread is still running, not closing socket")));
        thread_dispose(logger, &pipe_thread_description, &conn->pipe.thread);
    }

    LOG_TRACE(logger, (_T("Cleaned up after socket thread")));
}

BOOL socket_stop_thread(logger_instance* const logger, socket_data* const socket)
{
    BOOL ret = TRUE;

    LOG_TRACE(logger, (_T("Stopping socket thread")));

    if (unixlib_funcs.send_thread_exit_event(socket->event))
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
    poll_status pstatus;
    int error;

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

    error = unixlib_funcs.poll(socket->fd, socket->event, &pstatus);
    if (error)
    {
        LOG_ERROR(logger, (_T("Call to poll() failed: Error %d"), error));
        return RECV_MSG_RET_FAILURE;
    }

    switch (pstatus)
    {
        case POLL_STATUS_SUCCESS:
            break;
        case POLL_STATUS_EXIT_SIGNALED:
            LOG_DEBUG(logger, (_T("Socket thread: Received exit event")));
            return RECV_MSG_RET_EXIT;
        case POLL_STATUS_CLOSED_CONNECTION:
            LOG_INFO(logger, (_T("Server closed connection")));
            return RECV_MSG_RET_SHUTDOWN;
        case POLL_STATUS_INVALID_MESSAGE_FLAGS:
            LOG_ERROR(logger, (_T("Unhandled flags returned from poll")));
            return RECV_MSG_RET_FAILURE;
    }

    while (TRUE)
    {
        size_t msg_len;
        recv_status rstatus;
        size_t new_buffer_size;
        unsigned char* new_buffer;

        LOG_TRACE(logger, (_T("Reading message from socket")));

        error = unixlib_funcs.recv(socket->fd, *inout_buffer, *inout_buffer_size, &rstatus, &msg_len);
        if (error)
        {
            LOG_ERROR(logger, (_T("Reading from socket failed: Error %d"), error));
            return RECV_MSG_RET_FAILURE;
        }

        if (rstatus != RECV_STATUS_INSUFFICIENT_BUFFER)
        {
            if (rstatus == RECV_STATUS_DISCARDED_DATA)
                LOG_ERROR(logger, (_T("Discarded socket data")));
            else
                assert(rstatus == RECV_STATUS_SUCCESS);
            *out_message_length = msg_len;
            break;
        }

        new_buffer_size = msg_len + 1; /* message length + 1, to be safe. */
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

    LOG_TRACE(logger, (_T("Exited socket handler loop")));

    return ret;
}

BOOL socket_send_message(logger_instance* const logger, socket_data* const socket, unsigned char const* const message,
                         size_t const message_length)
{
    size_t bytes_written;
    int error;

    LOG_TRACE(logger, (_T("Sending message to socket")));

    if (InterlockedRead(&socket->thread.status) >= THREAD_STATUS_STOPPING)
    {
        LOG_ERROR(logger, (_T("Can't send message to closed socket")));
        return FALSE;
    }

    error = unixlib_funcs.send(socket->fd, message, message_length, &bytes_written);
    if (error)
    {
        LOG_ERROR(logger, (_T("Error %d while writing to socket"), error));
        return FALSE;
    }
    else if (bytes_written != message_length)
    {
        LOG_ERROR(logger, (
            bytes_written < message_length
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
