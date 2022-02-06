/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "argparser.h"
#include "../bool.h"
#include <winestreamproxy/logger.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

static bool resize_array(void** const array, size_t const elem_size, size_t* const size, size_t const new_size)
{
    void* old_array = *array, * new_array;

    if (old_array)
        new_array = HeapReAlloc(GetProcessHeap(), 0, old_array, elem_size * new_size);
    else
        new_array = HeapAlloc(GetProcessHeap(), 0, elem_size * new_size);
    if (!new_array)
        goto error;

    *size = new_size;
    *array = new_array;
    return true;

error:
    if (old_array)
        HeapFree(GetProcessHeap(), 0, old_array);
    return false;
}

static ARGPARSER_PARSE_RETURN parse_option_value(int const argc, TCHAR const* argv[],
                                                 argparser_option_list_entry const* const option,
                                                 TCHAR const* const value_ptr, int* const arg_index,
                                                 void* const out_value)
{
    ARGPARSER_PARSE_RETURN ret;
    ret.code = ARGPARSER_PARSE_RETURN_SUCCESS;
    ret.arg = &argv[*arg_index][1];
    ret.arg_len = value_ptr - ret.arg;

    switch (option->type)
    {
        case ARGPARSER_OPTION_TYPE_PRESENCE:
            if (!value_ptr)
                *(int*)out_value = 1;
            else
            {
                ret.code = ARGPARSER_PARSE_RETURN_EXTRANEOUS_VALUE;
                ret.value = value_ptr;
            }
            break;
        case ARGPARSER_OPTION_TYPE_STRING:
        {
            if (value_ptr)
                *(TCHAR const**)out_value = value_ptr;
            else if (*arg_index + 1 < argc)
                *(TCHAR const**)out_value = argv[++*arg_index];
            else
                ret.code = ARGPARSER_PARSE_RETURN_MISSING_VALUE;
            break;
        }
        case ARGPARSER_OPTION_TYPE_BOOLEAN:
            if (!value_ptr || _tcsicmp(value_ptr, _T("true")) == 0)
                *(int*)out_value = 1;
            else if (value_ptr && _tcsicmp(value_ptr, _T("false")) == 0)
                *(int*)out_value = 0;
            else
            {
                ret.code = ARGPARSER_PARSE_RETURN_MALFORMED_VALUE;
                ret.value = value_ptr;
            }
            break;
        case ARGPARSER_OPTION_TYPE_INTEGER:
        {
            TCHAR const* str_val = 0;
            if (value_ptr)
                str_val = value_ptr;
            else if (*arg_index + 1 < argc)
                str_val = argv[++*arg_index];
            else
                ret.code = ARGPARSER_PARSE_RETURN_MISSING_VALUE;
            if (str_val && _stscanf(str_val, _T("%d"), (int*)out_value) != 1)
            {
                ret.code = ARGPARSER_PARSE_RETURN_MALFORMED_VALUE;
                ret.value = str_val;
            }
            break;
        }
        case ARGPARSER_OPTION_TYPE_ACCUMULATOR:
            if (!value_ptr)
                ++*(unsigned int*)out_value;
            else
            {
                ret.code = ARGPARSER_PARSE_RETURN_EXTRANEOUS_VALUE;
                ret.value = value_ptr;
            }
            break;
        case ARGPARSER_OPTION_TYPE_DECIMAL:
        {
            TCHAR const* str_val = 0;
            if (value_ptr)
                str_val = value_ptr;
            else if (*arg_index + 1 < argc)
                str_val = argv[++*arg_index];
            else
                ret.code = ARGPARSER_PARSE_RETURN_MISSING_VALUE;
            if (str_val && _stscanf(str_val, _T("%f"), (float*)out_value) != 1)
            {
                ret.code = ARGPARSER_PARSE_RETURN_MALFORMED_VALUE;
                ret.value = str_val;
            }
            break;
        }
        default:
            ret.code = ARGPARSER_PARSE_RETURN_OTHER_ERROR;
            assert(0);
    }

    return ret;
}

static argparser_option_list_entry const* find_long_option(argparser_option_list_entry const* const option_list,
                                                           TCHAR const* const arg, TCHAR const** const value_ptr)
{
    argparser_option_list_entry const* option;

    for (option = option_list; option->type; ++option)
    {
        size_t option_length;

        if (!option->long_option || !*option->long_option)
            continue;
        option_length = _tcslen(option->long_option);
        if (_tcsncmp(arg, option->long_option, option_length) != 0)
            continue;
        if (arg[option_length] == _T('='))
            *value_ptr = &arg[option_length + 1];
        else if (arg[option_length] == _T('\0'))
            *value_ptr = 0;
        else
            continue;
        return option;
    }

    return 0;
}

