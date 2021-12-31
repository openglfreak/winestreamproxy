/* Copyright (C) 2020-2021 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "double_spawn.h"
#include "misc.h"
#include "standalone.h"
#include <winestreamproxy/logger.h>
#include <winestreamproxy/winestreamproxy.h>

#include <assert.h>
#include <stdio.h>

#include <ntdef.h>
#include <ntstatus.h>
#include <signal.h>
#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <wincon.h>
#include <winnt.h>
#include <winternl.h>

#ifndef _tcsnlen
#ifdef _UNICODE
#define _tcsnlen wcsnlen
#elif defined(_MBCS)
#define _tcsnlen _mbsnblen
#else
#define _tcsnlen strnlen
#endif
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#endif

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

void state_change_callback(logger_instance* const logger, proxy_data* const proxy, PROXY_STATE const prev_state,
                           PROXY_STATE const new_state)
{
    (void)proxy;
    (void)prev_state;

    if (new_state == PROXY_STATE_RUNNING)
        double_spawn_exit_parent(logger);
}

typedef struct wait_shutdown_thread_params {
    HANDLE exit_event;
    HANDLE shutdown_event;
} wait_shutdown_thread_params;

DWORD WINAPI wait_shutdown_event(LPVOID const lpParameter)
{
    wait_shutdown_thread_params params;
    HANDLE wait_handles[2];
    DWORD wait_result;
    DWORD ret;

    params = *(wait_shutdown_thread_params const*)lpParameter;
    HeapFree(GetProcessHeap(), 0, lpParameter);

    wait_handles[0] = params.exit_event;
    wait_handles[1] = params.shutdown_event;

    do {
        wait_result = \
            WaitForMultipleObjects(sizeof(wait_handles) / sizeof(wait_handles[0]), wait_handles, FALSE, INFINITE);
    } while (wait_result == WAIT_TIMEOUT);

    switch (wait_result)
    {
        case WAIT_OBJECT_0:
        case WAIT_OBJECT_0 + 1:
            ret = 0;
            break;
        default:
            ret = 1;
    }

    SetEvent(params.exit_event);
    return ret;
}

static BOOL legacy_make_process_system(logger_instance* const logger, HANDLE* const shutdown_event)
{
    HMODULE ntdll_handle;
    HANDLE CDECL (*p__wine_make_process_system)(void);

    ntdll_handle = GetModuleHandle(_T("ntdll.dll"));
    if (ntdll_handle == NULL)
    {
        LOG_ERROR(logger, (_T("Could not get handle to ntdll")));
        return FALSE;
    }

    p__wine_make_process_system = \
        (HANDLE CDECL (*)(void))(ULONG_PTR)GetProcAddress(ntdll_handle, "__wine_make_process_system");
    if (p__wine_make_process_system == NULL)
    {
        LOG_TRACE(logger, (_T("Could not get pointer to __wine_make_process_system function")));
        return FALSE;
    }

    *shutdown_event = p__wine_make_process_system();
    if (!*shutdown_event)
    {
        LOG_ERROR(logger, (_T("Error in __wine_make_process_system")));
        return FALSE;
    }

    return TRUE;
}

static BOOL new_make_process_system(logger_instance* const logger, HANDLE* const shutdown_event)
{
    NTSTATUS status = NtSetInformationProcess(GetCurrentProcess(),
        (PROCESS_INFORMATION_CLASS)1000 /*ProcessWineMakeProcessSystem*/, shutdown_event, sizeof(HANDLE));
    if (NT_SUCCESS(status))
        return TRUE;
    if (status == STATUS_NOT_IMPLEMENTED)
        LOG_TRACE(logger, (_T("ProcessWineMakeProcessSystem not implemented")));
    else
        LOG_ERROR(logger, (_T("Error in NtSetInformationProcess(..., ProcessWineMakeProcessSystem, ...)")));
    return FALSE;
}

