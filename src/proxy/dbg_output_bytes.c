/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "dbg_output_bytes.h"
#include <winestreamproxy/logger.h>

#include <stddef.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

void dbg_output_bytes(logger_instance* const logger, TCHAR const* const prefix,
                      char const* const bytes, size_t const count)
{
    TCHAR* hex_str;
    size_t i;

    LOG_TRACE(logger, (_T("Outputting array as hex string")));

    hex_str = (TCHAR*)HeapAlloc(GetProcessHeap(), 0, sizeof(TCHAR) * (2 * count + 1));
    if (!hex_str)
    {
        LOG_DEBUG(logger, (_T("Could not allocate memory for hex string")));
        return;
    }

    for (i = 0; i < count; ++i)
    {
        if ((unsigned char)bytes[i] < 0xA0)
            hex_str[i * 2] = (((unsigned char)bytes[i] >> 4) & 0x0F) + _T('0');
        else
            hex_str[i * 2] = (((unsigned char)bytes[i] >> 4) & 0x0F) - 10 + _T('A');
        if ((bytes[i] & 0x0F) < 0x0A)
            hex_str[i * 2 + 1] = (bytes[i] & 0x0F) + _T('0');
        else
            hex_str[i * 2 + 1] = (bytes[i] & 0x0F) - 10 + _T('A');
    }
    hex_str[count * 2] = _T('\0');

    LOG_DEBUG(logger, (_T("%s%s"), prefix, hex_str));

    HeapFree(GetProcessHeap(), 0, hex_str);

    LOG_TRACE(logger, (_T("Output array as hex string")));
}
