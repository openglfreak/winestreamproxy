/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "thread.h"
#include <winestreamproxy/logger.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

struct thread_proc_data {
    logger_instance*    logger;
    thread_description* desc;
    thread_data*        data;
    void*               thread_proc_param;
};

DWORD CALLBACK WINAPI dummy_thread_proc(LPVOID const voidp_tpdata)
{
    struct thread_proc_data tpdata;
    DWORD wait_result;
    DWORD ret;

    tpdata.logger = ((struct thread_proc_data*)voidp_tpdata)->logger;
    tpdata.desc = ((struct thread_proc_data*)voidp_tpdata)->desc;
    tpdata.data = ((struct thread_proc_data*)voidp_tpdata)->data;
    tpdata.thread_proc_param = ((struct thread_proc_data*)voidp_tpdata)->thread_proc_param;
    HeapFree(GetProcessHeap(), 0, voidp_tpdata);

    LOG_TRACE(tpdata.logger, (_T("Started dummy thread procedure")));

    do {
        wait_result = WaitForSingleObject(tpdata.data->trigger_event, INFINITE);
    } while (wait_result == WAIT_TIMEOUT);

    switch (wait_result)
    {
        case WAIT_OBJECT_0:
        {
            LONG prev_status;

            prev_status =
                InterlockedCompareExchange(&tpdata.data->status, THREAD_STATUS_RUNNING, THREAD_STATUS_STARTING);
            switch (prev_status)
            {
                case THREAD_STATUS_STARTING:
                    LOG_TRACE(tpdata.logger, (_T("Handing off to real thread procedure")));
                    ret = !tpdata.desc->thread_proc(tpdata.logger, tpdata.thread_proc_param);
                    break;
                case THREAD_STATUS_STOPPING:
                    LOG_TRACE(tpdata.logger, (_T("Stopping dummy thread procedure")));
                    ret = 0;
                    break;
                default:
                    LOG_ERROR(tpdata.logger, (_T("Invalid status for thread: %d"), prev_status));
                    ret = 1;
            }
            break;
        }
        case WAIT_FAILED:
            ret = GetLastError();
            LOG_ERROR(tpdata.logger, (_T("Error while waiting for thread trigger event: Error %d"), ret));
            if (!ret) ret = 1;
            break;
        default:
            ret = GetLastError();
            LOG_ERROR(tpdata.logger, (
                _T("Unexpected return value from WaitForMultipleObjects: Ret %d Error %d"),
                wait_result,
                ret
            ));
            if (!ret) ret = 1;
    }

    InterlockedExchange(&tpdata.data->status, THREAD_STATUS_STOPPING);
    tpdata.desc->cleanup_func(tpdata.logger, tpdata.data, tpdata.thread_proc_param);
    CloseHandle(tpdata.data->handle);
    CloseHandle(tpdata.data->trigger_event);
    InterlockedExchange(&tpdata.data->status, THREAD_STATUS_STOPPED);

    return ret;
}

BOOL thread_prepare(logger_instance* const logger, thread_description* const desc, thread_data* const data,
                    void* const thread_proc_param)
{
    struct thread_proc_data* tpdata;

    LOG_TRACE(logger, (_T("Preparing thread")));

    tpdata = (struct thread_proc_data*)HeapAlloc(GetProcessHeap(), 0, sizeof(struct thread_proc_data));
    if (!tpdata)
    {
        LOG_CRITICAL(logger, (_T("Could not allocate %lu bytes"), (unsigned long)sizeof(struct thread_proc_data)));
        return FALSE;
    }

    tpdata->logger = logger;
    tpdata->desc = desc;
    tpdata->data = data;
    tpdata->thread_proc_param = thread_proc_param;

    data->trigger_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!data->trigger_event)
    {
        LOG_CRITICAL(logger, (_T("Could not create thread trigger event: Error %d"), GetLastError()));
        HeapFree(GetProcessHeap(), 0, tpdata);
        return FALSE;
    }

    data->handle = CreateThread(NULL, 0, dummy_thread_proc, (LPVOID)tpdata, 0, NULL);
    if (data->handle == NULL)
    {
        LOG_CRITICAL(logger, (_T("Could not create thread: Error %d"), GetLastError()));
        CloseHandle(data->trigger_event);
        HeapFree(GetProcessHeap(), 0, tpdata);
        return FALSE;
    }

    data->status = THREAD_STATUS_PREPARED;

    LOG_TRACE(logger, (_T("Prepared thread")));

    return TRUE;
}

