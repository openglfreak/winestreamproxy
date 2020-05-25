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

WINEGCC ?= winegcc
RM ?= rm -f
OBJCOPY ?= x86_64-w64-mingw32-objcopy
STRIP ?= x86_64-w64-mingw32-strip

CFLAGS := -g3 -gdwarf-4 -fPIC -pie -Iinclude -mconsole -DWIN32_LEAN_AND_MEAN=1 -Wall -Wextra -pedantic -pipe $(CFLAGS)
DEBUG_CFLAGS = -Og -std=gnu89 -Werror $(CFLAGS) -UNDEBUG
RELEASE_CFLAGS = -O2 -fuse-linker-plugin -fuse-ld=gold -fgraphite-identity -floop-nest-optimize -fipa-pta \
                 -fno-semantic-interposition -fno-common -fno-plt $(CFLAGS) -DNDEBUG=1

CFLAGS := $(CFLAGS) -include stdarg.h # Work around Wine headers bug in 32-bit mode.

sources := src/main/main.c src/main/service.c src/main/standalone.c src/main/wide_to_narrow.c \
           src/proxy/dbg_output_bytes.c src/proxy/logger.c src/proxy/loop.c src/proxy/name_to_path.c \
           src/proxy/pipe.c src/proxy/pipe_handler.c src/proxy/proxy.c src/proxy/socket.c src/proxy/socket_handler.c \
           src/proxy/wait_thread.c
headers := include/winestreamproxy/logger.h include/winestreamproxy/winestreamproxy.h src/main/service.h \
           src/main/standalone.h src/main/wide_to_narrow.h src/proxy/connection.h src/proxy/dbg_output_bytes.h \
           src/proxy/pipe.h src/proxy/proxy.h src/proxy/socket.h src/proxy/wait_thread.h
main_file := src/main/main.c

all: winestreamproxy.exe.so

winestreamproxy.exe.so: $(sources) $(headers) Makefile
	$(WINEGCC) $(RELEASE_CFLAGS) $(LDFLAGS) -o winestreamproxy.exe.so $(filter-out $(main_file),$(sources)) $(main_file)
	$(OBJCOPY) --only-keep-debug winestreamproxy.exe.so winestreamproxy.exe.dbg.o
	$(STRIP) --strip-debug --strip-unneeded winestreamproxy.exe.so
	$(OBJCOPY) --add-gnu-debuglink="$${PWD:-$$(pwd)}/winestreamproxy.exe.dbg.o" winestreamproxy.exe.so

winestreamproxy-debug.exe.so: $(sources) $(headers) Makefile
	$(WINEGCC) $(DEBUG_CFLAGS) $(LDFLAGS) -o winestreamproxy-debug.exe.so $(filter-out $(main_file),$(sources)) $(main_file)

clean:
	$(RM) winestreamproxy.exe.so
	$(RM) winestreamproxy-debug.exe.so
	$(RM) winestreamproxy.exe.dbg.o

.PHONY: all clean
