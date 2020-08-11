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
#include <winbase.h>
#include <winnt.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef struct proxy_paths {
    TCHAR const*    named_pipe_path;
    char const*     unix_socket_path;
} proxy_paths;

extern TCHAR* pipe_name_to_path(logger_instance* logger, TCHAR const* named_pipe_name);
extern void deallocate_path(TCHAR const* named_pipe_path);

typedef enum PROXY_STATE {
    PROXY_STATE_CREATED,
    PROXY_STATE_STARTING,
    PROXY_STATE_RUNNING,
    PROXY_STATE_STOPPING,
    PROXY_STATE_STOPPED
} PROXY_STATE;

typedef struct proxy_data proxy_data;

typedef void (*proxy_state_change_callback)(logger_instance* logger, proxy_data* proxy, PROXY_STATE prev_state,
                                            PROXY_STATE new_state);

typedef struct proxy_parameters {
    proxy_paths                 paths;
    HANDLE                      exit_event; /* Must be manual-reset. */
    proxy_state_change_callback state_change_callback;
} proxy_parameters;

extern BOOL proxy_create(logger_instance* logger, proxy_parameters parameters, proxy_data** out_proxy);
extern void proxy_enter_loop(proxy_data* proxy);
extern void proxy_destroy(proxy_data* proxy);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_WINESTREAMPROXY_H__) */
