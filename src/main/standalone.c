/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "standalone.h"
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

    if (level < LOG_LEVEL_TRACE || level > LOG_LEVEL_CRITICAL)
        return 0;

    _ftprintf(level >= LOG_LEVEL_ERROR ? stderr : stdout, TEXT("%s: %s\n"),
              log_level_prefixes[level], (TCHAR const*)message);

    return 1;
}

int standalone_main(TCHAR* const pipe_arg, TCHAR* const socket_arg)
{
    logger_instance* logger;
    BOOL deallocate_pipe_path;
    connection_paths paths;
    HANDLE exit_event;
    proxy_data* proxy;

    if (!log_create_logger(log_message, (unsigned char)sizeof(TCHAR), &logger))
    {
        log_message(0, LOG_LEVEL_CRITICAL, TEXT("Couldn't create logger"));
        return 1;
    }

#ifdef NDEBUG
    log_set_min_level(logger, LOG_LEVEL_INFO);
#elif defined(TRACE)
    log_set_min_level(logger, LOG_LEVEL_TRACE);
#else
    log_set_min_level(logger, LOG_LEVEL_DEBUG);
#endif

    if (pipe_arg[0] != TEXT('\\') || pipe_arg[1] != TEXT('\\'))
    {
        paths.named_pipe_path = pipe_name_to_path(logger, pipe_arg);
        if (!paths.named_pipe_path)
            return 1;
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
        return 1;
#else
    paths.unix_socket_path = socket_arg;
#endif

    exit_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!exit_event)
        return 1;

    if (!create_proxy(logger, paths, exit_event, &proxy))
        return 1;

    enter_proxy_loop(proxy);

    destroy_proxy(proxy);
    CloseHandle(exit_event);
#ifdef _UNICODE
    HeapFree(GetProcessHeap(), paths.unix_socket_path);
#endif
    if (deallocate_pipe_path)
        deallocate_path(paths.named_pipe_path);
    log_destroy_logger(logger);

    return 0;
}
