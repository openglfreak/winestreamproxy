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
#include "standalone.h"
#ifdef _UNICODE
#include "wide_to_narrow.h"
#endif
#include <winestreamproxy/logger.h>
#include <winestreamproxy/winestreamproxy.h>

#include <stdio.h>

#include <signal.h>
#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <wincon.h>
#include <winnt.h>

static TCHAR const* const log_level_prefixes[] = {
    _T("TRACE   "),
    _T("DEBUG   "),
    _T("INFO    "),
    _T("WARNING "),
    _T("ERROR   "),
    _T("CRITICAL")
};

#define LOG_MESSAGE_LINE1_FMT _T("%s[%08x]: %s\n")
#define LOG_MESSAGE_LINE2_FMT _T("                    At %s:%li\n")

static int log_message(logger_instance* const logger, LOG_LEVEL const level, void const* const message)
{
    if (level < LOG_LEVEL_TRACE || level > LOG_LEVEL_CRITICAL)
        return 0;

    if (level == LOG_LEVEL_TRACE || (logger && LOG_IS_ENABLED(logger, LOG_LEVEL_TRACE)))
    {
        TCHAR const* file;
        long line;

        log_get_file_and_line((void const**)&file, &line);
        _ftprintf(level >= LOG_LEVEL_ERROR ? stderr : stdout, LOG_MESSAGE_LINE1_FMT LOG_MESSAGE_LINE2_FMT,
                  log_level_prefixes[level], (unsigned int)GetCurrentThreadId(), (TCHAR const*)message, file, line);
    }
    else
        _ftprintf(level >= LOG_LEVEL_ERROR ? stderr : stdout, LOG_MESSAGE_LINE1_FMT, log_level_prefixes[level],
                  (unsigned int)GetCurrentThreadId(), (TCHAR const*)message);

    return 1;
}

static double_spawn_data double_spawn;

void state_change_callback(logger_instance* const logger, proxy_data* const proxy, PROXY_STATE const prev_state,
                           PROXY_STATE const new_state)
{
    (void)proxy;
    (void)prev_state;

    if (new_state == PROXY_STATE_RUNNING)
        double_spawn_finish(logger, &double_spawn);
}

static HANDLE exit_event;

WINAPI BOOL console_ctrl_handler(DWORD const ctrl_type)
{
    switch (ctrl_type)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            SetEvent(exit_event);
            return TRUE;
    }
    return FALSE;
}

void signal_handler(int const signal)
{
    (void)signal;

    SetEvent(exit_event);
}

int standalone_main(unsigned int const verbose, TCHAR const* const pipe_arg, TCHAR const* const socket_arg)
{
    logger_instance* logger;
    LOG_LEVEL log_level;
    DOUBLE_SPAWN_RETURN dsret;
    BOOL deallocate_pipe_path;
    proxy_parameters params;
    proxy_data* proxy;

    if (!log_create_logger(log_message, (unsigned char)sizeof(TCHAR), &logger))
    {
        log_message(0, LOG_LEVEL_CRITICAL, _T("Couldn't create logger"));
        return 1;
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

    dsret = double_spawn_execute(logger, &double_spawn);
    if (dsret != DOUBLE_SPAWN_RETURN_CONTINUE)
    {
        log_destroy_logger(logger);
        return dsret != DOUBLE_SPAWN_RETURN_EXIT ? 1 : 0;
    }

    if (pipe_arg[0] != _T('\\') || pipe_arg[1] != _T('\\'))
    {
        params.paths.named_pipe_path = pipe_name_to_path(logger, pipe_arg);
        if (!params.paths.named_pipe_path)
        {
            log_destroy_logger(logger);
            return 1;
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
        log_destroy_logger(logger);
        return 1;
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
        log_destroy_logger(logger);
        return 1;
    }

    params.state_change_callback = state_change_callback;

    if (!proxy_create(logger, params, &proxy))
    {
        CloseHandle(params.exit_event);
#ifdef _UNICODE
        HeapFree(GetProcessHeap(), 0, (char*)params.paths.unix_socket_path);
#endif
        if (deallocate_pipe_path)
            deallocate_path(params.paths.named_pipe_path);
        log_destroy_logger(logger);
        return 1;
    }

    exit_event = params.exit_event;
    if (!SetConsoleCtrlHandler(console_ctrl_handler, TRUE))
        LOG_ERROR(logger, (_T("Could not set console ctrl handler: Error %d"), GetLastError()));
    signal(SIGTERM, signal_handler);

    proxy_enter_loop(proxy);

    proxy_destroy(proxy);
    CloseHandle(params.exit_event);
#ifdef _UNICODE
    HeapFree(GetProcessHeap(), 0, (char*)params.paths.unix_socket_path);
#endif
    if (deallocate_pipe_path)
        deallocate_path(params.paths.named_pipe_path);
    log_destroy_logger(logger);

    return 0;
}
