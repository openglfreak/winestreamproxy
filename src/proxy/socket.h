/* Copyright (C) 2020-2021 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#pragma once
#ifndef __WINESTREAMPROXY_PROXY_SOCKET_H__
#define __WINESTREAMPROXY_PROXY_SOCKET_H__

#include "data/connection_data.h"
#include "data/socket_data.h"
#include "../bool.h"
#include <winestreamproxy/logger.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

extern bool socket_init_unixlib(void);

extern bool socket_prepare(logger_instance* logger, char const* unix_socket_path, socket_data* socket);
extern bool socket_connect(logger_instance* logger, socket_data* socket);
extern bool socket_disconnect(logger_instance* logger, socket_data* socket);
extern void socket_cleanup(logger_instance* logger, connection_data* conn);

extern bool socket_stop_thread(logger_instance* logger, socket_data* socket); /* Only called if thread is running. */
extern bool socket_handler(logger_instance* logger, connection_data* conn);

extern bool socket_send_message(logger_instance* logger, socket_data* socket, unsigned char const* message,
                                size_t message_length);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_SOCKET_H__) */
