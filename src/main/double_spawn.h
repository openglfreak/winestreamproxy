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

#include <winestreamproxy/logger.h>

#include <windef.h>
#include <winbase.h>
#include <winnt.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef struct double_spawn_data {
    HANDLE event;
} double_spawn_data;

typedef enum DOUBLE_SPAWN_RETURN {
    DOUBLE_SPAWN_RETURN_EXIT,
    DOUBLE_SPAWN_RETURN_CONTINUE,
    DOUBLE_SPAWN_RETURN_ERROR
} DOUBLE_SPAWN_RETURN;

extern DOUBLE_SPAWN_RETURN double_spawn_execute(logger_instance* logger, double_spawn_data* data);
extern void double_spawn_finish(logger_instance* logger, double_spawn_data* data);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_MAIN_DOUBLE_SPAWN_H__) */