static BOOL make_process_system(logger_instance* const logger, HANDLE const exit_event)
{
    wait_shutdown_thread_params* params;
    HANDLE wait_shutdown_thread;
    HANDLE shutdown_event;

    LOG_TRACE(logger, (_T("Marking process as system process")));

    params = (wait_shutdown_thread_params*)HeapAlloc(GetProcessHeap(), 0, sizeof(wait_shutdown_thread_params));
    if (!params)
    {
        LOG_CRITICAL(logger, (_T("Failed to allocate %lu bytes"), (unsigned long)sizeof(wait_shutdown_thread_params)));
        return TRUE;
    }

    if (!legacy_make_process_system(logger, &shutdown_event) && !new_make_process_system(logger, &shutdown_event))
    {
        LOG_TRACE(logger, (_T("Could not mark process as system process")));
        HeapFree(GetProcessHeap(), 0, params);
        return TRUE;
    }

    params->exit_event = exit_event;
    params->shutdown_event = shutdown_event;
    wait_shutdown_thread = CreateThread(NULL, 0, wait_shutdown_event, (LPVOID)params, 0, NULL);
    if (wait_shutdown_thread == NULL)
    {
        LOG_CRITICAL(logger, (_T("Could not create shutdown event wait thread")));
        HeapFree(GetProcessHeap(), 0, params);
        return TRUE;
    }

    LOG_INFO(logger, (_T("Marked process as system process")));

    return TRUE;
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

static int standalone_main_3(logger_instance* const logger, BOOL const is_ds_child, int const system,
                             TCHAR const* const pipe_arg, TCHAR const* const socket_arg)
{
    BOOL deallocate_pipe_path;
    proxy_parameters params;
    proxy_data* proxy;

    if (pipe_arg[0] != _T('\\') || pipe_arg[1] != _T('\\'))
    {
        params.paths.named_pipe_path = pipe_name_to_path(logger, pipe_arg);
        if (!params.paths.named_pipe_path)
        {
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
        return 1;
    }

    params.state_change_callback = is_ds_child ? state_change_callback : 0;

    if (!proxy_create(logger, params, &proxy))
    {
        CloseHandle(params.exit_event);
#ifdef _UNICODE
        HeapFree(GetProcessHeap(), 0, (char*)params.paths.unix_socket_path);
#endif
        if (deallocate_pipe_path)
            deallocate_path(params.paths.named_pipe_path);
        return 1;
    }

    lower_process_priority(logger);

    if (system && !make_process_system(logger, params.exit_event))
    {
        proxy_destroy(proxy);
        CloseHandle(params.exit_event);
#ifdef _UNICODE
        HeapFree(GetProcessHeap(), 0, (char*)params.paths.unix_socket_path);
#endif
        if (deallocate_pipe_path)
            deallocate_path(params.paths.named_pipe_path);
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

    return 0;
}

static int standalone_main_2(unsigned int const verbose, int const system, TCHAR const* const pipe_arg,
                             TCHAR const* const socket_arg)
{
    logger_instance* logger;
    LOG_LEVEL log_level;
    int ret;

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

    ret = standalone_main_3(logger, TRUE, system, pipe_arg, socket_arg);

    log_destroy_logger(logger);
    return ret;
}

int double_spawn_proc(void* aux_data, size_t aux_data_size)
{
    char* p;
    unsigned int verbose;
    int system;
    TCHAR const* pipe_name, * socket_path;
    size_t pipe_name_len, socket_path_len;

    if (aux_data_size < sizeof(unsigned int) + 2)
        return 1;

    p = (char*)aux_data;

    verbose = *(unsigned int*)p;
    p += sizeof(unsigned int);
    aux_data_size -= sizeof(unsigned int);

    system = *(int*)p;
    p += sizeof(int);
    aux_data_size -= sizeof(int);

    pipe_name = (TCHAR const*)p;
    pipe_name_len = _tcsnlen(pipe_name, aux_data_size);
    p += (pipe_name_len + 1) * sizeof(TCHAR);
    aux_data_size -= (pipe_name_len + 1) * sizeof(TCHAR);

    socket_path = (TCHAR const*)p;
    socket_path_len = _tcsnlen(socket_path, aux_data_size);
    p += (socket_path_len + 1) * sizeof(TCHAR);
    aux_data_size -= (socket_path_len + 1) * sizeof(TCHAR);

    assert(aux_data_size == 0);

    return standalone_main_2(verbose, system, pipe_name, socket_path);
}

int standalone_main(unsigned int const verbose, int const foreground, int const system, TCHAR const* const pipe_arg,
                    TCHAR const* const socket_arg)
{
    logger_instance* logger;
    LOG_LEVEL log_level;

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

    LOG_TRACE(logger, (_T("Created main logger")));

    if (foreground)
        standalone_main_3(logger, FALSE, system, pipe_arg, socket_arg);
    else
    {
        size_t pipe_name_len, socket_path_len;
        size_t data_size;
        char* data;

        pipe_name_len = _tcslen(pipe_arg);
        socket_path_len = _tcslen(socket_arg);

        data_size = sizeof(unsigned int) + sizeof(int) + (pipe_name_len + 1) * sizeof(TCHAR)
                    + (socket_path_len + 1) * sizeof(TCHAR);
        data = (char*)HeapAlloc(GetProcessHeap(), 0, data_size);
        if (!data)
        {
            LOG_CRITICAL(logger, (_T("Failed to allocate %lu bytes"), (unsigned long)data_size));
            log_destroy_logger(logger);
            return 1;
        }

        *(unsigned int*)data = verbose;
        *(int*)(data + sizeof(unsigned int)) = system;
        RtlCopyMemory(data + sizeof(unsigned int) + sizeof(int), pipe_arg, (pipe_name_len + 1) * sizeof(TCHAR));
        RtlCopyMemory(data + sizeof(unsigned int) + sizeof(int) + (pipe_name_len + 1) * sizeof(TCHAR), socket_arg,
                      (socket_path_len + 1) * sizeof(TCHAR));

        double_spawn_fork(logger, double_spawn_proc, data, data_size);

        HeapFree(GetProcessHeap(), 0, data);
    }

    log_destroy_logger(logger);
    return 0;
}
