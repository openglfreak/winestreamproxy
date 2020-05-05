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
#include "standalone.h"

#include <stdio.h>
#include <string.h>

#include <tchar.h>
#include <windef.h>
#include <winnt.h>

static void print_help(TCHAR const* const arg0)
{
    TCHAR const* exe;

    if (arg0)
    {
        exe = _tcsrchr(arg0, TEXT('\\'));
        if (exe)
            ++exe;
        else
            exe = arg0;
    }
    else
        exe = TEXT("winestreamproxy.exe.so");

    _tprintf(TEXT("Usage: %s <pipe name> <socket name>\n"), exe);
}

#ifdef __cplusplus
extern "C"
#endif /* defined(__cplusplus) */
int __cdecl _tmain(int const argc, TCHAR* argv[])
{
    TCHAR* pipe_arg;
    TCHAR* socket_arg;

    if (argc != 1 + 2)
    {
        print_help(argc >= 1 ? argv[0] : 0);
        return 1;
    }

    pipe_arg = argv[1];
    socket_arg = argv[2];

    if (try_register_service(pipe_arg, socket_arg))
        return 0;

    return standalone_main(pipe_arg, socket_arg);
}
