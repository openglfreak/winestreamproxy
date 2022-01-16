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
#ifndef __WINESTREAMPROXY_PROXY_DATA_PIPE_DATA_H__
#define __WINESTREAMPROXY_PROXY_DATA_PIPE_DATA_H__

#include "thread_data.h"
#include "../../bool.h"

#include <windef.h>
#include <winbase.h>

typedef struct pipe_data {
    HANDLE      handle;
    OVERLAPPED  read_overlapped;
    bool        read_is_overlapped;
    OVERLAPPED  write_overlapped;
    bool        write_is_overlapped;
    thread_data thread;
} pipe_data;

#endif /* !defined(__WINESTREAMPROXY_PROXY_DATA_PIPE_DATA_H__) */
