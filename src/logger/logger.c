/* Copyright (C) 2020-2021 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include <winestreamproxy/logger.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#ifndef __WINE__
#include <fibersapi.h>
#endif
#include <windef.h>
#include <winbase.h>
#include <winnls.h>

DWORD fls_index;
BOOL fls_index_initialized = FALSE;

typedef struct fiber_local_data {
    BOOL do_log;
    LOG_LEVEL level;
    void const* file;
    unsigned char file_char_size;
    void const* converted_file;
    long line;
    logger_instance* logger;
} fiber_local_data;

void CALLBACK fls_callback(PVOID lpFlsData)
{
    if (lpFlsData)
        HeapFree(GetProcessHeap(), 0, lpFlsData);
}

int log_create_logger(log_message_callback const log_message, unsigned char const character_size,
                      logger_instance** const out_logger)
{
    logger_instance* logger;

    if (character_size != sizeof(char) && character_size != sizeof(wchar_t))
        return 0;

    if (!fls_index_initialized)
    {
        fls_index = FlsAlloc(fls_callback);
        if (fls_index == FLS_OUT_OF_INDEXES)
            return 0;
        fls_index_initialized = TRUE;
    }

    logger = (logger_instance*)HeapAlloc(GetProcessHeap(), 0, sizeof(logger_instance));
    if (logger == NULL)
        return 0;

    logger->log_message = log_message;
    logger->character_size = character_size;
    logger->impl_data = 0;
    logger->enabled_log_levels = 0x3C;

    *out_logger = logger;
    return 1;
}

void log_destroy_logger(logger_instance* const logger)
{
    if (logger)
        HeapFree(GetProcessHeap(), 0, logger);
}

static int log_init_message_impl2(logger_instance* const logger, LOG_LEVEL const level, void const* const file,
                                  unsigned char const file_char_size, long const line)
{
    fiber_local_data* fls_data;

    if (!fls_index_initialized)
        return 0;

    fls_data = (fiber_local_data*)FlsGetValue(fls_index);
    if (fls_data == NULL || fls_data == 0)
    {
        if (GetLastError() != ERROR_SUCCESS)
            return 0;
        fls_data = (fiber_local_data*)HeapAlloc(GetProcessHeap(), 0, sizeof(fiber_local_data));
        if (fls_data == NULL)
            return 0;
        fls_data->converted_file = 0;
        if (!FlsSetValue(fls_index, fls_data))
            return 0;
    }

    if (fls_data->converted_file)
        HeapFree(GetProcessHeap(), 0, (LPVOID)fls_data->converted_file);

    if (LOG_IS_ENABLED(logger, level))
    {
        fls_data->logger = logger;
        fls_data->level = level;
        fls_data->file = file;
        fls_data->file_char_size = file_char_size;
        fls_data->converted_file = 0;
        fls_data->line = line;
        fls_data->do_log = TRUE;
    }
    else
        fls_data->do_log = FALSE;

    return 1;
}

static int log_init_message_impl(logger_instance* const logger, LOG_LEVEL const level, void const* const file,
                                 unsigned char const file_char_size, long const line)
{
    DWORD last_error;
    int ret;

    last_error = GetLastError();
    ret = log_init_message_impl2(logger, level, file, file_char_size, line);
    SetLastError(last_error);

    return ret;
}

int log_init_message(logger_instance* const logger, LOG_LEVEL const level, char const* const file, long const line)
{
    return log_init_message_impl(logger, level, file, sizeof(char), line);
}

int log_print_message(char const* const format, ...)
{
    fiber_local_data* fls_data;
    va_list args;
    int message_len;
    char* message;
    int ret;

    if (!fls_index_initialized)
        return 0;

    fls_data = (fiber_local_data*)FlsGetValue(fls_index);
    if (fls_data == NULL || fls_data == 0)
        return 0;

    if (!fls_data->do_log)
        return 1;

    va_start(args, format);
    message_len = vsnprintf(0, 0, format, args);
    va_end(args);
    if (message_len < 0)
    {
        va_end(args);
        return 0;
    }

    message = (char*)HeapAlloc(GetProcessHeap(), 0, sizeof(char) * (message_len + 1));
    if (message == NULL)
    {
        va_end(args);
        return 0;
    }

    va_start(args, format);
    if (vsnprintf(message, message_len + 1, format, args) != message_len)
    {
        va_end(args);
        HeapFree(GetProcessHeap(), 0, message);
        return 0;
    }
    va_end(args);

    ret = 0;

    if (fls_data->logger->character_size == sizeof(char))
        ret = fls_data->logger->log_message(fls_data->logger, fls_data->level, message);
    else if (fls_data->logger->character_size == sizeof(wchar_t))
        do {
            int wide_len;
            LPWSTR wide_message;

            wide_len = MultiByteToWideChar(CP_UTF8, 0, message, message_len, NULL, 0);
            if (wide_len == 0)
                break;

            wide_message = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR) * (wide_len + 1));
            if (wide_message == NULL)
                break;

            if (MultiByteToWideChar(CP_UTF8, 0, message, message_len, wide_message, wide_len) != wide_len)
            {
                HeapFree(GetProcessHeap(), 0, wide_message);
                break;
            }

            wide_message[wide_len] = '\0';

            ret = fls_data->logger->log_message(fls_data->logger, fls_data->level, wide_message);

            HeapFree(GetProcessHeap(), 0, wide_message);
        } while (0);

    HeapFree(GetProcessHeap(), 0, message);
    return ret;
}

#ifdef __cplusplus
extern "C"
#endif /* defined(__cplusplus) */
int wlog_init_message(logger_instance* const logger, LOG_LEVEL const level, wchar_t const* const file, long const line)
{
    return log_init_message_impl(logger, level, file, sizeof(wchar_t), line);
}

