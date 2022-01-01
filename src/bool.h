/* Copyright (C) 2021 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#pragma once
#ifndef __WINESTREAMPROXY_MAIN_BOOL_H__
#define __WINESTREAMPROXY_MAIN_BOOL_H__

#ifndef __cplusplus
#if __STDC_VERSION__ >= 199901L
#include <stdbool.h> /* bool */
#else
typedef enum { false = 0, true = !false } bool;
#endif
#endif

#endif /* !defined(__WINESTREAMPROXY_MAIN_BOOL_H__) */
