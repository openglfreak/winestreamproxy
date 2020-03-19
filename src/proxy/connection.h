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

#include "pipe.h"
#include "socket.h"

#include <windef.h>
#include <winbase.h>
#include <winnt.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef struct connection_data {
    struct proxy_data* proxy;

    struct connection_data_data {
        pipe_data pipe;
        socket_data socket;
    } data;
    struct connection_data_closing {
        BOOL volatile pipe;
        BOOL volatile socket;
    } closing;
    struct connection_data_closed {
        BOOL volatile pipe;
        BOOL volatile socket;
    } closed;
    struct connection_data_start_events {
        HANDLE pipe;
        HANDLE socket;
    } start_events;
    struct connection_data_threads {
        HANDLE pipe;
        HANDLE socket;
    } threads;
    BOOL volatile start;
} connection_data;

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#include "proxy.h"

#endif /* !defined(__WINESTREAMPROXY_PROXY_CONNECTION_H__) */
