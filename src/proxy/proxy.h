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
#ifndef __WINESTREAMPROXY_PROXY_PROXY_H__
#define __WINESTREAMPROXY_PROXY_PROXY_H__

#include "connection.h"
#include <winestreamproxy/logger.h>
#include <winestreamproxy/winestreamproxy.h>

#include <limits.h>

#include <windef.h>
#include <winbase.h>
#include <winnt.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef struct connection_list_entry {
    struct connection_list_entry* previous;
    struct connection_list_entry* next;
    connection_data connection;
} connection_list_entry;

struct proxy_data {
    logger_instance* logger;
    connection_paths paths;
    OVERLAPPED connect_overlapped;
    connection_list_entry* connections_start;
    connection_list_entry* connections_end;
    HANDLE exit_event;
    char volatile running[((32 + CHAR_BIT - 1) / CHAR_BIT) - 1 + sizeof(LONG)];
};

extern BOOL allocate_connection(proxy_data* proxy, connection_data** out_connection);

extern void deallocate_connection(proxy_data* proxy, connection_data* connection);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_PROXY_H__) */