static ARGPARSER_PARSE_RETURN parse_long_parameter(logger_instance* const logger, argparser_data const* const data,
                                                   int const argc, TCHAR const* argv[], int* const arg_index)
{
    argparser_option_list_entry const* option;
    TCHAR const* value_ptr;
    ARGPARSER_PARSE_RETURN ret;
    void* option_value_ptr;

    LOG_TRACE(logger, (_T("Parsing long parameter")));

    option = find_long_option(data->option_list, &argv[*arg_index][2], &value_ptr);
    if (!option)
        goto error_unk_opt;

    LOG_DEBUG(logger, (_T("Found long option --%s"), option->long_option));

    option_value_ptr = (void*)((char*)data->option_values + option->value_offset);

    ret = parse_option_value(argc, argv, option, value_ptr, arg_index, option_value_ptr);
    if (ret.code != ARGPARSER_PARSE_RETURN_SUCCESS)
        return ret;

    if (option->validate && !option->validate(option_value_ptr))
        goto error_invalid;

    LOG_TRACE(logger, (_T("Parsed long parameter")));

    return ret;

error_unk_opt:
    LOG_ERROR(logger, (_T("Failed parsing parameter %i: Unknown option %s"), *arg_index, argv[*arg_index]));
    ret.code = ARGPARSER_PARSE_RETURN_UNKNOWN_OPTION;
    ret.arg = &argv[*arg_index][1];
    ret.arg_len = (size_t)-1;
    return ret;

error_invalid:
    LOG_ERROR(logger, (_T("Failed parsing parameter %i: Validation failed"), *arg_index, &argv[*arg_index][2]));
    ret.code = ARGPARSER_PARSE_RETURN_VALIDATE_ERROR;
    ret.arg = &argv[*arg_index][1];
    ret.arg_len = value_ptr - ret.arg;
    ret.value = value_ptr;
    return ret;
}

static argparser_option_list_entry const* find_short_option(argparser_option_list_entry const* const option_list,
                                                            TCHAR const* const str, TCHAR const** const name_end_ptr)
{
    argparser_option_list_entry const* option;
    argparser_option_list_entry const* longest_matching_option = 0;
    size_t longest_matching_option_length = 0;

    for (option = option_list; option->type; ++option)
    {
        size_t option_length;

        if (!option->short_option)
            continue;
        option_length = _tcslen(option->short_option);
        if (!option_length || option_length < longest_matching_option_length)
            continue;
        if (_tcsncmp(str, option->short_option, option_length) != 0)
            continue;
        longest_matching_option = option;
        longest_matching_option_length = option_length;
    }

    if (longest_matching_option)
        *name_end_ptr = &str[longest_matching_option_length];
    return longest_matching_option;
}

static ARGPARSER_PARSE_RETURN parse_short_parameter_group(logger_instance* const logger,
                                                          argparser_data const* const data, int const argc,
                                                          TCHAR const* argv[], int* const arg_index)
{
    TCHAR const* name_start_ptr;
    argparser_option_list_entry const* option = 0;
    TCHAR const* name_end_ptr;
    ARGPARSER_PARSE_RETURN ret;
    void* option_value_ptr = 0;

    LOG_TRACE(logger, (_T("Parsing short parameter group")));

    for (name_start_ptr = &argv[*arg_index][1]; *name_start_ptr; name_start_ptr = name_end_ptr)
    {
        option = find_short_option(data->option_list, name_start_ptr, &name_end_ptr);
        if (!option)
            goto error_unk_opt;

        option_value_ptr = (void*)((char*)data->option_values + option->value_offset);

        if (option->type == ARGPARSER_OPTION_TYPE_PRESENCE || option->type == ARGPARSER_OPTION_TYPE_BOOLEAN)
            *(int*)option_value_ptr = 1;
        else if (option->type == ARGPARSER_OPTION_TYPE_ACCUMULATOR)
            ++*(unsigned int*)option_value_ptr;
        else
            break;

        LOG_DEBUG(logger, (_T("Found short option -%s"), option->short_option));
    }

    if (!*name_start_ptr)
        ret.code = ARGPARSER_PARSE_RETURN_SUCCESS;
    else
    {
        TCHAR const* value_str_ptr;

        if (*name_end_ptr != _T('\0'))
            value_str_ptr = name_end_ptr;
        else if (*arg_index + 1 < argc)
            value_str_ptr = argv[++*arg_index];
        else
            goto err_no_val;

        ret = parse_option_value(argc, argv, option, value_str_ptr, arg_index, option_value_ptr);

        LOG_DEBUG(logger, (_T("Found short option -%s with value %s"), option->short_option, value_str_ptr));
    }

    if (ret.code == ARGPARSER_PARSE_RETURN_SUCCESS)
        LOG_TRACE(logger, (_T("Parsed short parameter group")));

    return ret;

error_unk_opt:
    LOG_ERROR(logger, (_T("Failed parsing parameter %i: Unknown option -%s"), *arg_index, name_start_ptr));
    ret.code = ARGPARSER_PARSE_RETURN_UNKNOWN_OPTION;
    ret.arg = name_start_ptr;
    ret.arg_len = (size_t)-1;
    ret.value = 0;
    return ret;

err_no_val:
    LOG_ERROR(logger, (_T("Failed parsing parameter %i: No parameter value given"), *arg_index - 1, name_start_ptr));
    ret.code = ARGPARSER_PARSE_RETURN_MISSING_VALUE;
    ret.arg = name_start_ptr;
    ret.arg_len = (size_t)-1;
    return ret;
}

