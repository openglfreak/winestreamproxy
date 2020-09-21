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
#ifndef __WINESTREAMPROXY_MAIN_ATTRIBUTES_H__
#define __WINESTREAMPROXY_MAIN_ATTRIBUTES_H__

#define CHECK_GCC_VERSION_2(maj, min) (__GNUC__ > (maj) || (__GNUC__ == (maj) && __GNUC_MINOR__ >= (min)))
#define CHECK_CLANG_ATTRIBUTE(attr) (__clang__ && __has_attribute(attr))
#define CHECK_MSVC_VERSION(ver) (_MSC_VER >= ver)

#if CHECK_GCC_VERSION_2(2, 5) || CHECK_CLANG_ATTRIBUTE(const)
#define ATTR_CONST __attribute__ ((const))
#elif __clang__ && __has_attribute(pure)
#define ATTR_CONST __attribute__ ((pure))
#else
#define ATTR_CONST
#endif

#if CHECK_GCC_VERSION_2(2, 96) || CHECK_CLANG_ATTRIBUTE(pure)
#define ATTR_PURE __attribute__ ((pure))
#else
#define ATTR_PURE
#endif

#if CHECK_GCC_VERSION_2(3, 1) || CHECK_CLANG_ATTRIBUTE(always_inline)
#define ATTR_ALWAYS_INLINE __attribute__ ((always_inline))
#else
#define ATTR_ALWAYS_INLINE
#endif

#if CHECK_GCC_VERSION_2(4, 3) || CHECK_CLANG_ATTRIBUTE(artificial)
#define ATTR_ARTIFICIAL __attribute__ ((artificial))
#elif CHECK_CLANG_ATTRIBUTE(nodebug)
#define ATTR_ARTIFICIAL __attribute__ ((nodebug))
#else
#define ATTR_ARTIFICIAL
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
#define ATTR_NOTHROW_CXX noexcept
#elif defined(__cplusplus)
#define ATTR_NOTHROW_CXX throw()
#else
#define ATTR_NOTHROW_CXX
#endif

#if CHECK_GCC_VERSION_2(3, 3) || CHECK_CLANG_ATTRIBUTE(nothrow)
#define ATTR_NOTHROW __attribute__ ((nothrow))
#elif CHECK_MSVC_VERSION(1200)
#define ATTR_NOTHROW __declspec(nothrow)
#else
#define ATTR_NOTHROW
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
#define ATTR_NODISCARD_CXX(x) [[nodiscard(x)]]
#else
#define ATTR_NODISCARD_CXX(x)
#endif

#if CHECK_GCC_VERSION_2(3, 4) || CHECK_CLANG_ATTRIBUTE(warn_unused_result)
#define ATTR_NODISCARD(x) __attribute__ ((warn_unused_result))
#elif CHECK_MSVC_VERSION(1700)
#define ATTR_NODISCARD(x) _Check_return_
#else
#define ATTR_NODISCARD(x)
#endif

#if CHECK_GCC_VERSION_2(3, 3) || CHECK_CLANG_ATTRIBUTE(visibility)
#define ATTR_INTERNAL __attribute__ ((visibility("internal")))
#else
#define ATTR_INTERNAL
#endif

#endif /* !defined(__WINESTREAMPROXY_MAIN_ATTRIBUTES_H__) */
