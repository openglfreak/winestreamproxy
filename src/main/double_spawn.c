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
#include <winestreamproxy/logger.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <wincon.h>
#include <psapi.h>
#include <tlhelp32.h>

#define DOUBLE_SPAWN_ENVVAR_NAME _T("__WINESTREAMPROXY_DOUBLE_SPAWN_DATA")

#define add_funcptrs(type, a, b) ((type)((ULONG_PTR)(a) + (ULONG_PTR)(b)))
#define subtract_funcptrs(type, a, b) ((type)((ULONG_PTR)(a) - (ULONG_PTR)(b)))

HANDLE double_spawn_parent_notify_event = INVALID_HANDLE_VALUE;

static unsigned char parse_hex_digit(TCHAR const chr)
{
    return chr >= _T('0') && chr <= _T('9') ? chr - _T('0')
           : chr >= _T('A') && chr <= _T('F') ? chr - _T('A') + 10
           : chr >= _T('a') && chr <= _T('f') ? chr - _T('a') + 10
           : 0xFF;
}

static TCHAR get_hex_digit(unsigned char const val)
{
    static TCHAR hex_digits[] = { _T('0'), _T('1'), _T('2'), _T('3'), _T('4'), _T('5'), _T('6'), _T('7'), _T('8'),
                                  _T('9'), _T('A'), _T('B'), _T('C'), _T('D'), _T('E'), _T('F') };
    return hex_digits[val];
}

static BOOL decode_hex_string(TCHAR const* input, BYTE* output, size_t size_bytes)
{
    while (size_bytes--)
    {
        unsigned char high_nibble, low_nibble;

        high_nibble = parse_hex_digit(*input++);
        if (high_nibble > 0xF)
            return FALSE;
        low_nibble = parse_hex_digit(*input++);
        if (low_nibble > 0xF)
            return FALSE;
        *output++ = (high_nibble << 4) | low_nibble;
    }

    return TRUE;
}

static void encode_hex_string(BYTE const* input, TCHAR* output, size_t size_bytes)
{
    while (size_bytes--)
    {
        BYTE val;

        val = *input++;
        *output++ = get_hex_digit((val >> 4) & 0xF);
        *output++ = get_hex_digit(val & 0xF);
    }
    *output = _T('\0');
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

    if (length == 0)
    {
        DWORD err;

        err = GetLastError();
        if (content)
            HeapFree(GetProcessHeap(), 0, content);
        if (err == ERROR_ENVVAR_NOT_FOUND)
            content = 0;
        else
            return FALSE;
    }
    else
    {
        TCHAR* new_content;

        new_content = (TCHAR*)HeapReAlloc(GetProcessHeap(), 0, content, length);
        if (new_content)
            content = new_content;
    }

    *out_content = content;
    *out_length = length;
    return TRUE;
}

