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
#include "../bool.h"
#include <winestreamproxy/logger.h>

#include <stddef.h>
#include <wchar.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <psapi.h>
#include <tlhelp32.h>

#define add_funcptrs(type, a, b) ((type)((ULONG_PTR)(a) + (ULONG_PTR)(b)))
#define subtract_funcptrs(type, a, b) ((type)((ULONG_PTR)(a) - (ULONG_PTR)(b)))

HANDLE double_spawn_parent_notify_event = INVALID_HANDLE_VALUE;

static DWORD get_parent_process_id(DWORD target_process_id)
{
    HANDLE tool_help_snapshot;
    BOOL b;
    PROCESSENTRY32 process;

    tool_help_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (tool_help_snapshot == INVALID_HANDLE_VALUE)
        return 0;

    process.dwSize = sizeof(PROCESSENTRY32);
    for (b = Process32First(tool_help_snapshot, &process); b; b = Process32Next(tool_help_snapshot, &process))
        if (process.th32ProcessID == target_process_id)
            break;

    CloseHandle(tool_help_snapshot);
    return b ? process.th32ParentProcessID : 0;
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
    size_t aux_data_size;
} ds_data_intl;

DWORD __stdcall double_spawn_main(ds_data_intl* const data)
{
    double_spawn_callback callback;
    int ret;

    if (!verify_parent_process(data->parent_process_handle))
    {
        HeapFree(GetProcessHeap(), 0, data);
        ret = 1;
        goto done;
    }
    CloseHandle(data->parent_process_handle);

    double_spawn_parent_notify_event = data->parent_notify_event;
    callback = add_funcptrs(double_spawn_callback, data->callback, &double_spawn_main);
    ret = callback(data + 1, data->aux_data_size);

done:
    VirtualFree(data, 0, MEM_RELEASE);
    return ret;
}

static bool spawn_process(logger_instance* const logger, ds_data_intl const* const data_intl,
                          void const* const aux_data)
{
    STARTUPINFO sinfo;
    PROCESS_INFORMATION procinfo;
    DWORD status;
    LPVOID child_memory;
    SIZE_T written;
    CONTEXT ctx;
    HANDLE wait_handles[2];
    DWORD err;

    RtlZeroMemory(&sinfo, sizeof(sinfo));
    sinfo.cb = sizeof(sinfo);
    status = CreateProcess(NULL, GetCommandLine(), NULL, NULL, TRUE, BELOW_NORMAL_PRIORITY_CLASS | CREATE_SUSPENDED,
                           NULL, NULL, &sinfo, &procinfo);
    if (!status)
    {
        LOG_CRITICAL(logger, (_T("Could not start process: Error %d"), GetLastError()));
        return false;
    }

    child_memory = VirtualAllocEx(procinfo.hProcess, NULL, sizeof(*data_intl) + data_intl->aux_data_size,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!child_memory)
    {
        LOG_CRITICAL(logger, (_T("Could not allocate memory in child process: Error %d"), GetLastError()));
        goto err_child_modify;
    }
    if (!WriteProcessMemory(procinfo.hProcess, child_memory, data_intl, sizeof(*data_intl), &written)
        || written != sizeof(*data_intl)
        || !WriteProcessMemory(procinfo.hProcess, (char*)child_memory + sizeof(*data_intl), aux_data,
                               data_intl->aux_data_size, &written)
        || written != data_intl->aux_data_size)
    {
        LOG_CRITICAL(logger, (_T("Could not write to memory in child process: Error %d"), GetLastError()));
        goto err_child_modify;
    }

    ctx.ContextFlags = CONTEXT_INTEGER;
    if (!GetThreadContext(procinfo.hThread, &ctx))
    {
        LOG_CRITICAL(logger, (_T("Could not get thread context of child process: Error %d"), GetLastError()));
        goto err_child_modify;
    }
#ifdef __x86_64__
    ctx.Rcx = (DWORD64)double_spawn_main;
    ctx.Rdx = (DWORD64)child_memory;
#elif defined(__i386__)
    ctx.Eax = (DWORD)double_spawn_main;
    ctx.Ebx = (DWORD)child_memory;
#else
#error "Unsupported architecture"
#endif
    if (!SetThreadContext(procinfo.hThread, &ctx) || ResumeThread(procinfo.hThread) == (DWORD)-1)
    {
        LOG_CRITICAL(logger, (_T("Could not set thread context of child process: Error %d"), GetLastError()));
        goto err_child_modify;
    }
    CloseHandle(procinfo.hThread);

    wait_handles[0] = data_intl->parent_notify_event;
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
            return true;
        case WAIT_FAILED:
            LOG_ERROR(logger, (_T("WaitForMultipleObjects failed: Error %d"), err));
            break;
        default:
            LOG_ERROR(logger, (_T("Unexpected WaitForMultipleObjects return value: %d"), status));
    }
    return false;

err_child_modify:
    CloseHandle(procinfo.hThread);
    TerminateProcess(procinfo.hProcess, 1);
    CloseHandle(procinfo.hProcess);
    return false;
}

static HANDLE get_current_process_handle(void)
{
    HANDLE currproc = GetCurrentProcess(), handle;
    if (!DuplicateHandle(currproc, currproc, currproc, &handle, 0, TRUE, DUPLICATE_SAME_ACCESS))
        return INVALID_HANDLE_VALUE;
    return handle;
}

bool double_spawn_fork(logger_instance* const logger, double_spawn_callback const callback, void const* aux_data,
                       size_t const aux_data_size)
{
    ds_data_intl data_intl;
    SECURITY_ATTRIBUTES sattrs;
    bool ret;

    data_intl.parent_process_handle = get_current_process_handle();
    if (data_intl.parent_process_handle == INVALID_HANDLE_VALUE)
        return false;

    sattrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    sattrs.lpSecurityDescriptor = NULL;
    sattrs.bInheritHandle = TRUE;
    data_intl.parent_notify_event = CreateEvent(&sattrs, TRUE, FALSE, NULL);
    if (!data_intl.parent_notify_event)
    {
        CloseHandle(data_intl.parent_process_handle);
        return false;
    }

    data_intl.callback = subtract_funcptrs(double_spawn_callback, callback, &double_spawn_main);
    data_intl.aux_data_size = aux_data_size;

    ret = spawn_process(logger, &data_intl, aux_data);

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
