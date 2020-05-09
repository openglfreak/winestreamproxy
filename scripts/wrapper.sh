#!/bin/sh

# Find the script directory.
# https://stackoverflow.com/a/29835459
: "${script_dir:="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)"}"

# Use start-debug if the script name is wrapper-debug.
case "${0##*/}" in
    wrapper-debug.sh)
        sh1=.sh; sh2=; start_script=start-debug;;
    wrapper-debug)
        sh1=; sh2=.sh; start_script=start-debug;;
    *.sh)
        sh1=.sh; sh2=; start_script=start;;
    *)
        sh1=; sh2=.sh; start_script=start;;
esac
if [ -e "${script_dir}/${start_script}${sh1}" ]; then
    start_script="${script_dir}/${start_script}${sh1}"
elif [ -e "${script_dir}/${start_script}${sh2}" ]; then
    start_script="${script_dir}/${start_script}${sh2}"
else
    printf 'error: %s not found\n' "${start_script}${sh1}" >&2
fi

# Run winestreamproxy launch script.
# shellcheck source=start.sh
. "${start_script}" || exit
# Run the supplied command.
exec ${1+"$@"}
