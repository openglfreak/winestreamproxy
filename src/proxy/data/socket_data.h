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
#ifndef __WINESTREAMPROXY_PROXY_DATA_SOCKET_DATA_H__
#define __WINESTREAMPROXY_PROXY_DATA_SOCKET_DATA_H__

#include "thread_data.h"
#include "../../proxy_unixlib/socket.h"

#include <windef.h>
#include <winbase.h>

#ifdef __linux__
#define socket_use_eventfd
#endif

typedef struct socket_data {
    void*               address;
    int                 fd;
    thread_exit_signal  signal;
    thread_data         thread;
} socket_data;

#endif /* !defined(__WINESTREAMPROXY_PROXY_DATA_SOCKET_DATA_H__) */
