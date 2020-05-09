#!/bin/sh

# https://stackoverflow.com/a/29835459
: "${script_dir:="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)"}"

# Make sure XDG_RUNTIME_DIR is set.
if ! [ "${XDG_RUNTIME_DIR+set}" = 'set' ]; then
    export XDG_RUNTIME_DIR=/tmp
    # shellcheck disable=SC2016
    printf 'warning: $XDG_RUNTIME_DIR not set, using %s\n' "${XDG_RUNTIME_DIR}" >&2
fi

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
case "${0##*/}" in
    start-debug|start-debug.sh)
        exe_name=winestreamproxy-debug.exe.so;;
    *)
        exe_name=winestreamproxy.exe.so;;
esac

# Find base directory.
# shellcheck disable=SC1011,SC2026
if ! [ x"${base_dir+set}"x = x'set'x ]; then
    if [ -e "${script_dir}/${exe_name}" ]; then
        base_dir="${script_dir}"
    elif [ -e "${script_dir}/../${exe_name}" ]; then
        # https://stackoverflow.com/a/29835459
        base_dir="$(CDPATH='' cd -- "${script_dir}/.." && pwd -P)"
    else
        printf 'error: %s not found\n' "${exe_name}" >&2
        exit 1
    fi
fi

find_wine() {
    # POSIX.
    WINE="$(command -v wine 2>/dev/null)" && return
    WINE="$(type -p wine)" && : "${WINE=wine}" && return
    WINE="$(hash wine && echo wine)" && return
    # Non-POSIX.
    # shellcheck disable=SC2230
    WINE="$(which wine 2>/dev/null)" && return
    WINE="$(wine --version >/dev/null 2>&1 && echo wine)" && return
    # Not found.
    return 1
}
# Find wine executable path.
# shellcheck disable=SC1011,SC2026
if ! [ x"${WINE+set}"x = x'set'x ]; then
    if ! find_wine; then
        printf 'error: Wine not found\n' >&2
        exit 1
    fi
    export WINE
    # shellcheck disable=SC2016
    printf 'warning: $WINE not set, using %s\n' "${WINE}" >&2
fi

# Start winestreamproxy in the background and wait until the pipe server loop is running.
{ nohup "${WINE}" "${base_dir}/${exe_name}" "${pipe_name}" "${socket_path}" </dev/null & } | \
LC_ALL=C grep -q -m 1 -F 'Started pipe server loop'
