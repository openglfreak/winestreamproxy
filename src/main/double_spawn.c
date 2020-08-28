/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "attributes.h"
#include "double_spawn.h"
#include <winestreamproxy/logger.h>

#include <errno.h>
#include <stdio.h>
#include <wchar.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>

#define DOUBLE_SPAWN_ENVVAR_NAME _T("__WINESTREAMPROXY_DOUBLE_SPAWN_DATA")

HANDLE event;

ATTR_NODISCARD_CXX("unnecessary parse_hex_digit call")
static inline unsigned char parse_hex_digit(TCHAR const chr) ATTR_NOTHROW_CXX
ATTR_ALWAYS_INLINE ATTR_CONST ATTR_NOTHROW ATTR_NODISCARD("unnecessary parse_hex_digit call");

static inline unsigned char parse_hex_digit(TCHAR const chr) ATTR_NOTHROW_CXX
{
    return chr >= _T('0') && chr <= _T('9') ? chr - _T('0')
           : chr >= _T('A') && chr <= _T('F') ? chr - _T('A') + 10
           : chr >= _T('a') && chr <= _T('f') ? chr - _T('a') + 10
           : 0xFF;
}

static BOOL get_envvar(TCHAR const* const name, TCHAR** const out_content, size_t* const out_length)
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

static BOOL decode_envvar(TCHAR const* envvar_content, BYTE* decoded_content, size_t decoded_content_size)
{
    while (decoded_content_size--)
    {
        unsigned char high_nibble, low_nibble;

        high_nibble = parse_hex_digit(*envvar_content++);
        if (high_nibble > 0xF)
            return FALSE;
        low_nibble = parse_hex_digit(*envvar_content++);
        if (low_nibble > 0xF)
            return FALSE;
        *decoded_content++ = (high_nibble << 4) | low_nibble;
    }

    return TRUE;
}

static BOOL get_data(void** const out_data, size_t* const out_data_size)
{
    TCHAR* envvar_content;
    size_t envvar_content_length = 0;
    size_t decoded_content_size;
    BYTE* decoded_content;

    if (!get_envvar(DOUBLE_SPAWN_ENVVAR_NAME, &envvar_content, &envvar_content_length))
        return FALSE;

    if (!envvar_content)
    {
        *out_data = 0;
        return TRUE;
    }

    if (envvar_content_length % 2 != 0)
    {
        HeapFree(GetProcessHeap(), 0, envvar_content);
        return FALSE;
    }

    decoded_content_size = envvar_content_length / 2;
    decoded_content = (BYTE*)HeapAlloc(GetProcessHeap(), 0, decoded_content_size);
    if (decoded_content == NULL)
    {
        HeapFree(GetProcessHeap(), 0, envvar_content);
        return FALSE;
    }

    if (!decode_envvar(envvar_content, decoded_content, decoded_content_size))
    {
        HeapFree(GetProcessHeap(), 0, decoded_content);
        HeapFree(GetProcessHeap(), 0, envvar_content);
        return FALSE;
    }

    HeapFree(GetProcessHeap(), 0, envvar_content);
    *out_data = decoded_content;
    *out_data_size = decoded_content_size;
    return TRUE;
}

static BOOL verify_parent_process(void)
{
    return TRUE;
}

typedef struct ds_data_intl {
    union ds_data_intl_callback {
        double_spawn_callback funcptr;
        ULONG_PTR ulongptr;
    } callback;
    HANDLE event;
} ds_data_intl;

int double_spawn_main_hook(BOOL* const out_exit)
{
    BYTE* data;
    size_t data_size;
    ds_data_intl* data_intl;
    int ret;

    if (!get_data((void**)&data, &data_size))
    {
        *out_exit = TRUE;
        return 1;
    }
    if (!data)
    {
        *out_exit = FALSE;
        return 0;
    }

    if (!verify_parent_process())
    {
        HeapFree(GetProcessHeap(), 0, data);
        *out_exit = TRUE;
        return 1;
    }

    data_intl = (ds_data_intl*)data;
    data_intl->callback.ulongptr += (ULONG_PTR)double_spawn_main_hook;
    event = data_intl->event;
    ret = data_intl->callback.funcptr(data + sizeof(ds_data_intl), data_size - sizeof(ds_data_intl));

    HeapFree(GetProcessHeap(), 0, data);
    *out_exit = TRUE;
    return ret;
}

ATTR_NODISCARD_CXX("unnecessary get_hex_digit call")
static inline TCHAR get_hex_digit(unsigned char const val) ATTR_NOTHROW_CXX
ATTR_CONST ATTR_ALWAYS_INLINE ATTR_NOTHROW ATTR_NODISCARD("unnecessary get_hex_digit call");

static inline TCHAR get_hex_digit(unsigned char const val) ATTR_NOTHROW_CXX
{
    static TCHAR const hex_digits[] = _T("0123456789ABCDEF");

    return hex_digits[val];
}

