#!/bin/sh
# Copyright (C) 2020 Torge Matthies
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Author contact info:
#   E-Mail address: openglfreak@googlemail.com
#   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D

# https://stackoverflow.com/a/29835459
: "${script_dir:="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)"}"

# Load settings.conf.
if [ -e ./settings.conf ]; then
    # shellcheck source=settings.conf
    . ./settings.conf
elif [ -e "${script_dir}/settings.conf" ]; then
    # shellcheck source=settings.conf
    . "${script_dir}/settings.conf"
else
    printf 'error: settings.conf not found\n' >&2
    exit 1
fi

# Check whether the socket exists.
if ! [ -e "${socket_path}" ]; then
    printf 'warning: %s does not exist\n' "${socket_path}" >&2
fi

# Switch to debug exe if the script name is start-debug.
if ! [ x"${exe_name+set}" = x'set' ]; then
    case "${0##*/}" in
        start-debug|start-debug.sh)
            exe_name=winestreamproxy-debug.exe.so;;
        *)
            exe_name=winestreamproxy.exe.so;;
    esac
fi

# Find base directory.
if ! [ x"${base_dir+set}" = x'set' ]; then
    if [ -e "${script_dir}/${exe_name}" ]; then
        base_dir="${script_dir}"
    elif [ -e "${script_dir}/../${exe_name}" ]; then
        # https://stackoverflow.com/a/29835459
        base_dir="$(CDPATH='' cd -- "${script_dir}/.." && pwd -P)"
    elif [ -e "${script_dir}/../out/${exe_name}" ]; then
        # https://stackoverflow.com/a/29835459
        base_dir="$(CDPATH='' cd -- "${script_dir}/../out" && pwd -P)"
    else
        printf 'error: %s not found\n' "${exe_name}" >&2
        exit 1
    fi
fi

# Function that can be used to check if a Wine prefix is a 64-bit prefix.
is_prefix_win64() {
    [ -e "$1/system.reg" ] && grep -q '^#arch=win64$' "$1/system.reg" && \
    return || return 1
}

# Function that can be used to check if a file is in 64-bit ELF format.
is_elf64() {
    od -N 5 -t x1 -- "$1" | \
    grep -q '^[^[:space:]][^[:space:]]*[[:space:]][[:space:]]*7f[[:space:]][[:space:]]*45[[:space:]][[:space:]]*4c[[:space:]][[:space:]]*46[[:space:]][[:space:]]*02$' && \
    return || return 1
}

# Function that can be used to check if a program is in the PATH.
# shellcheck disable=SC2015
is_in_path() {
    # POSIX.
    command -v -- "$1" >/dev/null 2>&1 && return || :
    hash -- "$1" >/dev/null 2>&1 && return || :
    # Non-POSIX.
    # shellcheck disable=SC2039
    type -p -- "$1" >/dev/null 2>&1 && return || :
    # shellcheck disable=SC2230
    which -- "$1" >/dev/null 2>&1 && return || :
    # Not found.
    return 1
}

# Find wine executable path.
# shellcheck disable=SC2153
if [ x"${WINE+set}" = x'set' ]; then
    wine="${WINE}"
else
    if ! [ x"${WINEARCH+set}" = x'set' ] && \
       is_prefix_win64 "${WINEPREFIX:-$HOME/.wine}"; then
        WINEARCH=win64
    fi
    if [ x"${WINEARCH}" = x'win64' ] && is_elf64 "${base_dir}/${exe_name}" && \
       { is_in_path wine64 || wine64 --version > /dev/null 2>&1; }; then
        wine=wine64
    elif is_in_path wine || wine --version > /dev/null 2>&1; then
        wine=wine
    else
        # shellcheck disable=SC2016
        printf 'error: $WINE not set and Wine binary not found\n' >&2
        exit 1
    fi
    # shellcheck disable=SC2016
    printf 'warning: $WINE not set, using "%s"\n' "${wine}" >&2
fi

if [ x"${wine#*/}" != x"${wine}" ] && [ -x "${wine}" ] || \
   is_in_path "${wine}"; then
    # shellcheck disable=SC2034
    _wine="${wine}"
    # shellcheck disable=SC2016
    wine='"${_wine}"'
fi

: "${pipe_name}" "${socket_path}"  # Make shellcheck happy.

# Start winestreamproxy in the background and wait until the pipe server loop is running.
eval "setsid -w ${wine} \"\${base_dir}/\${exe_name}\" \"\${pipe_name}\" \"\${socket_path}\""
