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
#include "socket.h"
#include <winestreamproxy/logger.h>

#include <stddef.h>

#include <windef.h>
#include <winnt.h>

BOOL socket_prepare(logger_instance* const logger, char const* const unix_socket_path, socket_data* const socket)
{
    (void)logger;
    (void)unix_socket_path;
    (void)socket;

    return TRUE;
}

BOOL socket_connect(logger_instance* const logger, socket_data* const socket)
{
    (void)logger;
    (void)socket;

    return TRUE;
}

BOOL socket_disconnect(logger_instance* const logger, socket_data* const socket)
{
    (void)logger;
    (void)socket;

    return TRUE;
}

BOOL socket_discard_prepared(logger_instance* const logger, socket_data* const socket)
{
    (void)logger;
    (void)socket;

    return TRUE;
}

void socket_cleanup(logger_instance* const logger, connection_data* const conn)
{
    (void)logger;
    (void)conn;
}

BOOL socket_stop_thread(logger_instance* const logger, socket_data* const socket)
{
    (void)logger;
    (void)socket;

    return TRUE;
}

BOOL socket_handler(logger_instance* const logger, connection_data* const conn)
{
    (void)logger;
    (void)conn;

    return TRUE;
}

BOOL socket_send_message(logger_instance* const logger, socket_data* const socket, unsigned char const* const message,
                         size_t const message_length)
{
    (void)logger;
    (void)socket;
    (void)message;
    (void)message_length;

    return TRUE;
}
