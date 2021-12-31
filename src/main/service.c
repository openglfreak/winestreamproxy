/* Copyright (C) 2020-2021 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "misc.h"
#include "service.h"
#include <winestreamproxy/logger.h>
#include <winestreamproxy/winestreamproxy.h>

#include <stdio.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>
#include <winsvc.h>

unsigned int verbose;
TCHAR const* pipe_arg;
TCHAR const* socket_arg;

static logger_instance* logger;
/*HANDLE service_event_source;*/
TCHAR const service_name[] = _T("Named pipe to unix socket proxy");
void CALLBACK service_proc(DWORD argc, LPTSTR* argv);
SERVICE_TABLE_ENTRY const service_table[] = { { (LPTSTR)service_name, service_proc }, { 0, 0 } };
SERVICE_STATUS service_status;
SERVICE_STATUS_HANDLE service_status_handle;
HANDLE service_exit_event;

/*static TCHAR const* const svc_log_level_prefixes[] = {
    _T("TRACE"),
    _T("DEBUG"),
    _T("INFO"),
    _T("WARNING"),
    _T("ERROR"),
    _T("CRITICAL")
};

static WORD const log_level_types[] = {
    EVENTLOG_INFORMATION_TYPE,
    EVENTLOG_INFORMATION_TYPE,
    EVENTLOG_INFORMATION_TYPE,
    EVENTLOG_WARNING_TYPE,
    EVENTLOG_ERROR_TYPE,
    EVENTLOG_ERROR_TYPE
};*/

static TCHAR const* const svc_log_level_prefixes[] = {
    _T("TRACE   "),
    _T("DEBUG   "),
    _T("INFO    "),
    _T("WARNING "),
    _T("ERROR   "),
    _T("CRITICAL")
};

#define LOG_MESSAGE_LINE1_FMT _T("%s[%08x]: %s\n")
#define LOG_MESSAGE_LINE2_FMT _T("                    At %s:%li\n")

static int svc_log_message(logger_instance* const logger, LOG_LEVEL const level, void const* const message)
{
    if (level < LOG_LEVEL_TRACE || level > LOG_LEVEL_CRITICAL)
        return 0;

    /* I am not implementing this crap... */
    /*ReportEvent(service_event_source, log_level_types[level],*/

    if (level == LOG_LEVEL_TRACE || (logger && LOG_IS_ENABLED(logger, LOG_LEVEL_TRACE)))
    {
        TCHAR const* file;
        long line;

        log_get_file_and_line((void const**)&file, &line);
        _ftprintf(level >= LOG_LEVEL_ERROR ? stderr : stdout, LOG_MESSAGE_LINE1_FMT LOG_MESSAGE_LINE2_FMT,
                  svc_log_level_prefixes[level], (unsigned int)GetCurrentThreadId(), (TCHAR const*)message, file, line);
    }
    else
        _ftprintf(level >= LOG_LEVEL_ERROR ? stderr : stdout, LOG_MESSAGE_LINE1_FMT, svc_log_level_prefixes[level],
                  (unsigned int)GetCurrentThreadId(), (TCHAR const*)message);

    return 1;
}

static void service_set_status_stopped(logger_instance* const logger)
{
    service_status.dwControlsAccepted = 0;
    service_status.dwCurrentState = SERVICE_STOPPED;
    service_status.dwWin32ExitCode = GetLastError();
    service_status.dwCheckPoint = 1;
    if (SetServiceStatus(service_status_handle , &service_status) == 0)
        LOG_ERROR(logger, (_T("Failed to set service status to stopped: Error %d"), GetLastError()));
}

static void service_set_status_running(logger_instance* const logger, proxy_data* const proxy,
                                       PROXY_STATE const prev_state, PROXY_STATE const new_state)
{
    (void)proxy;
    (void)prev_state;
    (void)new_state;

    service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    service_status.dwCurrentState = SERVICE_RUNNING;
    service_status.dwWin32ExitCode = 0;
    service_status.dwCheckPoint = 0;
    if (SetServiceStatus(service_status_handle , &service_status) == 0)
    {
        LOG_CRITICAL(logger, (_T("Failed to set service status to running: Error %d"), GetLastError()));
        SetEvent(service_exit_event);
    }
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
                LOG_ERROR(logger, (_T("Failed to set service status to stopping: Error %d"), GetLastError()));

            SetEvent(service_exit_event);
            break;
    }
}

