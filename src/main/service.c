/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "service.h"
#ifdef _UNICODE
#include "wide_to_narrow.h"
#endif
#include <winestreamproxy/logger.h>
#include <winestreamproxy/winestreamproxy.h>

#include <stdio.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>
#include <winsvc.h>

TCHAR const* pipe_arg;
TCHAR const* socket_arg;

static logger_instance* logger;
/*HANDLE service_event_source;*/
TCHAR const service_name[] = TEXT("Named pipe to unix socket proxy");
void CALLBACK service_proc(DWORD argc, LPTSTR* argv);
SERVICE_TABLE_ENTRY const service_table[] = { { (LPTSTR)service_name, service_proc }, { 0 } };
SERVICE_STATUS service_status = { 0 };
SERVICE_STATUS_HANDLE service_status_handle;
HANDLE service_exit_event;

/*static TCHAR const* const log_level_prefixes[] = {
    TEXT("TRACE"),
    TEXT("DEBUG"),
    TEXT("INFO"),
    TEXT("WARNING"),
    TEXT("ERROR"),
    TEXT("CRITICAL")
};

static WORD const log_level_types[] = {
    EVENTLOG_INFORMATION_TYPE,
    EVENTLOG_INFORMATION_TYPE,
    EVENTLOG_INFORMATION_TYPE,
    EVENTLOG_WARNING_TYPE,
    EVENTLOG_ERROR_TYPE,
    EVENTLOG_ERROR_TYPE
};*/

static TCHAR const* const log_level_prefixes[] = {
    TEXT("TRACE   "),
    TEXT("DEBUG   "),
    TEXT("INFO    "),
    TEXT("WARNING "),
    TEXT("ERROR   "),
    TEXT("CRITICAL")
};

static int log_message(logger_instance* const logger, LOG_LEVEL const level, void const* const message)
{
    (void)logger;
    (void)message;

    if (level < LOG_LEVEL_TRACE || level > LOG_LEVEL_CRITICAL)
        return 0;

    /* I am not implementing this crap... */
    /*ReportEvent(service_event_source, log_level_types[level],*/
    _ftprintf(level >= LOG_LEVEL_ERROR ? stderr : stdout, TEXT("%s: %s\n"),
              log_level_prefixes[level], (TCHAR const*)message);

    return 1;
}

static void service_set_status_stopped(logger_instance* const logger)
{
    service_status.dwControlsAccepted = 0;
    service_status.dwCurrentState = SERVICE_STOPPED;
    service_status.dwWin32ExitCode = GetLastError();
    service_status.dwCheckPoint = 1;
    if (SetServiceStatus(service_status_handle , &service_status) == 0)
        LOG_ERROR(logger, (TEXT("Failed to set service status to stopped: Error %d"), GetLastError()));
}

void WINAPI service_ctrl_handler(DWORD control)
{
    switch (control)
    {
        case SERVICE_CONTROL_STOP:
            if (service_status.dwCurrentState != SERVICE_RUNNING)
                break;

            service_status.dwControlsAccepted = 0;
            service_status.dwCurrentState = SERVICE_STOP_PENDING;
            service_status.dwWin32ExitCode = 0;
            service_status.dwCheckPoint = 3;
            if (SetServiceStatus(service_status_handle , &service_status) == 0)
                LOG_ERROR(logger, (TEXT("Failed to set service status to stopping: Error %d"), GetLastError()));

            SetEvent(service_exit_event);
            break;
    }
}

void CALLBACK service_proc(DWORD const argc, LPTSTR* const argv)
{
    BOOL deallocate_pipe_path;
    connection_paths paths;
    proxy_data* proxy;

    (void)argc;
    (void)argv;

    /*service_event_source = RegisterEventSource(NULL, service_name);
    if (service_event_source == NULL)
        return;*/

    if (!log_create_logger(log_message, (unsigned char)sizeof(TCHAR), &logger))
    {
        log_message(0, LOG_LEVEL_CRITICAL, TEXT("Couldn't create logger"));
        return;
    }

#ifdef NDEBUG
    log_set_min_level(logger, LOG_LEVEL_INFO);
#elif defined(TRACE)
    log_set_min_level(logger, LOG_LEVEL_TRACE);
#else
    log_set_min_level(logger, LOG_LEVEL_DEBUG);
#endif

    service_status_handle = RegisterServiceCtrlHandler(service_name, service_ctrl_handler);
    if (service_status_handle == 0)
    {
        LOG_CRITICAL(logger, (TEXT("Failed to register service control handler: Error %d"), GetLastError()));
        return;
    }

    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState = SERVICE_START_PENDING;
    if (SetServiceStatus(service_status_handle , &service_status) == 0)
        LOG_ERROR(logger, (TEXT("Failed to set service status to starting: Error %d"), GetLastError()));

    if (pipe_arg[0] != TEXT('\\') || pipe_arg[1] != TEXT('\\'))
    {
        paths.named_pipe_path = pipe_name_to_path(logger, pipe_arg);
        if (!paths.named_pipe_path)
        {
            service_set_status_stopped(logger);
            return;
        }
        deallocate_pipe_path = TRUE;
    }
    else
    {
        paths.named_pipe_path = pipe_arg;
        deallocate_pipe_path = FALSE;
    }

#ifdef _UNICODE
    paths.unix_socket_path = wide_to_narrow(logger, socket_arg);
    if (!paths.unix_socket_path)
    {
        service_set_status_stopped(logger);
        return;
    }
#else
    paths.unix_socket_path = socket_arg;
#endif

    service_exit_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!service_exit_event)
        return;

    if (!create_proxy(logger, paths, service_exit_event, 0, &proxy))
    {
        service_set_status_stopped(logger);
        return;
    }

    service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    service_status.dwCurrentState = SERVICE_RUNNING;
    service_status.dwWin32ExitCode = 0;
    service_status.dwCheckPoint = 0;
    if (SetServiceStatus(service_status_handle , &service_status) == 0)
    {
        LOG_CRITICAL(logger, (TEXT("Failed to set service status to running: Error %d"), GetLastError()));
        service_set_status_stopped(logger);
        return;
    }

    enter_proxy_loop(proxy);

    destroy_proxy(proxy);
    CloseHandle(service_exit_event);
#ifdef _UNICODE
    HeapFree(GetProcessHeap(), paths.unix_socket_path);
#endif
    if (deallocate_pipe_path)
        deallocate_path(paths.named_pipe_path);

    service_status.dwControlsAccepted = 0;
    service_status.dwCurrentState = SERVICE_STOPPED;
    service_status.dwWin32ExitCode = 0;
    service_status.dwCheckPoint = 4;
    if (SetServiceStatus(service_status_handle , &service_status) == 0)
        LOG_ERROR(logger, (TEXT("Failed to set service status to stopped: Error %d"), GetLastError()));

    log_destroy_logger(logger);
}

BOOL try_register_service(TCHAR* const _pipe_arg, TCHAR* const _socket_arg)
{
    pipe_arg = _pipe_arg;
    socket_arg = _socket_arg;

    return !!StartServiceCtrlDispatcher(service_table);
}
