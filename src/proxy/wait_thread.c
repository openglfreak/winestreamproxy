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
#include "wait_thread.h"

#include <tchar.h>
#include <windef.h>
#include <winbase.h>

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
            return 0;
        default:
            LOG_ERROR(logger, (
                _T("Unexpected return value from WaitForSingleObject: Ret %d Error %d"),
                wait_result,
                GetLastError()
            ));
            return 0;
    }

    LOG_TRACE(logger, (_T("Waiting for %s thread exit finished"), name));

    return 1;
}