void CALLBACK service_proc(DWORD const argc, LPTSTR* const argv)
{
    LOG_LEVEL log_level;
    BOOL deallocate_pipe_path;
    proxy_parameters params;
    proxy_data* proxy;

    (void)argc;
    (void)argv;

    /*service_event_source = RegisterEventSource(NULL, service_name);
    if (service_event_source == NULL)
        return;*/

    if (!log_create_logger(svc_log_message, (unsigned char)sizeof(TCHAR), &logger))
    {
        svc_log_message(0, LOG_LEVEL_CRITICAL, _T("Couldn't create logger"));
        return;
    }

#if defined(TRACE)
    log_level = LOG_LEVEL_TRACE;
#elif defined(NDEBUG)
    log_level = LOG_LEVEL_INFO;
#else
    log_level = LOG_LEVEL_DEBUG;
#endif
    if (verbose < log_level)
        log_level = (LOG_LEVEL)((int)log_level - verbose);
    else
        log_level = (LOG_LEVEL)0;
    log_set_min_level(logger, log_level);

    LOG_TRACE(logger, (_T("Created main logger")));

    service_status_handle = RegisterServiceCtrlHandler(service_name, service_ctrl_handler);
    if (service_status_handle == 0)
    {
        LOG_CRITICAL(logger, (_T("Failed to register service control handler: Error %d"), GetLastError()));
        log_destroy_logger(logger);
        return;
    }

    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    /* Wine already continues with prefix boot when SERVICE_START_PENDING is reached, so we can't send that here. */
    /*service_status.dwCurrentState = SERVICE_START_PENDING;
    if (SetServiceStatus(service_status_handle , &service_status) == 0)
        LOG_ERROR(logger, (_T("Failed to set service status to starting: Error %d"), GetLastError()));*/

    if (pipe_arg[0] != _T('\\') || pipe_arg[1] != _T('\\'))
    {
        params.paths.named_pipe_path = pipe_name_to_path(logger, pipe_arg);
        if (!params.paths.named_pipe_path)
        {
            service_set_status_stopped(logger);
            log_destroy_logger(logger);
            return;
        }
        deallocate_pipe_path = TRUE;
    }
    else
    {
        params.paths.named_pipe_path = pipe_arg;
        deallocate_pipe_path = FALSE;
    }

#ifdef _UNICODE
    params.paths.unix_socket_path = wide_to_narrow(logger, socket_arg);
    if (!params.paths.unix_socket_path)
    {
        if (deallocate_pipe_path)
            deallocate_path(params.paths.named_pipe_path);
        service_set_status_stopped(logger);
        log_destroy_logger(logger);
        return;
    }
#else
    params.paths.unix_socket_path = socket_arg;
#endif

    params.exit_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!params.exit_event)
    {
#ifdef _UNICODE
        HeapFree(GetProcessHeap(), 0, (char*)params.paths.unix_socket_path);
#endif
        if (deallocate_pipe_path)
            deallocate_path(params.paths.named_pipe_path);
        service_set_status_stopped(logger);
        log_destroy_logger(logger);
        return;
    }

    params.state_change_callback = service_set_status_running;

    if (!proxy_create(logger, params, &proxy))
    {
        CloseHandle(params.exit_event);
#ifdef _UNICODE
        HeapFree(GetProcessHeap(), 0, (char*)params.paths.unix_socket_path);
#endif
        if (deallocate_pipe_path)
            deallocate_path(params.paths.named_pipe_path);
        service_set_status_stopped(logger);
        log_destroy_logger(logger);
        return;
    }

    lower_process_priority(logger);

    service_exit_event = params.exit_event;

    proxy_enter_loop(proxy);

    proxy_destroy(proxy);
    CloseHandle(params.exit_event);
#ifdef _UNICODE
    HeapFree(GetProcessHeap(), 0, (char*)params.paths.unix_socket_path);
#endif
    if (deallocate_pipe_path)
        deallocate_path(params.paths.named_pipe_path);

    service_status.dwControlsAccepted = 0;
    service_status.dwCurrentState = SERVICE_STOPPED;
    service_status.dwWin32ExitCode = 0;
    service_status.dwCheckPoint = 4;
    if (SetServiceStatus(service_status_handle , &service_status) == 0)
        LOG_ERROR(logger, (_T("Failed to set service status to stopped: Error %d"), GetLastError()));

    log_destroy_logger(logger);
}

int service_main(unsigned int const _verbose, int const foreground, int const system, TCHAR const* const _pipe_arg,
                 TCHAR const* const _socket_arg)
{
    if (foreground)
    {
        svc_log_message(0, LOG_LEVEL_CRITICAL, _T("Can not start service in foreground"));
        return FALSE;
    }

    if (system)
        svc_log_message(0, LOG_LEVEL_INFO, _T("Services are always system processes, ignoring -y/--system parameter"));

    verbose = _verbose;
    pipe_arg = _pipe_arg;
    socket_arg = _socket_arg;

    return !!StartServiceCtrlDispatcher(service_table);
}
