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

# Use start-debug if the script name is wrapper-debug.
case "${0##*/}" in
    winestreamproxy-wrapper-debug)
        sh1=; sh2=.sh; start_script=winestreamproxy-debug;;
    wrapper-debug.sh)
        sh1=.sh; sh2=; start_script=start-debug;;
    wrapper-debug)
        sh1=; sh2=.sh; start_script=start-debug;;
    winestreamproxy-wrapper)
        sh1=; sh2=.sh; start_script=winestreamproxy;;
    *.sh)
        sh1=.sh; sh2=; start_script=start;;
    *)
        sh1=; sh2=.sh; start_script=start;;
esac
case "$start_script" in *-debug)
    exe_name=winestreamproxy-debug.exe.so
esac
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
