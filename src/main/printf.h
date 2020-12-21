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
#ifndef __WINESTREAMPROXY_MAIN_PRINTF_H__
#define __WINESTREAMPROXY_MAIN_PRINTF_H__

#ifdef __WINE__

#include <stdio.h>

#include <windef.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

extern int WINAPIV fake_printf(char const* format, ...);
#ifdef printf
#undef printf
#endif
#define printf fake_printf

extern int WINAPIV fake_fprintf(FILE* stream, char const* format, ...);
#ifdef fprintf
#undef fprintf
#endif
#define fprintf fake_fprintf

extern int WINAPIV fake_wprintf(wchar_t const* format, ...);
#ifdef wprintf
#undef wprintf
#endif
#define wprintf fake_wprintf

extern int WINAPIV fake_fwprintf(FILE* stream, wchar_t const* format, ...);
#ifdef fwprintf
#undef fwprintf
#endif
#define fwprintf fake_fwprintf

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* defined(__WINE__) */

#endif /* !defined(__WINESTREAMPROXY_MAIN_PRINTF_H__) */
