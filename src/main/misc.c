
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
