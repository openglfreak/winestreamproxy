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
#ifndef __WINESTREAMPROXY_PROXY_THREAD_H__
#define __WINESTREAMPROXY_PROXY_THREAD_H__

#include "data/thread_data.h"
#include "../bool.h"
#include <winestreamproxy/logger.h>

#include <windef.h>
#include <winbase.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef enum THREAD_RUN_ERROR {
    THREAD_RUN_ERROR_SUCCESS,
    THREAD_RUN_ERROR_ALREADY_RUNNING,
    THREAD_RUN_ERROR_ALREADY_FINISHED,
    THREAD_RUN_ERROR_OTHER
} THREAD_RUN_ERROR;

extern bool thread_prepare(logger_instance* logger, thread_description* desc, thread_data* data,
                           void* thread_proc_param);
extern THREAD_RUN_ERROR thread_run(logger_instance* logger, thread_description* desc, thread_data* data);
extern bool thread_stop(logger_instance* logger, thread_description* desc, thread_data* data);
#define thread_dispose thread_stop
extern bool thread_wait(logger_instance* logger, thread_description* desc, thread_data* data);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_THREAD_H__) */
