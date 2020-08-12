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
#ifndef __WINESTREAMPROXY_PROXY_DATA_CONNECTION_DATA_H__
#define __WINESTREAMPROXY_PROXY_DATA_CONNECTION_DATA_H__

#include "pipe_data.h"
#include "socket_data.h"

#include <windef.h>

typedef struct connection_data {
    struct proxy_data* proxy;

    pipe_data   pipe;
    socket_data socket;

    /* Used internally by the pipe and socket threads to make sure that
       clean-up is only done once. */
    LONG volatile   do_cleanup;
} connection_data;

#endif /* !defined(__WINESTREAMPROXY_PROXY_DATA_CONNECTION_DATA_H__) */
