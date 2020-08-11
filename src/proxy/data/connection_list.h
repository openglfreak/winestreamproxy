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
#ifndef __WINESTREAMPROXY_PROXY_DATA_CONNECTION_LIST_H__
#define __WINESTREAMPROXY_PROXY_DATA_CONNECTION_LIST_H__

#include "connection_data.h"

typedef struct connection_list_entry connection_list_entry;
struct connection_list_entry {
    connection_list_entry*  previous;
    connection_list_entry*  next;
    struct connection_data  connection;
};

typedef struct connection_list {
    connection_list_entry*  start;
    connection_list_entry*  end;
} connection_list;

#endif /* !defined(__WINESTREAMPROXY_PROXY_DATA_CONNECTION_LIST_H__) */
