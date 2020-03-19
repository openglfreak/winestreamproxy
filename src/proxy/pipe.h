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

#include <winestreamproxy/logger.h>

#include <windef.h>
#include <winbase.h>
#include <winnt.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef struct pipe_data {
    HANDLE handle;
    HANDLE thread_exit_event;
    OVERLAPPED read_overlapped;
    BOOL read_is_overlapped;
    OVERLAPPED write_overlapped;
    BOOL write_is_overlapped;
} pipe_data;

struct connection_data;

extern BOOL pipe_handler(struct connection_data* conn);

extern BOOL create_pipe(logger_instance* logger, TCHAR const* path, HANDLE* pipe);

extern BOOL start_connecting_pipe(logger_instance* logger, HANDLE pipe, BOOL* is_async, LPOVERLAPPED overlapped);

extern BOOL wait_for_pipe_connection(logger_instance* logger, HANDLE exit_event, LPOVERLAPPED overlapped);

extern BOOL prepare_pipe_data(logger_instance* logger, HANDLE pipe, pipe_data* pipe_data);

extern void discard_prepared_pipe_data(logger_instance* logger, pipe_data* pipe_data);

extern void close_pipe(struct connection_data* conn, BOOL exit_thread);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_PIPE_H__) */
