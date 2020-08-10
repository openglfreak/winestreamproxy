/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "double_spawn.h"
#include <winestreamproxy/logger.h>

#include <errno.h>
#include <stdio.h>
#include <wchar.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>

#define DOUBLE_SPAWN_EVENT_ENVVAR_NAME _T("__WINESTREAMPROXY_DOUBLE_SPAWN_EVENT")

static BOOL double_spawn_parent2(logger_instance* const logger, double_spawn_data* const data)
{
    SECURITY_ATTRIBUTES sattrs;
    TCHAR event_hex[32];
    int event_hex_len;
    STARTUPINFO sinfo;
    PROCESS_INFORMATION procinfo;
    DWORD status;
    HANDLE wait_handles[2];
    DWORD err;

    sattrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    sattrs.lpSecurityDescriptor = NULL;
    sattrs.bInheritHandle = TRUE;
    data->event = CreateEvent(&sattrs, TRUE, FALSE, NULL);
    if (data->event == NULL)
    {
        LOG_CRITICAL(logger, (_T("Could not create double-spawn event: Error %d"), GetLastError()));
        return FALSE;
    }

#if UNICODE
    event_hex_len = swprintf(event_hex, sizeof(event_hex), _T("%p"), data->event);
#else
    event_hex_len = snprintf(event_hex, sizeof(event_hex), "%p", data->event);
#endif
    if (event_hex_len < 0 || (size_t)event_hex_len >= sizeof(event_hex))
    {
        LOG_CRITICAL(logger, (_T("Could not convert double-spawn event handle to string: Error %d"), errno));
        CloseHandle(data->event);
        return FALSE;
    }

    if (!SetEnvironmentVariable(DOUBLE_SPAWN_EVENT_ENVVAR_NAME, event_hex))
    {
        LOG_CRITICAL(logger, (_T("Could not set double spawn environment variable: Error %d"), GetLastError()));
        CloseHandle(data->event);
        return FALSE;
    }

    RtlZeroMemory(&sinfo, sizeof(sinfo));
    sinfo.cb = sizeof(sinfo);
    status = CreateProcess(NULL, GetCommandLine(), NULL, NULL, TRUE, BELOW_NORMAL_PRIORITY_CLASS, NULL, NULL, &sinfo,
                           &procinfo);
    if (!status)
    {
        LOG_CRITICAL(logger, (_T("Could not start process: Error %d"), GetLastError()));
        CloseHandle(data->event);
        return FALSE;
    }
    CloseHandle(procinfo.hThread);

    wait_handles[0] = data->event;
    wait_handles[1] = procinfo.hProcess;
    do {
        status = WaitForMultipleObjects(sizeof(wait_handles) / sizeof(wait_handles[0]), wait_handles, FALSE, INFINITE);
    } while (status == WAIT_TIMEOUT);
    err = GetLastError();
    CloseHandle(procinfo.hProcess);
    CloseHandle(data->event);
    switch (status)
    {
        case WAIT_OBJECT_0:
        case WAIT_OBJECT_0 + 1:
            return TRUE;
        case WAIT_FAILED:
            LOG_ERROR(logger, (_T("WaitForMultipleObjects failed: Error %d"), err));
            break;
        default:
            LOG_ERROR(logger, (_T("Unexpected WaitForMultipleObjects return value: %d"), status));
    }
    return FALSE;
}

static BOOL double_spawn_parent(logger_instance* const logger, double_spawn_data* const data)
{
    BOOL ret;

    LOG_TRACE(logger, (_T("Entering double-spawn parent process function")));

    ret = double_spawn_parent2(logger, data);

    LOG_TRACE(logger, (_T("Exiting double-spawn parent process function")));

    return ret;
}

static BOOL double_spawn_child(logger_instance* const logger, double_spawn_data* const data,
                               TCHAR* const event_envvar_content)
{
    BOOL ret;

    LOG_TRACE(logger, (_T("Entering double-spawn child process function")));

    if (!SetEnvironmentVariable(DOUBLE_SPAWN_EVENT_ENVVAR_NAME, NULL))
        LOG_ERROR(logger, (_T("Could not unset double spawn environment variable: Error %d"), GetLastError()));

    ret = FALSE;

    if (_stscanf(event_envvar_content, _T("%p"), (void**)&data->event) != 1)
        LOG_CRITICAL(logger, (_T("Could not decode double spawn environment variable: Error %d"), errno));
    else if (!GetHandleInformation(data->event, 0))
        LOG_CRITICAL(logger, (_T("Invalid double spawn event handle: Error %d"), GetLastError()));
    else
        ret = TRUE;

    LOG_TRACE(logger, (_T("Exiting double-spawn child process function")));

    return ret;
}

DOUBLE_SPAWN_RETURN double_spawn_execute(logger_instance* const logger, double_spawn_data* const data)
{
    TCHAR event_envvar_content[32];
    DWORD gev_ret;
    DOUBLE_SPAWN_RETURN ret;

    LOG_TRACE(logger, (_T("Executing double-spawn, you should see this message twice")));

    gev_ret = GetEnvironmentVariable(DOUBLE_SPAWN_EVENT_ENVVAR_NAME, event_envvar_content,
                                     sizeof(event_envvar_content));

    if (gev_ret != 0 && gev_ret < sizeof(event_envvar_content))
    {
        LOG_TRACE(logger, (_T("Delegating to double-spawn child process handler")));
        ret = double_spawn_child(logger, data, event_envvar_content)
              ? DOUBLE_SPAWN_RETURN_CONTINUE : DOUBLE_SPAWN_RETURN_ERROR;
    }
    else if (gev_ret != 0 || GetLastError() == ERROR_ENVVAR_NOT_FOUND)
    {
        LOG_TRACE(logger, (_T("Delegating to double-spawn parent process handler")));
        ret = double_spawn_parent(logger, data)
              ? DOUBLE_SPAWN_RETURN_EXIT : DOUBLE_SPAWN_RETURN_ERROR;
    }
    else
    {
        LOG_CRITICAL(logger, (_T("Error getting environment variable")));
        ret = DOUBLE_SPAWN_RETURN_ERROR;
    }

    LOG_TRACE(logger, (_T("Executed double-spawn")));

    return ret;
}

void double_spawn_finish(logger_instance* const logger, double_spawn_data* const data)
{
    LOG_TRACE(logger, (_T("Finishing double-spawn")));

    if (!SetEvent(data->event))
        LOG_ERROR(logger, (_T("Could not signal double-spawn parent process: Error %d"), GetLastError()));
    if (!CloseHandle(data->event))
        LOG_ERROR(logger, (_T("Could not close double-spawn event handle: Error %d"), GetLastError()));

    LOG_TRACE(logger, (_T("Finished double-spawn")));
}
