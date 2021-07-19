#!/bin/sh

set -e

( SOURCE_DATE_EPOCH=0 . ./build-32.sh debug release )
make debug-tarball
make release-tarball
. out/.version
mv out/debug.tar.gz "winestreamproxy-debug-${MAJOR_VERSION}.${MINOR_VERSION}.${MINOR_VERSION}${EXTRA_VERSION}-i386.tar.gz"
mv out/release.tar.gz "winestreamproxy-${MAJOR_VERSION}.${MINOR_VERSION}.${MINOR_VERSION}${EXTRA_VERSION}-i386.tar.gz"

( SOURCE_DATE_EPOCH=0 . ./build-64.sh debug release )
make debug-tarball
make release-tarball
. out/.version
mv out/debug.tar.gz "winestreamproxy-debug-${MAJOR_VERSION}.${MINOR_VERSION}.${MINOR_VERSION}${EXTRA_VERSION}-amd64.tar.gz"
mv out/release.tar.gz "winestreamproxy-${MAJOR_VERSION}.${MINOR_VERSION}.${MINOR_VERSION}${EXTRA_VERSION}-amd64.tar.gz"

make clean
