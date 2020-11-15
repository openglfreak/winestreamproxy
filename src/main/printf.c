/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>

#include "printf.h"

static HMODULE msvcrt_handle;

static BOOL load_msvcrt(void)
{
    if (msvcrt_handle)
        return TRUE;

    msvcrt_handle = LoadLibrary(_T("msvcrt.dll"));
    return msvcrt_handle != NULL;
}

typedef struct MSVCRT__iobuf {
    char* _ptr;
    int   _cnt;
    char* _base;
    int   _flag;
    int   _file;
    int   _charbuf;
    int   _bufsiz;
    char* _tmpfname;
} MSVCRT_FILE;

static MSVCRT_FILE* (__cdecl* pmsvcrt___iob_func)(void);

static BOOL load_iob_func(void)
{
    if (pmsvcrt___iob_func)
        return TRUE;

    if (!load_msvcrt())
        return FALSE;

    pmsvcrt___iob_func = (MSVCRT_FILE* (__cdecl*)(void))(ULONG_PTR)GetProcAddress(msvcrt_handle, "__iob_func");
    return pmsvcrt___iob_func != NULL;
}

int WINAPIV fake_printf(char const* format, ...)
{
    static int (__cdecl* pmsvcrt_vprintf)(char const*, __ms_va_list);
    __ms_va_list valist;
    int ret;

    if (!pmsvcrt_vprintf)
    {
        if (!load_msvcrt())
            return -1;

        pmsvcrt_vprintf =
            (int (__cdecl*)(char const*, __ms_va_list))(ULONG_PTR)GetProcAddress(msvcrt_handle, "vprintf");
        if (pmsvcrt_vprintf == NULL)
            return -1;
    }

    __ms_va_start(valist, format);
    ret = pmsvcrt_vprintf(format, valist);
    __ms_va_end(valist);
    return ret;
}

int WINAPIV fake_fprintf(FILE* stream, char const* format, ...)
{
    static int (__cdecl* pmsvcrt_vfprintf)(MSVCRT_FILE*, char const*, __ms_va_list);
    int ret;

    if (!load_iob_func())
        return -1;

    if (!pmsvcrt_vfprintf)
    {
        if (!load_msvcrt())
            return -1;

        pmsvcrt_vfprintf = (int (__cdecl*)(MSVCRT_FILE*, char const*, __ms_va_list))(ULONG_PTR)\
            GetProcAddress(msvcrt_handle, "vfprintf");
        if (pmsvcrt_vfprintf == NULL)
            return -1;
    }

    if (stream == stdout || stream == stderr)
    {
        __ms_va_list valist;

        __ms_va_start(valist, format);
        if (stream == stdout)
            ret = pmsvcrt_vfprintf(pmsvcrt___iob_func() + 1, format, valist);
        else
            ret = pmsvcrt_vfprintf(pmsvcrt___iob_func() + 2, format, valist);
        __ms_va_end(valist);
    }
    else
    {
        va_list valist;

        va_start(valist, format);
        ret = vfprintf(stream, format, valist);
        va_end(valist);
    }
    return ret;
}

int WINAPIV fake_wprintf(wchar_t const* format, ...)
{
    static int (__cdecl* pmsvcrt_vwprintf)(wchar_t const*, __ms_va_list);
    __ms_va_list valist;
    int ret;

    if (!pmsvcrt_vwprintf)
    {
        if (!load_msvcrt())
            return 0;

        pmsvcrt_vwprintf =
            (int (__cdecl*)(wchar_t const*, __ms_va_list))(ULONG_PTR)GetProcAddress(msvcrt_handle, "vwprintf");
        if (pmsvcrt_vwprintf == NULL)
            return 0;
    }

    __ms_va_start(valist, format);
    ret = pmsvcrt_vwprintf(format, valist);
    __ms_va_end(valist);
    return ret;
}

int WINAPIV fake_fwprintf(FILE* stream, wchar_t const* format, ...)
{
    static int (__cdecl* pmsvcrt_vfwprintf)(MSVCRT_FILE*, wchar_t const*, __ms_va_list);
    int ret;

    if (!load_iob_func())
        return -1;

    if (!pmsvcrt_vfwprintf)
    {
        if (!load_msvcrt())
            return 0;

        pmsvcrt_vfwprintf = (int (__cdecl*)(MSVCRT_FILE*, wchar_t const*, __ms_va_list))(ULONG_PTR)\
            GetProcAddress(msvcrt_handle, "vfwprintf");
        if (pmsvcrt_vfwprintf == NULL)
            return -1;
    }

    if (stream == stdout || stream == stderr)
    {
        __ms_va_list valist;

        __ms_va_start(valist, format);
        if (stream == stdout)
            ret = pmsvcrt_vfwprintf(pmsvcrt___iob_func() + 1, format, valist);
        else
            ret = pmsvcrt_vfwprintf(pmsvcrt___iob_func() + 2, format, valist);
        __ms_va_end(valist);
    }
    else
    {
        va_list valist;

        va_start(valist, format);
        ret = vfwprintf(stream, format, valist);
        va_end(valist);
    }
    return ret;
}
