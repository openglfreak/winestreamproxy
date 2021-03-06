/* Copyright (C) 2020 Torge Matthies
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

#include "connection.h"
#include <winestreamproxy/logger.h>

#include <windef.h>
#include <sys/un.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef struct socket_data {
    struct sockaddr_un addr;
    int eventfd;
    int socketfd;
} socket_data;

struct connection_data;

extern BOOL socket_handler(struct connection_data* conn);

extern BOOL prepare_socket(logger_instance* logger, char const* socket_path, socket_data* socket);

extern void discard_prepared_socket(logger_instance* logger, socket_data* socket);

extern BOOL connect_socket(logger_instance* logger, socket_data* socket);

extern void close_socket(struct connection_data* conn, BOOL exit_thread);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_SOCKET_H__) */
