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
#ifndef __WINESTREAMPROXY_PROXY_PROXY_H__
#define __WINESTREAMPROXY_PROXY_PROXY_H__

#include "data/proxy_data.h"
#include <winestreamproxy/logger.h>
#include <winestreamproxy/winestreamproxy.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

extern BOOL proxy_create(logger_instance* logger, proxy_parameters parameters, proxy_data** out_proxy);
extern void proxy_enter_loop(proxy_data* proxy);
extern void proxy_destroy(proxy_data* proxy);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_PROXY_H__) */