static void encode_envvar(ds_data_intl const data, BYTE const* aux_data, size_t aux_data_size,
                          TCHAR* out_envvar_content)
{
    size_t i;

    for (i = 0; i < sizeof(ds_data_intl); ++i)
    {
        BYTE val;

        val = ((BYTE*)&data)[i];
        *out_envvar_content++ = get_hex_digit((val >> 4) & 0xF);
        *out_envvar_content++ = get_hex_digit(val & 0xF);
    }
    while (aux_data_size--)
    {
        BYTE val;

        val = *aux_data++;
        *out_envvar_content++ = get_hex_digit((val >> 4) & 0xF);
        *out_envvar_content++ = get_hex_digit(val & 0xF);
    }
    *out_envvar_content = _T('\0');
}

static BOOL set_data(logger_instance* const logger, ds_data_intl const data, BYTE const* aux_data, size_t aux_data_size)
{
    TCHAR* envvar_content;
    BOOL ret;

    envvar_content = (TCHAR*)HeapAlloc(GetProcessHeap(), 0,
                                       ((sizeof(ds_data_intl) + aux_data_size) * 2 + 1) * sizeof(TCHAR));
    if (envvar_content == NULL)
    {
        LOG_CRITICAL(logger, (
            _T("Failed to allocate %lu bytes"),
            (unsigned long)(((sizeof(ds_data_intl) + aux_data_size) * 2 + 1) * sizeof(TCHAR))
        ));
        return FALSE;
    }

    encode_envvar(data, aux_data, aux_data_size, envvar_content);

    ret = SetEnvironmentVariable(DOUBLE_SPAWN_ENVVAR_NAME, envvar_content) != 0;
    if (!ret)
        LOG_CRITICAL(logger, (
            _T("Failed to set environment variable: Error %d"),
            GetLastError()
        ));

    HeapFree(GetProcessHeap(), 0, envvar_content);
    return ret;
}

static BOOL spawn_process(logger_instance* const logger, HANDLE const event)
{
    STARTUPINFO sinfo;
    PROCESS_INFORMATION procinfo;
    DWORD status;
    HANDLE wait_handles[2];
    DWORD err;

    RtlZeroMemory(&sinfo, sizeof(sinfo));
    sinfo.cb = sizeof(sinfo);
    status = CreateProcess(NULL, GetCommandLine(), NULL, NULL, TRUE, BELOW_NORMAL_PRIORITY_CLASS, NULL, NULL, &sinfo,
                           &procinfo);
    if (!status)
    {
        LOG_CRITICAL(logger, (_T("Could not start process: Error %d"), GetLastError()));
        return FALSE;
    }
    CloseHandle(procinfo.hThread);

    wait_handles[0] = event;
    wait_handles[1] = procinfo.hProcess;
    do {
        status = WaitForMultipleObjects(sizeof(wait_handles) / sizeof(wait_handles[0]), wait_handles, FALSE, INFINITE);
    } while (status == WAIT_TIMEOUT);
    err = GetLastError();
    CloseHandle(procinfo.hProcess);
    switch (status)
    {
        case WAIT_OBJECT_0:
        case WAIT_OBJECT_0 + 1:
            return TRUE;
        case WAIT_FAILED:
            LOG_ERROR(logger, (_T("WaitForMultipleObjects failed: Error %d"), err));
            break;
        default:
            LOG_ERROR(logger, (_T("Unexpected WaitForMultipleObjects return value: %d"), status));
    }
    return FALSE;
}

BOOL double_spawn_fork(logger_instance* const logger, double_spawn_callback const callback, void const* aux_data,
                       size_t const aux_data_size)
{
    ds_data_intl data_intl;
    SECURITY_ATTRIBUTES sattrs;
    BOOL ret;

    data_intl.callback.funcptr = callback;
    data_intl.callback.ulongptr -= (ULONG_PTR)double_spawn_main_hook;

    sattrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    sattrs.lpSecurityDescriptor = NULL;
    sattrs.bInheritHandle = TRUE;
    data_intl.event = CreateEvent(&sattrs, TRUE, FALSE, NULL);
    if (!data_intl.event)
        return FALSE;

    if (!set_data(logger, data_intl, (BYTE const*)aux_data, aux_data_size))
    {
        CloseHandle(data_intl.event);
        return FALSE;
    }

    ret = spawn_process(logger, data_intl.event);

    CloseHandle(data_intl.event);
    return ret;
}

void double_spawn_exit_parent(logger_instance* const logger)
{
    LOG_TRACE(logger, (_T("Signaling double-spawn parent to exit")));

    if (event == INVALID_HANDLE_VALUE)
    {
        LOG_ERROR(logger, (_T("Could not signal parent process to exit: No event handle")));
        return;
    }

    if (!SetEvent(event))
        LOG_ERROR(logger, (_T("Could not signal double-spawn parent process: Error %d"), GetLastError()));
    if (!CloseHandle(event))
        LOG_ERROR(logger, (_T("Could not close double-spawn event handle: Error %d"), GetLastError()));
    event = INVALID_HANDLE_VALUE;

    LOG_TRACE(logger, (_T("Signaled double-spawn parent to exit")));
}
