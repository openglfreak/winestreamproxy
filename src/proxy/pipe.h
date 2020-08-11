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
#ifndef __WINESTREAMPROXY_PROXY_PIPE_H__
#define __WINESTREAMPROXY_PROXY_PIPE_H__

#include "data/connection_data.h"
#include "data/pipe_data.h"
#include <winestreamproxy/logger.h>

#include <windef.h>
#include <winbase.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

extern BOOL pipe_create_server(logger_instance* logger, pipe_data* pipe, TCHAR const* pipe_path);
extern BOOL pipe_server_start_accept(logger_instance* logger, pipe_data* pipe, BOOL* out_is_async,
                                     OVERLAPPED* inout_accept_overlapped);
extern BOOL pipe_prepare(logger_instance* logger, pipe_data* pipe_data);
extern BOOL pipe_discard_prepared(logger_instance* logger, pipe_data* pipe_data);
extern BOOL pipe_server_wait_accept(logger_instance* logger, pipe_data* pipe, HANDLE exit_event,
                                    OVERLAPPED* inout_accept_overlapped);
extern BOOL pipe_close_server(logger_instance* logger, pipe_data* pipe);

extern BOOL pipe_stop_thread(logger_instance* logger, pipe_data* pipe); /* Only called if status is 1 or 2. */
extern BOOL pipe_handler(connection_data* conn);

extern BOOL pipe_send_message(logger_instance* logger, pipe_data* pipe, unsigned char const* message,
                              size_t message_length);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_PIPE_H__) */
