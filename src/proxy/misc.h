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
#ifndef __WINESTREAMPROXY_PROXY_MISC_H__
#define __WINESTREAMPROXY_PROXY_MISC_H__

#include <winestreamproxy/logger.h>

#include <stddef.h>

#include <windef.h>
#include <winnt.h>

#ifndef offsetof
#define offsetof(type, member) ((size_t)((char*)&((type*)0)->member - (char*)0))
#endif
#ifndef container_of
#define container_of(ptr, type, member) ((type*)((char*)ptr - offsetof(type, member)))
#endif

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

extern void dbg_output_bytes(logger_instance* logger, TCHAR const* prefix, unsigned char const* bytes, size_t count);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_MISC_H__) */