THREAD_RUN_ERROR thread_run(logger_instance* const logger, thread_description* const desc, thread_data* const data)
{
    LONG prev_status;

    (void)desc;

    LOG_TRACE(logger, (_T("Starting thread")));

    prev_status = InterlockedCompareExchange(&data->status, THREAD_STATUS_STARTING, THREAD_STATUS_PREPARED);
    if (prev_status != THREAD_STATUS_PREPARED)
    {
        if (prev_status >= THREAD_STATUS_STOPPING)
        {
            LOG_ERROR(logger, (_T("Could not start thread because it has already been stopped")));
            return THREAD_RUN_ERROR_ALREADY_FINISHED;
        }
        else
        {
            LOG_ERROR(logger, (_T("Could not start thread because it has already been started")));
            return THREAD_RUN_ERROR_ALREADY_RUNNING;
        }
    }

    if (!SetEvent(data->trigger_event))
    {
        LOG_CRITICAL(logger, (_T("Signaling pipe thread trigger event failed: Error %d"), GetLastError()));
        return THREAD_RUN_ERROR_OTHER;
    }

    LOG_TRACE(logger, (_T("Started thread")));

    return THREAD_RUN_ERROR_SUCCESS;
}

BOOL thread_stop(logger_instance* const logger, thread_description* const desc, thread_data* const data)
{
    (void)desc;

    LOG_TRACE(logger, (_T("Sending stop signal to thread")));

    if (InterlockedCompareExchange(&data->status, THREAD_STATUS_STOPPING, THREAD_STATUS_PREPARED)
        == THREAD_STATUS_PREPARED)
    {
        if (!SetEvent(data->trigger_event))
        {
            LOG_CRITICAL(logger, (_T("Signaling thread trigger event failed: Error %d"), GetLastError()));
            return FALSE;
        }
    }
    else if (InterlockedCompareExchange(&data->status, THREAD_STATUS_STOPPING, THREAD_STATUS_STARTING)
             == THREAD_STATUS_STARTING ||
             InterlockedCompareExchange(&data->status, THREAD_STATUS_STOPPING, THREAD_STATUS_RUNNING)
             == THREAD_STATUS_RUNNING)
    {
        if (!desc->stop_func(logger, data))
            return FALSE;
    }

    LOG_TRACE(logger, (_T("Sent stop signal to thread")));

    return TRUE;
}

extern BOOL thread_wait(logger_instance* const logger, thread_description* const desc, thread_data* const data)
{
    HANDLE thread_handle;
    DWORD wait_result;

    (void)desc;

    LOG_TRACE(logger, (_T("Waiting for thread to exit")));

    if (!DuplicateHandle(GetCurrentProcess(), data->handle, GetCurrentProcess(), &thread_handle, SYNCHRONIZE, FALSE, 0))
    {
        LOG_ERROR(logger, (_T("Could not duplicate thread handle: Error %d"), GetLastError()));
        return FALSE;
    }

    do {
        wait_result = WaitForSingleObject(thread_handle, INFINITE);
    } while (wait_result == WAIT_TIMEOUT);

    CloseHandle(thread_handle);

    switch (wait_result)
    {
        case WAIT_OBJECT_0:
            break;
        case WAIT_FAILED:
            LOG_ERROR(logger, (_T("Waiting for thread exit failed: Error %d"), GetLastError()));
            return FALSE;
        default:
            LOG_ERROR(logger, (
                _T("Unexpected return value from WaitForSingleObject: Ret %d Error %d"),
                wait_result,
                GetLastError()
            ));
            return FALSE;
    }

    LOG_TRACE(logger, (_T("Waiting for thread exit finished")));

    return TRUE;
}
