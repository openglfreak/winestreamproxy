/* Copyright (C) 2020-2021 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "argparser.h"
#include "double_spawn.h"
#include "service.h"
#include "standalone.h"
#include <winestreamproxy/logger.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

#ifndef offsetof
#define offsetof(st, m) ((size_t)((char*)&((st*)0)->m - (char*)0))
#endif

static TCHAR const* const early_log_level_prefixes[] = {
    _T("Trace"),
    _T("Debug"),
    _T("Info"),
    _T("Warning"),
    _T("Error"),
    _T("Error")
};

static int early_log_message(logger_instance* const logger, LOG_LEVEL const level, void const* const message)
{
#ifdef EARLY_TRACE
    TCHAR const* file;
    long line;
#endif

    (void)logger;

    if (level < LOG_LEVEL_TRACE || level > LOG_LEVEL_CRITICAL)
        return 0;

#ifdef EARLY_TRACE
    log_get_file_and_line((void const**)&file, &line);
    _ftprintf(level >= LOG_LEVEL_ERROR ? stderr : stdout, _T("%s: %s\n    At %s:%li\n"),
              early_log_level_prefixes[level], (TCHAR const*)message, file, line);
#else
    _ftprintf(level >= LOG_LEVEL_ERROR ? stderr : stdout, _T("%s: %s\n"), early_log_level_prefixes[level],
              (TCHAR const*)message);
#endif

    return 1;
}

typedef struct main_option_values {
    int show_help;
    int show_version;
    unsigned int verbose;
    int foreground;
    int system;
    int svchost;
    TCHAR const* pipe_name;
    TCHAR const* socket_path;
} main_option_values;

typedef struct main_positionals {
    TCHAR const** positionals;
    size_t positionals_count;
} main_positionals;

argparser_option_list_entry main_arg_option_list[] = {
    { _T("h"),  _T("help"),         ARGPARSER_OPTION_TYPE_PRESENCE,     0, offsetof(main_option_values, show_help) },
    { 0,        _T("version"),      ARGPARSER_OPTION_TYPE_PRESENCE,     0, offsetof(main_option_values, show_version) },
    { _T("v"),  _T("verbose"),      ARGPARSER_OPTION_TYPE_ACCUMULATOR,  0, offsetof(main_option_values, verbose) },
    { _T("f"),  _T("foreground"),   ARGPARSER_OPTION_TYPE_BOOLEAN,      0, offsetof(main_option_values, foreground) },
    { _T("y"),  _T("system"),       ARGPARSER_OPTION_TYPE_BOOLEAN,      0, offsetof(main_option_values, system) },
    { 0,        _T("svchost"),      ARGPARSER_OPTION_TYPE_BOOLEAN,      0, offsetof(main_option_values, svchost) },
    { _T("p"),  _T("pipe"),         ARGPARSER_OPTION_TYPE_STRING,       0, offsetof(main_option_values, pipe_name) },
    { _T("s"),  _T("socket"),       ARGPARSER_OPTION_TYPE_STRING,       0, offsetof(main_option_values, socket_path) },
    { 0,        0,                  (ARGPARSER_OPTION_TYPE)0,           0, 0 }
};

static void output_argparser_error(logger_instance* const logger, ARGPARSER_PARSE_RETURN const argp_ret)
{
    assert(argp_ret.code != ARGPARSER_PARSE_RETURN_SUCCESS);
    switch (argp_ret.code)
    {
        case ARGPARSER_PARSE_RETURN_SUCCESS: break;
        case ARGPARSER_PARSE_RETURN_UNKNOWN_OPTION:
            LOG_CRITICAL(logger, (_T("Unknown option: -%.*s"), argp_ret.arg_len, argp_ret.arg));
            break;
        case ARGPARSER_PARSE_RETURN_EXTRANEOUS_VALUE:
            LOG_CRITICAL(logger, (_T("Option doesn't allow an argument: -%.*s"), argp_ret.arg_len, argp_ret.arg));
            break;
        case ARGPARSER_PARSE_RETURN_MISSING_VALUE:
            LOG_CRITICAL(logger, (_T("Option requires an argument: -%.*s"), argp_ret.arg_len, argp_ret.arg));
            break;
        case ARGPARSER_PARSE_RETURN_MALFORMED_VALUE:
        case ARGPARSER_PARSE_RETURN_VALIDATE_ERROR:
            LOG_CRITICAL(logger, (
                _T("Invalid argument for option -%.*s: %s"),
                argp_ret.arg_len, argp_ret.arg,
                argp_ret.value
            ));
            break;
        case ARGPARSER_PARSE_RETURN_OUT_OF_MEMORY:
            LOG_CRITICAL(logger, (_T("Out of memory, please free up system resources")));
            break;
        case ARGPARSER_PARSE_RETURN_OTHER_ERROR:
            LOG_CRITICAL(logger, (_T("Internal error")));
            break;
        default:
            assert(0);
    }
}

static BOOL parse_args(logger_instance* const logger, int const argc, TCHAR* argv[],
                       main_option_values* const out_optvals, main_positionals* const out_positionals)
{
    argparser_data apdata;
    ARGPARSER_PARSE_RETURN argp_ret;

    apdata.option_list = main_arg_option_list;
    apdata.option_values = out_optvals;
    apdata.positionals = &out_positionals->positionals;
    apdata.positionals_count = &out_positionals->positionals_count;

    argp_ret = argparser_parse_parameters(logger, &apdata, argc, (TCHAR const**)argv);
    if (argp_ret.code != ARGPARSER_PARSE_RETURN_SUCCESS)
    {
        output_argparser_error(logger, argp_ret);
        return FALSE;
    }

    return TRUE;
}

#define _STRINGIFY(x) _T(#x)
#define STRINGIFY(x) _STRINGIFY(x)

#define VERSION STRINGIFY(MAJOR_VERSION) _T(".") STRINGIFY(MINOR_VERSION) _T(".") STRINGIFY(PATCH_VERSION) \
                STRINGIFY(EXTRA_VERSION)

static void print_version(void)
{
    _tprintf(_T("winestreamproxy version %s\n"), VERSION);
}

static void print_help(TCHAR const* const arg0)
{
    TCHAR const* exe;

    if (arg0)
    {
        exe = _tcsrchr(arg0, _T('\\'));
        if (exe)
            ++exe;
        else
            exe = arg0;
    }
    else
        exe = _T("winestreamproxy.exe.so");

    _tprintf(
        _T("Usage: %s [options] <pipe name> <socket name>\n")
        _T("\n")
        _T("-h, --help             Show this help message and exit\n")
        _T("    --version          Show the version number and exit\n")
        _T("-v, --verbose          Be more verbose (can be specified multiple times)\n")
        _T("-f, --foreground       Do not daemonize\n")
        _T("-y, --system           Exit when all other processes have exited\n")
        _T("-p, --pipe <name>      Explicitly specify the pipe name\n")
        _T("-s, --socket <path>    Explicitly specify the socket path\n"),
        exe
    );
}

#ifdef __cplusplus
extern "C"
#endif /* defined(__cplusplus) */
int _tmain(int const argc, TCHAR* argv[])
{
    BOOL exit;
    int dsret;
    logger_instance* early_logger;
    main_option_values optvals;
    main_positionals positionals;
    size_t i;

    dsret = double_spawn_main_hook(&exit);
    if (exit)
        return dsret;

    if (!log_create_logger(early_log_message, (unsigned char)sizeof(TCHAR), &early_logger))
    {
        early_log_message(0, LOG_LEVEL_CRITICAL, _T("Couldn't create early logger"));
        return 1;
    }

#if defined(EARLY_TRACE)
    log_set_min_level(early_logger, LOG_LEVEL_TRACE);
#elif defined(NDEBUG)
    log_set_min_level(early_logger, LOG_LEVEL_INFO);
#else
    log_set_min_level(early_logger, LOG_LEVEL_DEBUG);
#endif

    RtlZeroMemory(&optvals, sizeof(optvals));
    if (!parse_args(early_logger, argc, argv, &optvals, &positionals))
    {
        log_destroy_logger(early_logger);
        return 1;
    }

    if (optvals.show_version)
    {
        print_version();
        log_destroy_logger(early_logger);
        return 0;
    }
    if (optvals.show_help)
    {
        print_help(argc >= 1 ? argv[0] : 0);
        log_destroy_logger(early_logger);
        return 0;
    }

    i = 0;
    if (!optvals.pipe_name)
    {
        if (positionals.positionals_count >= i + 1)
            optvals.pipe_name = positionals.positionals[i++];
        else
        {
            LOG_CRITICAL(early_logger, (_T("Missing pipe name")));
            print_help(argc >= 1 ? argv[0] : 0);
            log_destroy_logger(early_logger);
            return 1;
        }
    }
    if (!optvals.socket_path)
    {
        if (positionals.positionals_count >= i + 1)
            optvals.socket_path = positionals.positionals[i++];
        else
        {
            LOG_CRITICAL(early_logger, (_T("Missing socket path")));
            print_help(argc >= 1 ? argv[0] : 0);
            log_destroy_logger(early_logger);
            return 1;
        }
    }
    if (positionals.positionals_count > i)
    {
        LOG_CRITICAL(early_logger, (_T("Too many positional parameters")));
        print_help(argc >= 1 ? argv[0] : 0);
        log_destroy_logger(early_logger);
        return 0;
    }

    HeapFree(GetProcessHeap(), 0, positionals.positionals);
    log_destroy_logger(early_logger);

    if (optvals.svchost)
        return service_main(optvals.verbose, optvals.foreground, optvals.system, optvals.pipe_name,
                            optvals.socket_path);
    else
        return standalone_main(optvals.verbose, optvals.foreground, optvals.system, optvals.pipe_name,
                               optvals.socket_path);
}
