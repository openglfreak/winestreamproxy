# Copyright (C) 2021 Torge Matthies
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Author contact info:
#   E-Mail address: openglfreak@googlemail.com
#   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D

FROM ubuntu:focal-20210713

ADD https://dl.winehq.org/wine-builds/winehq.key /tmp/winehq.key

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && \
    apt-get install -y --no-install-recommends gnupg2 ca-certificates && \
    apt-key add /tmp/winehq.key && \
    echo 'deb https://dl.winehq.org/wine-builds/ubuntu/ focal main' >/etc/apt/sources.list.d/winehq.list && \
    dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y --no-install-recommends winehq-devel='6.13~focal-1' wine-devel='6.13~focal-1' wine-devel-amd64='6.13~focal-1' wine-devel-i386='6.13~focal-1' wine-devel-dev='6.13~focal-1' libc6-dev:i386 gcc gcc-multilib g++ g++-multilib make git && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
CMD . ./make-release.sh