static BOOL get_and_decode_envvar(TCHAR const* const name, BYTE** const out_data, size_t* const out_data_size)
{
    TCHAR* envvar_content;
    size_t envvar_content_length;
    size_t decoded_content_size;
    BYTE* decoded_content;

    if (!get_envvar(name, &envvar_content, &envvar_content_length))
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

    if (!decode_hex_string(envvar_content, decoded_content, decoded_content_size))
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

static DWORD get_parent_process_id(DWORD target_process_id)
{
    HANDLE tool_help_snapshot;
    BOOL b;
    PROCESSENTRY32 process;
    DWORD ret;

    tool_help_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (tool_help_snapshot == INVALID_HANDLE_VALUE)
        return 0;

    process.dwSize = sizeof(PROCESSENTRY32);
    ret = 0;
    for (b = Process32First(tool_help_snapshot, &process); b; b = Process32Next(tool_help_snapshot, &process))
        if (process.th32ProcessID == target_process_id)
        {
            ret = process.th32ParentProcessID;
            break;
        }

    CloseHandle(tool_help_snapshot);
    return ret;
}

static BOOL verify_parent_process(HANDLE process_handle)
{
    DWORD parent_process_id;
    size_t child_process_path_length;
    TCHAR child_process_path[MAX_PATH];
    TCHAR parent_process_path[MAX_PATH];

    parent_process_id = GetProcessId(process_handle);
    if (!parent_process_id || parent_process_id != get_parent_process_id(GetCurrentProcessId()))
        return FALSE;

    child_process_path_length = GetProcessImageFileName(GetCurrentProcess(), child_process_path, MAX_PATH);
    if (child_process_path_length == 0)
        return FALSE;

    if (GetProcessImageFileName(process_handle, parent_process_path, child_process_path_length + 1)
        != child_process_path_length)
        return FALSE;
    if (memcmp(child_process_path, parent_process_path, (child_process_path_length + 1) * sizeof(TCHAR)) != 0)
        return FALSE;

    return TRUE;
}

typedef struct ds_data_intl {
    HANDLE parent_process_handle;
    HANDLE parent_notify_event;
    double_spawn_callback callback;
} ds_data_intl;

int double_spawn_main_hook(BOOL* const out_exit)
{
    BYTE* data;
    size_t data_size;
    double_spawn_callback callback;
    int ret;

    if (!get_and_decode_envvar(DOUBLE_SPAWN_ENVVAR_NAME, &data, &data_size))
    {
        *out_exit = TRUE;
        return 1;
    }
    if (!data)
    {
        *out_exit = FALSE;
        return 0;
    }

    if (!verify_parent_process(((ds_data_intl*)data)->parent_process_handle))
    {
        HeapFree(GetProcessHeap(), 0, data);
        *out_exit = FALSE;
        return 0;
    }
    CloseHandle(((ds_data_intl*)data)->parent_process_handle);

    double_spawn_parent_notify_event = ((ds_data_intl*)data)->parent_notify_event;
    callback = add_funcptrs(double_spawn_callback, ((ds_data_intl*)data)->callback, &double_spawn_main_hook);

    ret = callback(data + sizeof(ds_data_intl), data_size - sizeof(ds_data_intl));

    HeapFree(GetProcessHeap(), 0, data);
    *out_exit = TRUE;
    return ret;
}

static BOOL set_ds_data(logger_instance* const logger, ds_data_intl const data, BYTE const* aux_data, size_t aux_data_size)
{
    size_t envvar_content_length;
    TCHAR* envvar_content;
    BOOL ret;

    envvar_content_length = ((sizeof(ds_data_intl) + aux_data_size) * 2 + 1);
    envvar_content = (TCHAR*)HeapAlloc(GetProcessHeap(), 0, envvar_content_length * sizeof(TCHAR));
    if (envvar_content == NULL)
    {
        LOG_CRITICAL(logger, (
            _T("Failed to allocate %lu bytes"),
            (unsigned long)(envvar_content_length * sizeof(TCHAR))
        ));
        return FALSE;
    }

    encode_hex_string((BYTE*)&data, envvar_content, sizeof(ds_data_intl));
    encode_hex_string(aux_data, &envvar_content[sizeof(ds_data_intl) * 2], aux_data_size);

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

    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(),
                         &data_intl.parent_process_handle, 0, TRUE, DUPLICATE_SAME_ACCESS))
        return FALSE;

    sattrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    sattrs.lpSecurityDescriptor = NULL;
    sattrs.bInheritHandle = TRUE;
    data_intl.parent_notify_event = CreateEvent(&sattrs, TRUE, FALSE, NULL);
    if (!data_intl.parent_notify_event)
    {
        CloseHandle(data_intl.parent_process_handle);
        return FALSE;
    }

    data_intl.callback = subtract_funcptrs(double_spawn_callback, callback, &double_spawn_main_hook);

    ret = set_ds_data(logger, data_intl, (BYTE const*)aux_data, aux_data_size)
          && spawn_process(logger, data_intl.parent_notify_event);

    CloseHandle(data_intl.parent_notify_event);
    CloseHandle(data_intl.parent_process_handle);
    return ret;
}

void double_spawn_exit_parent(logger_instance* const logger)
{
    LOG_TRACE(logger, (_T("Signaling double-spawn parent to exit")));

    if (double_spawn_parent_notify_event == INVALID_HANDLE_VALUE)
    {
        LOG_ERROR(logger, (_T("Could not signal parent process to exit: No event handle")));
        return;
    }

    if (!SetEvent(double_spawn_parent_notify_event))
        LOG_ERROR(logger, (_T("Could not signal double-spawn parent process: Error %d"), GetLastError()));

    if (!CloseHandle(double_spawn_parent_notify_event))
        LOG_ERROR(logger, (_T("Could not close double-spawn event handle: Error %d"), GetLastError()));
    double_spawn_parent_notify_event = INVALID_HANDLE_VALUE;

    LOG_TRACE(logger, (_T("Signaled double-spawn parent to exit")));
}
