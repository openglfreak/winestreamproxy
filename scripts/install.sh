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
. "${base_dir}/${common_sh_name}"

# Load settings file.
load_settings || exit

# Find path to the Wine binary.
find_wine || exit

# Escape a string for literal use in CreateProcess.
escape_param() {
    set -- "$(printf '%sx\n' "${1}" | sed 's/"/""/g'; echo x)"
    printf '"%s"\n' "${1%x?x}"
}

binpath="$(escape_param "${exe_path:?}") --pipe $(escape_param "${pipe_name}") --socket $(escape_param "${socket_path}") --svchost"
run_wine 'C:\windows\system32\sc.exe' delete winestreamproxy ||:
run_wine 'C:\windows\system32\sc.exe' create winestreamproxy start= auto binpath= "${binpath}"
