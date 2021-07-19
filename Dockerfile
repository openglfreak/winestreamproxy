FROM ubuntu:focal-20210713

ADD https://dl.winehq.org/wine-builds/winehq.key /tmp/winehq.key

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && \
    apt-get install -y --no-install-recommends gnupg2 ca-certificates && \
    apt-key add /tmp/winehq.key && \
    echo 'deb https://dl.winehq.org/wine-builds/ubuntu/ focal main' >/etc/apt/sources.list.d/winehq.list && \
    dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y --no-install-recommends winehq-devel='6.12~focal-1' wine-devel-amd64='6.12~focal-1' wine-devel-i386='6.12~focal-1' wine-devel-dev='6.12~focal-1' libc6-dev:i386 gcc gcc-multilib g++ g++-multilib mingw-w64 make git && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
CMD . ./make-release.sh
