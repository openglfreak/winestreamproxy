/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#pragma once
#ifndef __WINESTREAMPROXY_MAIN_MISC_H__
#define __WINESTREAMPROXY_MAIN_MISC_H__

#include <winestreamproxy/logger.h>

#ifdef _UNICODE
#include <windef.h>
#include <winnt.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

extern void lower_process_priority(logger_instance* logger);

#ifdef _UNICODE
extern LPSTR wide_to_narrow(logger_instance* logger, LPCWCH wide_path);
#endif

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_MAIN_MISC_H__) */
