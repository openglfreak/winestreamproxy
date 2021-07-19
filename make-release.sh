#!/bin/sh
# Copyright (C) 2021 Torge Matthies
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Author contact info:
#   E-Mail address: openglfreak@googlemail.com
#   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D

set -e

( set -- debug-tarball release-tarball; export SOURCE_DATE_EPOCH=0; . ./build-32.sh )
. out/.version
mv out/debug.tar.gz "winestreamproxy-debug-${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}${EXTRA_VERSION}-i386.tar.gz"
mv out/release.tar.gz "winestreamproxy-${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}${EXTRA_VERSION}-i386.tar.gz"

( set -- debug-tarball release-tarball; export SOURCE_DATE_EPOCH=0; . ./build-64.sh )
. out/.version
mv out/debug.tar.gz "winestreamproxy-debug-${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}${EXTRA_VERSION}-amd64.tar.gz"
mv out/release.tar.gz "winestreamproxy-${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}${EXTRA_VERSION}-amd64.tar.gz"

make clean
