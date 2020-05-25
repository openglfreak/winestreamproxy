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
#ifndef __WINESTREAMPROXY_WINESTREAMPROXY_H__
#define __WINESTREAMPROXY_WINESTREAMPROXY_H__

#include "logger.h"

#include <windef.h>
#include <winnt.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef struct connection_paths {
    TCHAR const* named_pipe_path;
    char const* unix_socket_path;
} connection_paths;

extern TCHAR* pipe_name_to_path(logger_instance* logger, TCHAR const* named_pipe_name);
extern void deallocate_path(TCHAR const* named_pipe_path);

typedef struct proxy_data proxy_data;
typedef void (*proxy_running_callback)(proxy_data* proxy);

extern BOOL create_proxy(logger_instance* logger, connection_paths paths, HANDLE exit_event,
                         proxy_running_callback running_callback, proxy_data** out_proxy);
extern void enter_proxy_loop(proxy_data* proxy);
extern void destroy_proxy(proxy_data* proxy);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_WINESTREAMPROXY_H__) */
