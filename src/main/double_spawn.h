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
#ifndef __WINESTREAMPROXY_MAIN_DOUBLE_SPAWN_H__
#define __WINESTREAMPROXY_MAIN_DOUBLE_SPAWN_H__

#include "bool.h"
#include <winestreamproxy/logger.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef int (*double_spawn_callback)(void* aux_data, size_t aux_data_size);

extern bool double_spawn_fork(logger_instance* logger, double_spawn_callback callback, void const* aux_data,
                              size_t aux_data_size);
extern void double_spawn_exit_parent(logger_instance* logger);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_MAIN_DOUBLE_SPAWN_H__) */
