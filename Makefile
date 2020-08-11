#!/usr/bin/make -f
# Copyright (C) 2020 Torge Matthies
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Author contact info:
#   E-Mail address: openglfreak@googlemail.com
#   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D

OUT = out

MKDIR = mkdir -p --
WINEGCC = winegcc
OBJCOPY = x86_64-w64-mingw32-objcopy
STRIP = x86_64-w64-mingw32-strip
CP = cp -LRpf --
TOUCH = touch --
RM = rm -f --
RMDIR = rmdir --

_include_stdarg = -include stdarg.h  # Work around Wine headers bug in 32-bit mode.

_CFLAGS = -g3 -gdwarf-4 -fPIC -pie -Iinclude -DWIN32_LEAN_AND_MEAN=1 -Wall -Wextra -pedantic -pipe $(CFLAGS) \
          $(_include_stdarg)
_RELEASE_CFLAGS = -O2 -fomit-frame-pointer -fno-stack-protector -fuse-linker-plugin -fuse-ld=gold -fgraphite-identity \
                  -floop-nest-optimize -fipa-pta -fno-semantic-interposition -fno-common -fdevirtualize-at-ltrans \
                  -fno-plt -fgcse-after-reload -fipa-cp-clone -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
                  -ffile-prefix-map="$${PWD:-$$(pwd)}"=. $(_CFLAGS) $(RELEASE_CFLAGS) -DNDEBUG=1
_DEBUG_CFLAGS = -Og -std=gnu89 -Werror -Wno-long-long -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 $(_CFLAGS) $(DEBUG_CFLAGS) \
                -UNDEBUG

_LDFLAGS = -mconsole $(LDFLAGS)
_RELEASE_LDFLAGS = $(_LDFLAGS) $(RELEASE_LDFLAGS)
_DEBUG_LDFLAGS = $(_LDFLAGS) $(DEBUG_LDFLAGS)

sources = src/main/main.c src/main/service.c src/main/standalone.c src/main/wide_to_narrow.c \
          src/proxy/dbg_output_bytes.c src/proxy/logger.c src/proxy/loop.c src/proxy/name_to_path.c \
          src/proxy/pipe.c src/proxy/pipe_handler.c src/proxy/proxy.c src/proxy/socket.c src/proxy/socket_handler.c \
          src/proxy/wait_thread.c
headers = include/winestreamproxy/logger.h include/winestreamproxy/winestreamproxy.h src/main/service.h \
          src/main/standalone.h src/main/wide_to_narrow.h src/proxy/connection.h src/proxy/dbg_output_bytes.h \
          src/proxy/pipe.h src/proxy/proxy.h src/proxy/socket.h src/proxy/wait_thread.h

all: release
release: $(OUT)/winestreamproxy.exe.so $(OUT)/start.sh $(OUT)/wrapper.sh
debug: $(OUT)/winestreamproxy-debug.exe.so $(OUT)/start-debug.sh $(OUT)/wrapper-debug.sh

$(OUT)/winestreamproxy.exe.so $(OUT)/winestreamproxy.exe.dbg.o: $(sources) $(headers) Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) $(_RELEASE_CFLAGS) $(_RELEASE_LDFLAGS) -o $(OUT)/winestreamproxy.exe.so $(sources)
	$(OBJCOPY) --only-keep-debug $(OUT)/winestreamproxy.exe.so $(OUT)/winestreamproxy.exe.dbg.o
	$(STRIP) --strip-debug --strip-unneeded $(OUT)/winestreamproxy.exe.so
	$(OBJCOPY) --add-gnu-debuglink=$(OUT)/winestreamproxy.exe.dbg.o $(OUT)/winestreamproxy.exe.so

$(OUT)/winestreamproxy-debug.exe.so: $(sources) $(headers) Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) $(_DEBUG_CFLAGS) $(_DEBUG_LDFLAGS) -o $(OUT)/winestreamproxy-debug.exe.so $(sources)

$(OUT)/settings.conf: scripts/settings.conf
	$(CP) scripts/settings.conf $(OUT)/settings.conf
	$(TOUCH) $(OUT)/settings.conf
$(OUT)/start.sh $(OUT)/start-debug.sh: scripts/start.sh $(OUT)/settings.conf
	$(CP) scripts/start.sh $@
	$(TOUCH) $@
$(OUT)/wrapper.sh: scripts/wrapper.sh $(OUT)/start.sh
	$(CP) scripts/wrapper.sh $(OUT)/wrapper.sh
	$(TOUCH) $(OUT)/wrapper.sh
$(OUT)/wrapper-debug.sh: scripts/wrapper.sh $(OUT)/start-debug.sh
	$(CP) scripts/wrapper.sh $(OUT)/wrapper-debug.sh
	$(TOUCH) $(OUT)/wrapper-debug.sh

clean:
	$(RM) $(OUT)/winestreamproxy.exe.so
	$(RM) $(OUT)/winestreamproxy.exe.dbg.o
	$(RM) $(OUT)/settings.conf
	$(RM) $(OUT)/start.sh
	$(RM) $(OUT)/wrapper.sh
	$(RM) $(OUT)/winestreamproxy-debug.exe.so
	$(RM) $(OUT)/start-debug.sh
	$(RM) $(OUT)/wrapper-debug.sh
	-$(RMDIR) $(OUT) 2>/dev/null || :

.PHONY: all release debug clean
.ONESHELL:
.POSIX:
