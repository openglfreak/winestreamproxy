#!/bin/sh
# Copyright (C) 2020-2021 Torge Matthies
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Author contact info:
#   E-Mail address: openglfreak@googlemail.com
#   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D

# Find the script directory.
# https://stackoverflow.com/a/29835459
: "${script_dir:="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)"}"

# shellcheck disable=SC2050
if [ x'@prefix_defined@' = x'true' ]; then
    prefix='@prefix@'
else
    prefix='/usr/local'
fi

# shellcheck disable=SC2050
if [ x'@debug@' = x'true' ]; then
    debug='true'
else
    case "${0##*/}" in
        *-debug.sh|*-debug)
            debug='true';;
        *)
            debug='false';;
    esac
fi

if [ x"${debug}" = x'true' ]; then
    common_sh_name='common-debug.sh'
else
    common_sh_name='common.sh'
fi

# Find base directory.
if ! [ x"${base_dir+set}" = x'set' ]; then
    # shellcheck disable=SC2050
    if [ x'@installed@' = x'true' ]; then
        if [ -e "${prefix}/lib/winestreamproxy/${common_sh_name}" ]; then
            # https://stackoverflow.com/a/29835459
            base_dir="$(CDPATH='' cd -- "${prefix}/lib/winestreamproxy" && \
                        pwd -P)"
        else
            printf 'error: %s not found\n' "${common_sh_name}" >&2
            exit 1
        fi
    else
        if [ -e "${script_dir}/${common_sh_name}" ]; then
            base_dir="${script_dir}"
        elif [ -e "${script_dir}/../out/${common_sh_name}" ]; then
            # https://stackoverflow.com/a/29835459
            base_dir="$(CDPATH='' cd -- "${script_dir}/../out" && pwd -P)"
        else
            printf 'error: %s not found\n' "${common_sh_name}" >&2
            exit 1
        fi
    fi
fi

# Source script with common functions and variables.
# shellcheck source=common.sh
. "${base_dir}/${common_sh_name}"

# Load settings file.
load_settings || exit

# Check whether the socket exists.
if ! [ -e "${socket_path}" ]; then
    printf 'warning: %s does not exist\n' "${socket_path}" >&2
fi

# Find path to the Wine binary.
find_wine || exit

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

# Start winestreamproxy in the background and wait until the proxy loop is running.
run_wine "${exe_path}" --pipe "${pipe_name}" --socket "${socket_path}" \
                       ${system+--system="${system}"} ${1+"$@"}
