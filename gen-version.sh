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

git_version_tag_match='--match v*\.*\.* --exclude v*[!0-9]*\.*\.* --exclude v*\.*[!0-9]*\.* --exclude v*\.*\.*[!0-9]*'

# variables:
#     [out] int major_version
#     [out] int minor_version
#     [out] int patch_version
#     [out] int additional_commits
#     [out] str commit
#     [out] str dirty
#     [out] str local_version
read_version() {
    major_version=0; minor_version=0; patch_version=0; additional_commits=0; commit=; dirty=; local_version=;
    local __get_version_version __get_version_additional_commits __get_version_commit || :

    if [ -f .version ]; then . ./.version; fi
    if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then return; fi
    if __get_version_version="$(git describe --tags --long $git_version_tag_match --dirty 2>/dev/null)"; then
        major_version="${__get_version_version#v}"; major_version="${major_version%%.*}"
        minor_version="${__get_version_version#*.}"; minor_version="${minor_version%%.*}"
        patch_version="${__get_version_version#*.*.}"; patch_version="${patch_version%%-*}"
        additional_commits="${__get_version_version#*.*.*-}"; additional_commits="${additional_commits%%-*}"
        commit="${__get_version_version#*.*.*-*-}"; commit="${commit%%-*}"
        dirty="${__get_version_version#*.*.*-*-*-}"; dirty="${dirty#${__get_version_version}}"
    elif __get_version_additional_commits="$(git rev-list --count HEAD)" && \
        __get_version_commit="$(git describe --always --long $git_version_tag_match --dirty 2>/dev/null)"; then
        additional_commits="${__get_version_additional_commits}"
        commit="${__get_version_commit%%-*}"
        dirty="${__get_version_commit#*-}"; dirty="${dirty#${__get_version_commit}}"
    fi
}

# variables:
#     [in] int additional_commits
#     [in] str commit
#     [in] str dirty
#     [in] str local_version
#     [in] str extra_version
#     [out] int has_extra_version
#     [out] int has_local_version
#     [out] int is_release
#     [out] int has_commit
#     [out] int is_dirty
process_version() {
    if [ "${additional_commits:-0}" -eq 0 ]; then
        extra_version="${dirty:+-dirty}${local_version:++${local_version}}"
    else
        extra_version="-r${additional_commits}${dirty:+.dirty}"
        if [ -n "${commit}" ]; then
            extra_version="${extra_version}+${commit}${local_version:+.${local_version}}"
        else
            extra_version="${extra_version}${local_version:++${local_version}}"
        fi
    fi

    if [ -n "${extra_version}" ]; then
        has_extra_version=1
    else
        has_extra_version=0
    fi

    if [ -n "${local_version}" ]; then
        has_local_version=1
    else
        has_local_version=0
    fi

    if [ "${additional_commits:-0}" -eq 0 ]; then
        is_release=1
    else
        is_release=0
    fi

    if [ -n "${commit}" ]; then
        has_commit=1
    else
        has_commit=0
    fi

    if [ -n "${dirty}" ]; then
        is_dirty=1
    else
        is_dirty=0
    fi
}

# variables:
#     [out] int major_version
#     [out] int minor_version
#     [out] int patch_version
#     [out] int additional_commits
#     [out] str commit
#     [out] str dirty
#     [out] str local_version
#     [out] int has_extra_version
#     [out] int has_local_version
#     [out] int is_release
#     [out] int has_commit
#     [out] int is_dirty
get_version() { read_version && process_version; }

# variables:
#     [in] int major_version
#     [in] int minor_version
#     [in] int patch_version
#     [in] str extra_version
#     [in] int has_extra_version
#     [in] int has_local_version
#     [in] str local_version
#     [in] int is_release
#     [in] int additional_commits
#     [in] int has_commit
#     [in] str commit
#     [in] int is_dirty
# stdout:
#     The generated #defines.
output_version_defines() {
    cat <<EOF
#define MAJOR_VERSION ${major_version}
#define MINOR_VERSION ${minor_version}
#define PATCH_VERSION ${patch_version}
#define EXTRA_VERSION ${extra_version}

#define HAS_EXTRA_VERSION ${has_extra_version}
#define HAS_LOCAL_VERSION ${has_local_version}
#define LOCAL_VERSION ${local_version}
#define IS_RELEASE ${is_release}
#define ADDITIONAL_COMMITS ${additional_commits:-0}
#define HAS_COMMIT ${has_commit}
#define COMMIT ${commit}
#define IS_DIRTY ${is_dirty}
EOF
}

# variables:
#     [in] int major_version
#     [in] int minor_version
#     [in] int patch_version
#     [in] str extra_version
#     [in] int has_extra_version
#     [in] int has_local_version
#     [in] str local_version
#     [in] int is_release
#     [in] int additional_commits
#     [in] int has_commit
#     [in] str commit
#     [in] int is_dirty
# stdout:
#     The generated #defines.
output_version_assignments() {
    cat <<EOF
MAJOR_VERSION=${major_version}
MINOR_VERSION=${minor_version}
PATCH_VERSION=${patch_version}
EXTRA_VERSION=${extra_version}

HAS_EXTRA_VERSION=${has_extra_version}
HAS_LOCAL_VERSION=${has_local_version}
LOCAL_VERSION=${local_version}
IS_RELEASE=${is_release}
ADDITIONAL_COMMITS=${additional_commits:-0}
HAS_COMMIT=${has_commit}
COMMIT=${commit}
IS_DIRTY=${is_dirty}
EOF
}