static ARGPARSER_PARSE_RETURN append_positional(TCHAR const*** const positionals, size_t* const positionals_count,
                                                TCHAR const* const arg)
{
    size_t prev_count = *positionals_count;
    ARGPARSER_PARSE_RETURN ret;

    ret.code = ARGPARSER_PARSE_RETURN_SUCCESS;
    if (resize_array((void**)positionals, sizeof(TCHAR const*), positionals_count, prev_count + 1))
        (*positionals)[prev_count] = arg;
    else
    {
        *positionals = 0;
        ret.code = ARGPARSER_PARSE_RETURN_OUT_OF_MEMORY;
    }

    return ret;
}

static ARGPARSER_PARSE_RETURN parse_parameter(logger_instance* const logger, argparser_data const* const data,
                                              int const argc, TCHAR const* argv[], int* const arg_index,
                                              TCHAR const*** const positionals, size_t* const positionals_count,
                                              BOOL* const exit_loop)
{
    ARGPARSER_PARSE_RETURN ret;

    if (argv[*arg_index][0] != _T('-') || argv[*arg_index][1] == _T('\0'))
    {
        LOG_DEBUG(logger, (_T("Appending parameter %i as positional parameter"), *arg_index));
        return append_positional(positionals, positionals_count, argv[*arg_index]);
    }

    if (argv[*arg_index][1] != _T('-'))
    {
        LOG_DEBUG(logger, (_T("Found short parameter(s) at argument %i"), *arg_index));
        return parse_short_parameter_group(logger, data, argc, argv, arg_index);
    }

    if (argv[*arg_index][2] != _T('\0'))
    {
        LOG_DEBUG(logger, (_T("Found long parameter at argument %i"), *arg_index));
        return parse_long_parameter(logger, data, argc, argv, arg_index);
    }

    LOG_DEBUG(logger, (_T("Found option list delimiter at argument %i"), *arg_index));
    ++*arg_index;
    *exit_loop = TRUE;
    ret.code = ARGPARSER_PARSE_RETURN_SUCCESS;
    return ret;
}

static ARGPARSER_PARSE_RETURN append_positional_all(TCHAR const*** const positionals, size_t* const positionals_count,
                                                    TCHAR const* args[], size_t const count)
{
    size_t prev_count = *positionals_count;
    ARGPARSER_PARSE_RETURN ret;

    ret.code = ARGPARSER_PARSE_RETURN_SUCCESS;
    if (resize_array((void**)positionals, sizeof(TCHAR const*), positionals_count, prev_count + count))
        RtlCopyMemory(*positionals + prev_count, args, sizeof(TCHAR const*) * count);
    else
    {
        *positionals = 0;
        ret.code = ARGPARSER_PARSE_RETURN_OUT_OF_MEMORY;
    }

    return ret;
}

ARGPARSER_PARSE_RETURN argparser_parse_parameters(logger_instance* const logger, argparser_data const* const data,
                                                  int const argc, TCHAR const* argv[])
{
    int arg_index;
    BOOL exit_loop = FALSE;
    TCHAR const** positionals = 0;
    size_t positionals_count = 0;
    ARGPARSER_PARSE_RETURN ret;

    LOG_TRACE(logger, (_T("Parsing parameters")));

    for (arg_index = 1; arg_index < argc && !exit_loop; ++arg_index)
    {
        LOG_TRACE(logger, (_T("Parsing parameter %d <%s>"), arg_index, argv[arg_index]));
        ret = parse_parameter(logger, data, argc, argv, &arg_index, &positionals, &positionals_count, &exit_loop);
        if (ret.code != ARGPARSER_PARSE_RETURN_SUCCESS)
            goto err_parse_failed;
    }

    if (arg_index < argc)
    {
        LOG_DEBUG(logger, (_T("Appending %d remaining positional parameters"), argc - arg_index));
        ret = append_positional_all(&positionals, &positionals_count, &argv[arg_index], argc - arg_index);
        if (ret.code != ARGPARSER_PARSE_RETURN_SUCCESS)
            goto err_append_failed;
    }

    LOG_TRACE(logger, (_T("Parsed parameters")));

    *data->positionals = positionals;
    *data->positionals_count = positionals_count;
    ret.code = ARGPARSER_PARSE_RETURN_SUCCESS;
    return ret;

err_parse_failed:
    LOG_CRITICAL(logger, (_T("Error %d while parsing argument %i"), ret.code, arg_index));
    if (positionals)
        HeapFree(GetProcessHeap(), 0, positionals);
    return ret;

err_append_failed:
    LOG_CRITICAL(logger, (_T("Error %d while appending remaining positional parameters"), ret.code, arg_index));
    if (positionals)
        HeapFree(GetProcessHeap(), 0, positionals);
    return ret;
}
