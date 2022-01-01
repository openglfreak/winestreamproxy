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
#ifndef __WINESTREAMPROXY_PROXY_CONNECTION_LIST_H__
#define __WINESTREAMPROXY_PROXY_CONNECTION_LIST_H__

#include "../bool.h"
#include "data/connection_list.h"
#include <winestreamproxy/logger.h>

#include <windef.h>
#include <winbase.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

#define connection_list_lock(connection_list) EnterCriticalSection(&(connection_list)->lock)
#define connection_list_start(connection_list) ((connection_list)->start)
#define connection_list_next(entry) ((entry)->next)
#define connection_list_end(connection_list) ((connection_list)->end)
#define connection_list_previous(entry) ((entry)->previous)
#define connection_list_unlock(connection_list) LeaveCriticalSection(&(connection_list)->lock)

extern bool connection_list_initialize(logger_instance* logger, connection_list* connection_list);
extern bool connection_list_allocate_entry(logger_instance* logger, connection_list* connection_list,
                                           connection_data** out_connection);
extern void connection_list_deallocate_entry(logger_instance* logger, connection_list* connection_list,
                                             connection_data* connection);
extern void connection_list_deallocate(logger_instance* logger, connection_list* connection_list);
extern void connection_list_finalize(logger_instance* logger, connection_list* connection_list);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_CONNECTION_LIST_H__) */