#ifdef __cplusplus
extern "C"
#endif /* defined(__cplusplus) */
int wlog_print_message(wchar_t const* const format, ...)
{
    fiber_local_data* fls_data;
    size_t buffer_size;
    wchar_t* message;
    va_list args;
    int message_len;
    int ret;

    if (!fls_index_initialized)
        return 0;

    fls_data = (fiber_local_data*)FlsGetValue(fls_index);
    if (fls_data == NULL || fls_data == 0)
        return 0;

    if (!fls_data->do_log)
        return 1;

    buffer_size = 240;

    while (TRUE)
    {
        message = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * buffer_size);
        if (message == NULL)
            return 0;

        va_start(args, format);
        message_len = _vsnwprintf(message, buffer_size, format, args);
        va_end(args);

        if (message_len >= 0)
            break;

        HeapFree(GetProcessHeap(), 0, message);
        buffer_size = buffer_size * 2 + 16;
    }

    ret = 0;

    if (fls_data->logger->character_size == sizeof(wchar_t))
        ret = fls_data->logger->log_message(fls_data->logger, fls_data->level, message);
    else if (fls_data->logger->character_size == sizeof(char))
        do {
            int narrow_len;
            LPSTR narrow_message;

            narrow_len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)message, message_len, NULL, 0, NULL, NULL);
            if (narrow_len == 0)
                break;

            narrow_message = (LPSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR) * (narrow_len + 1));
            if (narrow_message == NULL)
                break;

            if (WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)message, message_len, narrow_message, narrow_len, NULL, NULL)
                != narrow_len)
            {
                HeapFree(GetProcessHeap(), 0, narrow_message);
                break;
            }

            narrow_message[narrow_len] = '\0';

            ret = fls_data->logger->log_message(fls_data->logger, fls_data->level, narrow_message);

            HeapFree(GetProcessHeap(), 0, narrow_message);
        } while (0);

    HeapFree(GetProcessHeap(), 0, message);
    return ret;
}

void log_get_file_and_line(void const** const file, long* const line)
{
    fiber_local_data* fls_data;

    *file = L"";
    *line = -1;

    if (!fls_index_initialized)
        return;

    fls_data = (fiber_local_data*)FlsGetValue(fls_index);
    if (fls_data == NULL || fls_data == 0)
        return;

    *line = fls_data->line;

    if (fls_data->file_char_size == fls_data->logger->character_size)
        *file = fls_data->file;
    else if (fls_data->converted_file)
        *file = fls_data->converted_file;
    else if (fls_data->file_char_size == sizeof(char))
    {
        int wide_len;
        LPWSTR wide_file;

        *file = L"<error>";

        wide_len = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)fls_data->file, -1, NULL, 0) - 1;
        if (wide_len == 0)
            return;

        wide_file = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR) * (wide_len + 1));
        if (wide_file == NULL)
            return;

        if (MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)fls_data->file, -1, wide_file, wide_len) - 1 != wide_len)
        {
            HeapFree(GetProcessHeap(), 0, wide_file);
            return;
        }

        wide_file[wide_len] = '\0';

        fls_data->converted_file = wide_file;
        *file = wide_file;
    }
    else if (fls_data->file_char_size == sizeof(wchar_t))
    {
        int narrow_len;
        LPSTR narrow_file;

        *file = "<error>";

        narrow_len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)fls_data->file, -1, NULL, 0, NULL, NULL) - 1;
        if (narrow_len == 0)
            return;

        narrow_file = (LPSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR) * (narrow_len + 1));
        if (narrow_file == NULL)
            return;

        if (WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)fls_data->file, -1, narrow_file, narrow_len, NULL, NULL) - 1
            != narrow_len)
        {
            HeapFree(GetProcessHeap(), 0, narrow_file);
            return;
        }

        narrow_file[narrow_len] = '\0';

        fls_data->converted_file = narrow_file;
        *file = narrow_file;
    }
}
