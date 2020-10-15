
/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "misc.h"
#include <winestreamproxy/logger.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#ifdef _UNICODE
#include <winnls.h>
#endif
#include <winnt.h>

void lower_process_priority(logger_instance* const logger)
{
    LOG_TRACE(logger, (_T("Lowering the process priority")));

    if (!SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS))
    {
        LOG_ERROR(logger, (_T("Failed to lower the process priority")));
        return;
    }

    LOG_TRACE(logger, (_T("Lowered the process priority")));
}

#ifdef _UNICODE

LPSTR wide_to_narrow(logger_instance* const logger, LPCWCH const wide_path)
{
    int length, length2;
    LPSTR mb_path;

    length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_path, -1, NULL, 0, NULL, NULL);
    if (length == 0)
    {
        LOG_ERROR(logger, (_T("WideCharToMultiByte failed: Error %d"), GetLastError()));
        return NULL;
    }

    mb_path = (LPSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(char) * length);
    if (mb_path == NULL)
    {
        LOG_ERROR(logger, (_T("Failed to allocate %lu bytes"), (unsigned long)(sizeof(char) * length)));
        return NULL;
    }

    length2 = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_path, -1, mb_path, length, NULL, NULL);
    if (length2 == 0)
    {
        LOG_ERROR(logger, (_T("WideCharToMultiByte failed: Error %d"), GetLastError()));
        HeapFree(GetProcessHeap(), 0, mb_path);
        return NULL;
    }

    return mb_path;
}

#endif /* !defined(_UNICODE) */

BOOL get_envvar(TCHAR const* const name, TCHAR** const out_content, size_t* const out_length)
{
    TCHAR* content;
    size_t capacity;
    DWORD length;

    capacity = 32;
    while (TRUE)
    {
        content = (TCHAR*)HeapAlloc(GetProcessHeap(), 0, capacity * sizeof(TCHAR));
        if (content == NULL)
            return FALSE;

        length = GetEnvironmentVariable(name, content, capacity);
        if (length < capacity)
            break;

        HeapFree(GetProcessHeap(), 0, content);
        capacity *= 2;
    }

    if (length <= 0)
    {
        DWORD err;

        err = GetLastError();
        HeapFree(GetProcessHeap(), 0, content);
        if (err == ERROR_ENVVAR_NOT_FOUND)
        {
            *out_content = 0;
            return TRUE;
        }
        return FALSE;
    }

    *out_content = content;
    *out_length = length;
    return TRUE;
}
