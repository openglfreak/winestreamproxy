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

# shellcheck disable=SC2050
if [ x'@prefix_defined@' = x'true' ]; then
    prefix='@prefix@'
else
    prefix='/usr/local'
fi

# Splits the first parameter using the second parameter as the delimiter,
# appends the result to the end of the parameter list, and executes the
# resulting command.
split() {
    IFS="$1" eval 'set -- "$@" $2'
    shift 2
    "$@"
}

# Calls the function in the first parameter with each of the following
# parameters in reverse order separately.
foreach_reverse() {
    set -- "$(($#+1))" "$@"
    while [ "$1" -ge '3' ]; do
        eval "\"$2\" \"\$$1\""
        eval "shift; set -- '$(($1-1))' \"\$@\""
    done
}

# Define settings helper functions.
settings_found=false
load_settings_file() {
    if [ -e "$1" ]; then
        # shellcheck source=settings.conf
        . "$1"
        settings_found=true
    fi
}
check_settings_found() {
    if [ x"${settings_found}" != x'true' ]; then
        printf 'error: configuration file not found\n' >&2
        exit 1
    fi
}
# Load settings.
load_settings_file "${script_dir}/settings.conf"
load_settings_file "${prefix}/lib/winestreamproxy/settings.conf"
f() { load_settings_file "$1/winestreamproxy/settings.conf"; };
split : "${XDG_CONFIG_DIRS:-/etc/xdg}" foreach_reverse f
load_settings_file \
    "${XDG_CONFIG_HOME:-${HOME}/.config}/winestreamproxy/settings.conf"
load_settings_file ./winestreamproxy.conf
check_settings_found

# Check whether the socket exists.
if ! [ -e "${socket_path}" ]; then
    printf 'warning: %s does not exist\n' "${socket_path}" >&2
fi

# Switch to debug exe if the script name ends with -debug.
if ! [ x"${exe_name+set}" = x'set' ]; then
    case "${0##*/}" in
        start-debug|start-debug.sh|winestreamproxy-debug)
            exe_name=winestreamproxy-debug.exe.so;;
        *)
            exe_name=winestreamproxy.exe.so;;
    esac
fi

# Find base directory.
if ! [ x"${base_dir+set}" = x'set' ]; then
    if [ -e "${script_dir}/${exe_name}" ]; then
        base_dir="${script_dir}"
    elif [ -e "${script_dir}/../out/${exe_name}" ]; then
        # https://stackoverflow.com/a/29835459
        base_dir="$(CDPATH='' cd -- "${script_dir}/../out" && pwd -P)"
    elif [ -e "${prefix}/lib/winestreamproxy/${exe_name}" ]; then
        # https://stackoverflow.com/a/29835459
        base_dir="$(CDPATH='' cd -- "${prefix}/lib/winestreamproxy" && pwd -P)"
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
    # shellcheck disable=SC2039,SC3045
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

# Generates a pseudo-random 32-bit hex string.
genrandom() {
    # shellcheck disable=SC3028
    if [ x"${RANDOM}" != x"${RANDOM}" ]; then
        # Native shell support for $RANDOM is the best and fastest.
        printf '%04x' $((((RANDOM&0x3FF)<<22)|((RANDOM&0x7FF)<<11)|(RANDOM&0x7FF)))
    elif [ -e /dev/urandom ]; then
        # /dev/urandom is not specified by POSIX, but it's still better than
        # awk srand() + rand().
        od -N 4 -t x1 -- /dev/urandom | sed -n '1s/^[^[:space:]][^[:space:]]*[[:space:]][[:space:]]*//;1s/[[:space:]]//gp'
    else
        # Need to sleep to make sure srand() picks a unique seed.
        sleep 1
        awk 'BEGIN { srand(); printf("%04x\n", rand() * 0x100000000); }'
    fi
}

# Creates a temporary fifo.
mktempfifo() (
    n=20  # Try at most 20 times.
    while [ 0 -lt "$n" ]; do
        path="${TMPDIR:-/tmp}/tmp$(genrandom).fifo" || exit
        if mkfifo -m 600 -- "${path}" 2>/dev/null; then
            break
        fi
        n="$((n - 1))"
    done
    if [ -z "${path}" ]; then
        printf 'error: Could not create a temporary fifo' >&2
        return 1
    else
        printf '%s\n' "${path}"
    fi
)

# Starts a dummy process to keep wine running if winestreamproxy is started
# in system mode.
start_dummy_process() {
    tmpfifo="$(mktempfifo)" || exit
    eval "cat < \"\${tmpfifo}\" | setsid -w ${wine} cmd /C 'set /P x=' &"
}

# Stops the dummy process started by start_dummy_process.
# To be used by the wrapper script.
stop_dummy_process() {
    if [ -n "${tmpfifo}" ] && [ -e "${tmpfifo}" ]; then
        echo > "${tmpfifo}"
        rm -f "${tmpfifo}"
        tmpfifo=''
    fi
}

if [ x"${__start_dummy_process:-false}" = x'true' ]; then
    start_dummy_process
fi

: "${pipe_name}" "${socket_path}"  # Make shellcheck happy.

# Start winestreamproxy in the background and wait until the proxy loop is running.
eval "setsid -w ${wine} \"\${base_dir}/\${exe_name}\" --pipe \"\${pipe_name}\" --socket \"\${socket_path}\" \
                                                      ${system+ --system=\"\${system\}\"} \${1+\"\$@\"}"
