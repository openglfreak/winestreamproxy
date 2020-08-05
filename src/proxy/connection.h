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
#ifndef __WINESTREAMPROXY_PROXY_CONNECTION_H__
#define __WINESTREAMPROXY_PROXY_CONNECTION_H__

#include "data/connection_data.h"
#include "data/proxy_data.h"
#include <winestreamproxy/logger.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

extern thread_description pipe_thread_description;
extern thread_description socket_thread_description;

extern void connection_initialize(proxy_data* proxy, connection_data* conn);
extern BOOL connection_prepare_threads(connection_data* conn);
extern BOOL connection_launch_threads(connection_data* conn);
extern void connection_close(connection_data* conn);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_CONNECTION_H__) */
