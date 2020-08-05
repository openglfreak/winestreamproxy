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
#ifndef __WINESTREAMPROXY_PROXY_DATA_THREAD_DATA_H__
#define __WINESTREAMPROXY_PROXY_DATA_THREAD_DATA_H__

#include <winestreamproxy/logger.h>

#include <windef.h>
#include <winbase.h>

typedef enum THREAD_STATUS {
    THREAD_STATUS_PREPARED = 1,
    THREAD_STATUS_STARTING,
    THREAD_STATUS_RUNNING,
    THREAD_STATUS_STOPPING,
    THREAD_STATUS_STOPPED
} THREAD_STATUS;

typedef struct thread_data thread_data;

typedef BOOL (*thread_proc_func)(logger_instance* logger, void* param);
typedef void (*thread_cleanup_func)(logger_instance* logger, thread_data* data, void* thread_proc_param);
typedef BOOL (*thread_stop_func)(logger_instance* logger, thread_data* data);

typedef struct thread_description {
    thread_proc_func    thread_proc;    /* The function to execute in the thread. */
    thread_cleanup_func cleanup_func;   /* Called while stopping the thread. */
    thread_stop_func    stop_func;      /* Called to stop a thread in running state. */
} thread_description;

struct thread_data {
    HANDLE          handle;         /* The thread handle. */
    LONG volatile   status;         /* Status of the thread, values from THREAD_STATUS. */
    HANDLE          trigger_event;  /* Used to communicate with the thread. Can be used arbitrarily */
                                    /* while the thread is running, e.g. to signal the thread to stop.*/
};

#endif /* !defined(__WINESTREAMPROXY_PROXY_DATA_THREAD_DATA_H__) */
