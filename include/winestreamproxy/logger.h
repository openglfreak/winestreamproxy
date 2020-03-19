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
#ifndef __WINESTREAMPROXY_LOGGER_H__
#define __WINESTREAMPROXY_LOGGER_H__

#ifdef _UNICODE
#include <wchar.h>
#endif /* defined(_UNICODE) */

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef enum LOG_LEVEL {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARNING = 3,
    LOG_LEVEL_ERROR = 4,
    LOG_LEVEL_CRITICAL = 5
} LOG_LEVEL;

typedef struct logger_instance logger_instance;

typedef int (*log_message_callback)(logger_instance* logger, LOG_LEVEL level, void const* message);

struct logger_instance {
    log_message_callback log_message;
    unsigned char character_size;
    void* impl_data;

    unsigned int enabled_log_levels;
};

extern int log_create_logger(log_message_callback log_message, unsigned char character_size,
                             logger_instance** out_logger);
#define log_set_callback(logger, callback) ((void)(logger->log_message = callback))
#define log_get_impl_data(logger, data) (logger->impl_data)
#define log_set_impl_data(logger, data) ((void)(logger->impl_data = data))
#define log_enable_level(logger, level) ((void)(logger->enabled_log_levels |= 1U << (level)))
#define log_disable_level(logger, level) ((void)(logger->enabled_log_levels &= ~(1U << (level))))
#define log_set_min_level(logger, level) ((void)(logger->enabled_log_levels = ~0U << (level)))
extern void log_destroy_logger(logger_instance* logger);

extern int log_init_message(logger_instance* logger, LOG_LEVEL level, char const* file, long line);
extern int log_print_message(char const* format, ...);
#ifdef _UNICODE
extern int wlog_init_message(logger_instance* logger, LOG_LEVEL level, wchar_t const* file, long line);
extern int wlog_print_message(wchar_t const* format, ...);
#endif /* defined(_UNICODE) */

extern void log_get_file_and_line(void const** file, long* line);

#define LOG_IS_ENABLED(logger, level) (!!((1U << (level)) & (logger)->enabled_log_levels))

#define LOG_NARROW(logger, level, message) \
    (LOG_IS_ENABLED(logger, level) ? ( \
        log_init_message((logger), (level), __FILE__, __LINE__) ? log_print_message message : 0 \
    ) : 1)
#define LOG_WIDE(logger, level, message) \
    (LOG_IS_ENABLED(logger, level) ? ( \
        wlog_init_message((logger), (level), L __FILE__, __LINE__) ? wlog_print_message message : 0 \
    ) : 1)
#ifdef _UNICODE
#define LOG(logger, level, message) LOG_WIDE(logger, level, message)
#else /* !defined(_UNICODE) */
#define LOG(logger, level, message) LOG_NARROW(logger, level, message)
#endif /* !defined(_UNICODE) */

#define LOG_TRACE(logger, message) LOG(logger, LOG_LEVEL_TRACE, message)
#define LOG_DEBUG(logger, message) LOG(logger, LOG_LEVEL_DEBUG, message)
#define LOG_INFO(logger, message) LOG(logger, LOG_LEVEL_INFO, message)
#define LOG_WARNING(logger, message) LOG(logger, LOG_LEVEL_WARNING, message)
#define LOG_ERROR(logger, message) LOG(logger, LOG_LEVEL_ERROR, message)
#define LOG_CRITICAL(logger, message) LOG(logger, LOG_LEVEL_CRITICAL, message)

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_LOGGER_H__) */
