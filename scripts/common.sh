#!/hint/sh
# Copyright (C) 2021 Torge Matthies
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Author contact info:
#   E-Mail address: openglfreak@googlemail.com
#   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D

# Required variables.
: "${script_dir:?}"
: "${base_dir:?}"
: "${debug:?}"
: "${prefix:?}"

# Determine exe name and path.
if [ x"${debug}" = x'true' ]; then
    exe_name=winestreamproxy-debug.exe
    dll_name=winestreamproxy_unixlib-debug.dll.so
else
    exe_name=winestreamproxy.exe
    dll_name=winestreamproxy_unixlib.dll.so
fi
exe_path="${base_dir}/${exe_name}"
# shellcheck disable=SC2034
dll_path="${base_dir}/${dll_name}"

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
    if [ x"${settings_found}" != x'true' ] && \
       [ x"${WINESTREAMPROXY_PIPE_NAME:+set}${WINESTREAMPROXY_SOCKET_PATH:+set}${WINESTREAMPROXY_SYSTEM:+set}" != x'setsetset' ]; then
        printf 'error: configuration file not found\n' >&2
        exit 1
    fi
}
load_settings() {
    # Load settings.
    load_settings_file "${script_dir}/settings.conf"
    load_settings_file "${prefix}/lib/winestreamproxy/settings.conf"
    f() { load_settings_file "$1/winestreamproxy/settings.conf"; };
    split : "${XDG_CONFIG_DIRS:-/etc/xdg}" foreach_reverse f
    load_settings_file \
        "${XDG_CONFIG_HOME:-${HOME}/.config}/winestreamproxy/settings.conf"
    load_settings_file ./winestreamproxy.conf
    check_settings_found
    pipe_name="${WINESTREAMPROXY_PIPE_NAME:-${pipe_name}}"
    socket_path="${WINESTREAMPROXY_SOCKET_PATH:-${socket_path}}"
    system="${WINESTREAMPROXY_SYSTEM:-${system}}"
}

# Function that can be used to check the architecture of a Wine prefix.
# Takes either "win64" or "win32" as the first parameter and the prefix
# path as the second parameter.
check_prefix_arch() {
    case "$1" in win32|win64) :;; *)
        printf 'error: invalid prefix architecture: %s\n' "$1" >2
        return 1
    esac
    [ -e "$2/system.reg" ] && grep -q "^#arch=$1$" "$2/system.reg" && \
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

# Function for finding the path to the Wine binary to be used for the
# selected prefix.
find_wine() {
    # shellcheck disable=SC2153
    if [ x"${WINE+set}" = x'set' ]; then
        wine="${WINE}"
    else
        if ! [ x"${WINEARCH+set}" = x'set' ]; then
            if check_prefix_arch win64 "${WINEPREFIX:-${HOME}/.wine}"; then
                WINEARCH=win64
            elif check_prefix_arch win32 "${WINEPREFIX:-${HOME}/.wine}"; then
                WINEARCH=win32
            fi
        fi
        if [ x"${WINEARCH}" = x'win64' ] && is_elf64 "${dll_path}" && \
           { is_in_path wine64 || wine64 --version > /dev/null 2>&1; }; then
            wine=wine64
        elif is_in_path wine || wine --version > /dev/null 2>&1; then
            wine=wine
        else
            # shellcheck disable=SC2016
            printf 'error: $WINE not set and Wine binary not found\n' >&2
            return 1
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
}

# Function for running Wine.
run_wine() {
    eval "setsid -w ${wine} \"\$@\""
}

# Function for finding the path to the wineserver binary to be used for the
# selected prefix.
find_wineserver() {
    # shellcheck disable=SC2153
    if [ x"${WINESERVER+set}" = x'set' ]; then
        wineserver="${WINESERVER}"
        if [ x"${wineserver#*/}" != x"${wineserver}" ] && \
           [ -x "${wineserver}" ] || is_in_path "${wineserver}"; then
            # shellcheck disable=SC2034
            _wineserver="${wineserver}"
            # shellcheck disable=SC2016
            wineserver='"${_wineserver}"'
        fi
    else
        _wineserver="$(run_wine 'C:\windows\system32\cmd.exe' /C 'winepath -u %WINEDATADIR%\..\..\bin\wineserver'; echo x)"
        _wineserver="${_wineserver%?x}"
        _wineserver="${_wineserver%$(printf '\r')}"  # Work around old Wine bug
        if ! [ -x "${_wineserver}" ]; then
            # shellcheck disable=SC2016
            printf 'error: $WINESERVER not set and wineserver binary not found\n' >&2
            return 1
        fi
        # shellcheck disable=SC2016
        wineserver='"${_wineserver}"'
    fi
}

# Function for running wineserver.
run_wineserver() {
    eval "setsid -w ${wineserver} \"\$@\""
}

# Generates a pseudo-random 32-bit hex string.
genrandom() {
    # shellcheck disable=SC3028,SC2039
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
