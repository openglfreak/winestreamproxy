/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "data/connection_data.h"
#include "pipe.h"
#include <winestreamproxy/logger.h>

#include <stddef.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>

BOOL pipe_create_server(logger_instance* const logger, pipe_data* const pipe, TCHAR const* const pipe_path)
{
    (void)logger;
    (void)pipe;
    (void)pipe_path;

    return TRUE;
}

BOOL pipe_server_start_accept(logger_instance* const logger, pipe_data* const pipe, BOOL* const out_is_async,
                              OVERLAPPED* const inout_accept_overlapped)
{
    (void)logger;
    (void)pipe;
    (void)inout_accept_overlapped;

    *out_is_async = TRUE;
    return TRUE;
}

BOOL pipe_prepare(logger_instance* const logger, pipe_data* const pipe)
{
    (void)logger;
    (void)pipe;

    return TRUE;
}

BOOL pipe_discard_prepared(logger_instance* const logger, pipe_data* const pipe)
{
    (void)logger;
    (void)pipe;

    return TRUE;
}

BOOL pipe_server_wait_accept(logger_instance* const logger, pipe_data* const pipe, HANDLE const exit_event,
                             OVERLAPPED* const inout_accept_overlapped)
{
    (void)logger;
    (void)pipe;
    (void)exit_event;
    (void)inout_accept_overlapped;

    return FALSE;
}

BOOL pipe_close_server(logger_instance* const logger, pipe_data* const pipe)
{
    (void)logger;
    (void)pipe;

    return TRUE;
}

BOOL pipe_stop_thread(logger_instance* const logger, pipe_data* const pipe)
{
    (void)logger;
    (void)pipe;

    return TRUE;
}

BOOL pipe_handler(connection_data* const conn)
{
    (void)conn;

    return TRUE;
}

BOOL pipe_send_message(logger_instance* const logger, pipe_data* const pipe, unsigned char const* const message,
                       size_t const message_length)
{
    (void)logger;
    (void)pipe;
    (void)message;
    (void)message_length;

    return TRUE;
}
