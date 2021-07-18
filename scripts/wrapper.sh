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
if [ x'@debug@' = x'true' ]; then
    debug='true'
else
    case "${0##*/}" in
        *-debug.sh|*-debug) debug='true';;
        *) debug='false';;
    esac
fi

# shellcheck disable=SC2050
if [ x'@installed@' = x'true' ]; then
    sh1=''
    sh2='.sh'
    start_script="winestreamproxy"
else
    start_script="start"
    case "${0##*/}" in
        *.sh) sh1=.sh; sh2=;;
        *) sh1=; sh2=.sh;;
    esac
fi

if [ x"${debug}" = x'true' ]; then
    start_script="${start_script}-debug"
fi

if [ -e "${script_dir}/${start_script}${sh1}" ]; then
    start_script="${script_dir}/${start_script}${sh1}"
elif [ -e "${script_dir}/${start_script}${sh2}" ]; then
    start_script="${script_dir}/${start_script}${sh2}"
else
    printf 'error: %s not found\n' "${start_script}${sh1}" >&2
fi

# Tell the launch script to start a dummy Wine process before starting
# the real winestreamproxy process.
__start_dummy_process=true
# Run winestreamproxy launch script.
# shellcheck source=start.sh
run_start_script() { . "${start_script}"; }
run_start_script </dev/null || exit
# Stop the dummy process after a timeout of 3 seconds.
{ sleep 3; stop_dummy_process; } &
# Run the supplied command.
exec ${1+"$@"}
