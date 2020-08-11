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
#ifndef __WINESTREAMPROXY_PROXY_DATA_PROXY_DATA_H__
#define __WINESTREAMPROXY_PROXY_DATA_PROXY_DATA_H__

#include "connection_list.h"
#include <winestreamproxy/logger.h>
#include <winestreamproxy/winestreamproxy.h>

#include <windef.h>
#include <winbase.h>

/* Already typedef'd in winestreamproxy.h. */
struct proxy_data {
    logger_instance*    logger;
    proxy_parameters    parameters;
    LONG volatile       is_running;
    connection_list     conn_list;
    OVERLAPPED          accept_overlapped;
};

#endif /* !defined(__WINESTREAMPROXY_PROXY_DATA_PROXY_DATA_H__) */
