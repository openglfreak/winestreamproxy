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
#include "../bool.h"
#include <winestreamproxy/logger.h>

#include <windef.h>
#include <winbase.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

extern bool pipe_create_server(logger_instance* logger, pipe_data* pipe, TCHAR const* pipe_path);
extern bool pipe_server_start_accept(logger_instance* logger, pipe_data* pipe, bool* out_is_async,
                                     OVERLAPPED* inout_accept_overlapped);
extern bool pipe_prepare(logger_instance* logger, pipe_data* pipe_data);
extern bool pipe_server_wait_accept(logger_instance* logger, pipe_data* pipe, HANDLE exit_event,
                                    OVERLAPPED* inout_accept_overlapped);
extern bool pipe_close_server(logger_instance* logger, pipe_data* pipe);
extern void pipe_cleanup(logger_instance* logger, connection_data* conn);

extern bool pipe_stop_thread(logger_instance* logger, pipe_data* pipe); /* Only called if thread is running. */
extern bool pipe_handler(logger_instance* logger, connection_data* conn);

extern bool pipe_send_message(logger_instance* logger, pipe_data* pipe, unsigned char const* message,
                              size_t message_length);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_PIPE_H__) */
