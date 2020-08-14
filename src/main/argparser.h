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
#ifndef __WINESTREAMPROXY_MAIN_ARGPARSER_H__
#define __WINESTREAMPROXY_MAIN_ARGPARSER_H__

#include <winestreamproxy/logger.h>

#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef enum ARGPARSER_OPTION_TYPE {
    ARGPARSER_OPTION_TYPE_PRESENCE = 1,
    ARGPARSER_OPTION_TYPE_STRING,
    ARGPARSER_OPTION_TYPE_BOOLEAN,
    ARGPARSER_OPTION_TYPE_INTEGER,
    ARGPARSER_OPTION_TYPE_ACCUMULATOR,
    ARGPARSER_OPTION_TYPE_DECIMAL
} ARGPARSER_OPTION_TYPE;

typedef int (* argparser_option_validate_callback)(void* value);

typedef struct argparser_option_list_entry {
    TCHAR const*                        short_option;
    TCHAR const*                        long_option;
    ARGPARSER_OPTION_TYPE               type;
    argparser_option_validate_callback  validate;
    size_t                              value_offset;
} argparser_option_list_entry;

typedef struct argparser_data {
    argparser_option_list_entry const*  option_list;
    void*                               option_values;
    TCHAR const***                      positionals;
    size_t*                             positionals_count;
} argparser_data;

typedef enum ARGPARSER_PARSE_RETURN_CODE {
    ARGPARSER_PARSE_RETURN_SUCCESS,
    ARGPARSER_PARSE_RETURN_UNKNOWN_OPTION,
    ARGPARSER_PARSE_RETURN_EXTRANEOUS_VALUE,
    ARGPARSER_PARSE_RETURN_MISSING_VALUE,
    ARGPARSER_PARSE_RETURN_MALFORMED_VALUE,
    ARGPARSER_PARSE_RETURN_VALIDATE_ERROR,
    ARGPARSER_PARSE_RETURN_OUT_OF_MEMORY,
    ARGPARSER_PARSE_RETURN_OTHER_ERROR
} ARGPARSER_PARSE_RETURN_CODE;

typedef struct ARGPARSER_PARSE_RETURN {
    ARGPARSER_PARSE_RETURN_CODE code;
    TCHAR const* arg;
    size_t arg_len;
    TCHAR const* value;
} ARGPARSER_PARSE_RETURN;

extern ARGPARSER_PARSE_RETURN argparser_parse_parameters(logger_instance* logger, argparser_data const* data, int argc,
                                                         TCHAR const* argv[]);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_MAIN_ARGPARSER_H__) */
