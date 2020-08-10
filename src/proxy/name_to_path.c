/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include <winestreamproxy/logger.h>
#include <winestreamproxy/winestreamproxy.h>

#include <stddef.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

TCHAR const pipe_path_prefix[] = _T("\\\\.\\pipe\\");

#define static_strlen(x) (sizeof(x) / sizeof(x[0]) - 1)

TCHAR* pipe_name_to_path(logger_instance* const logger, TCHAR const* const named_pipe_name)
{
    size_t name_len;
    TCHAR* pipe_path;

    name_len = _tcslen(named_pipe_name);

    pipe_path = (TCHAR*)HeapAlloc(GetProcessHeap(), 0,
                                  sizeof(TCHAR) * (static_strlen(pipe_path_prefix) + name_len + 1));
    if (!pipe_path)
    {
        LOG_ERROR(logger, (
            _T("Failed to allocate %lu bytes"),
            (unsigned long)(sizeof(TCHAR) * (static_strlen(pipe_path_prefix) + name_len + 1))
        ));
        return 0;
    }

    RtlCopyMemory(pipe_path, pipe_path_prefix, sizeof(TCHAR) * static_strlen(pipe_path_prefix));
    RtlCopyMemory(&pipe_path[sizeof(TCHAR) * static_strlen(pipe_path_prefix)], named_pipe_name,
                  sizeof(TCHAR) * (name_len + 1));

    return pipe_path;
}

void deallocate_path(TCHAR const* const named_pipe_path)
{
    if (named_pipe_path)
        HeapFree(GetProcessHeap(), 0, (LPVOID)named_pipe_path);
}
