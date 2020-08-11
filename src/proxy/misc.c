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

#include <stddef.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

BOOL wait_for_thread(logger_instance* const logger, TCHAR const* const name, HANDLE const thread)
{
    DWORD wait_result;

    LOG_TRACE(logger, (_T("Waiting for %s thread to exit"), name));

    do {
        wait_result = WaitForSingleObject(thread, INFINITE);
    } while (wait_result == WAIT_TIMEOUT);

    switch (wait_result)
    {
        case WAIT_OBJECT_0:
            break;
        case WAIT_FAILED:
            LOG_ERROR(logger, (_T("Waiting for %s thread exit failed: Error %d"), name, GetLastError()));
            return FALSE;
        default:
            LOG_ERROR(logger, (
                _T("Unexpected return value from WaitForSingleObject: Ret %d Error %d"),
                wait_result,
                GetLastError()
            ));
            return FALSE;
    }

    LOG_TRACE(logger, (_T("Waiting for %s thread exit finished"), name));

    return TRUE;
}

static void make_hex_string(unsigned char const* bytes, size_t count, TCHAR* out_hex_str)
{
    for (; count-- > 0; ++bytes)
    {
        unsigned char byte;

        byte = *bytes & 0xFF;
        if (byte < 0xA0)
            *out_hex_str = (byte >> 4) + _T('0');
        else
            *out_hex_str = (byte >> 4) - 10 + _T('A');
        ++out_hex_str;
        byte &= 0x0F;
        if (byte < 0x0A)
            *out_hex_str = byte + _T('0');
        else
            *out_hex_str = byte - 10 + _T('A');
        ++out_hex_str;
    }
    *out_hex_str = _T('\0');
}

void dbg_output_bytes(logger_instance* const logger, TCHAR const* const prefix,
                      unsigned char const* const bytes, size_t const count)
{
    TCHAR* hex_str;

    LOG_TRACE(logger, (_T("Outputting array as hex string")));

    hex_str = (TCHAR*)HeapAlloc(GetProcessHeap(), 0, sizeof(TCHAR) * (2 * count + 1));
    if (!hex_str)
    {
        LOG_DEBUG(logger, (_T("Could not allocate memory for hex string")));
        return;
    }
    make_hex_string(bytes, count, hex_str);
    LOG_DEBUG(logger, (_T("%s%s"), prefix, hex_str));
    HeapFree(GetProcessHeap(), 0, hex_str);

    LOG_TRACE(logger, (_T("Output array as hex string")));
}
