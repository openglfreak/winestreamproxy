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
.POSIX:

OUT = out
OBJ = obj
PREFIX = /usr/local

MKDIR = mkdir -p --
WRC = wrc
WINEGCC = winegcc
OBJCOPY = objcopy
STRIP = strip
CP = cp -LRpf --
TOUCH = touch --
RM = rm -f --
RMDIR = rmdir --
TAR = tar -czf
AWK = awk
REPLACE = $(AWK) -- 'BEGIN { FS = ARGV[1]; delete ARGV[1]; OFS = ARGV[2]; delete ARGV[2]; } { $$1 = $$1; print; }'
CHMODX = chmod -- a+x

_include_stdarg = -include stdarg.h  # Work around Wine headers bug in 32-bit mode.

_CPPFLAGS = -DWIN32_LEAN_AND_MEAN=1 $(CPPFLAGS)
_RELEASE_CPPFLAGS = -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 $(_CPPFLAGS) $(RELEASE_CPPFLAGS) -UNDEBUG -DNDEBUG=1
_DEBUG_CPPFLAGS = -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 $(_CPPFLAGS) $(DEBUG_CPPFLAGS) -UNDEBUG

_WRCFLAGS = $(WRCFLAGS)
_RELEASE_WRCFLAGS = $(RELEASE_WRCFLAGS)
_DEBUG_WRCFLAGS = $(DEBUG_WRCFLAGS)

_CFLAGS = -g3 -gdwarf-4 -fPIC -pie -Iinclude -Wall -Wextra -pedantic -pipe $(CFLAGS) $(_include_stdarg)
_RELEASE_CFLAGS = -O2 -fomit-frame-pointer -fno-stack-protector -fuse-linker-plugin -fuse-ld=gold -fgraphite-identity \
                  -floop-nest-optimize -fipa-pta -fno-semantic-interposition -fno-common -fdevirtualize-at-ltrans \
                  -fno-plt -fgcse-after-reload -fipa-cp-clone -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
                  -ffile-prefix-map="$${PWD:-$$(pwd)}"=. $(_CFLAGS) $(RELEASE_CFLAGS) -DNDEBUG=1
_DEBUG_CFLAGS = -Og -std=gnu89 -Werror -Wno-long-long -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 $(_CFLAGS) $(DEBUG_CFLAGS) \
                -UNDEBUG

_LDFLAGS = -mconsole $(LDFLAGS)
_RELEASE_LDFLAGS = $(_LDFLAGS) $(RELEASE_LDFLAGS)
_DEBUG_LDFLAGS = $(_LDFLAGS) $(DEBUG_LDFLAGS)

sources = src/main/double_spawn.c src/main/main.c src/main/service.c src/main/standalone.c src/main/wide_to_narrow.c \
          src/proxy/dbg_output_bytes.c src/proxy/logger.c src/proxy/loop.c src/proxy/name_to_path.c \
          src/proxy/pipe.c src/proxy/pipe_handler.c src/proxy/proxy.c src/proxy/socket.c src/proxy/socket_handler.c \
          src/proxy/wait_thread.c
headers = include/winestreamproxy/logger.h include/winestreamproxy/winestreamproxy.h src/main/double_spawn.h \
          src/main/service.h src/main/standalone.h src/main/wide_to_narrow.h src/proxy/connection.h \
          src/proxy/dbg_output_bytes.h src/proxy/pipe.h src/proxy/proxy.h src/proxy/socket.h src/proxy/wait_thread.h

all: release
release: $(OUT)/winestreamproxy.exe.so $(OUT)/start.sh $(OUT)/wrapper.sh
debug: $(OUT)/winestreamproxy-debug.exe.so $(OUT)/start-debug.sh $(OUT)/wrapper-debug.sh

$(OBJ)/version.h $(OBJ)/.version: Makefile gen-version.sh
	$(MKDIR) $(OBJ)
	. ./gen-version.sh && get_version && \
	output_version_defines >$(OBJ)/version.h && \
	output_version_assignments >$(OBJ)/.version

$(OBJ)/version.res: $(OBJ)/version.h src/version.rc
	$(MKDIR) $(OBJ)
	cat $(OBJ)/version.h src/version.rc | $(WRC) $(_RELEASE_CPPFLAGS) $(_RELEASE_WRCFLAGS) -o $(OBJ)/version.res

$(OUT)/winestreamproxy.exe.so $(OUT)/winestreamproxy.exe.dbg.o $(OUT)/.version: $(OBJ)/version.h $(OBJ)/version.res \
                              $(OBJ)/.version $(sources) $(headers) Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_CPPFLAGS) $(_RELEASE_CFLAGS) $(_RELEASE_LDFLAGS) \
	           $(OBJ)/version.res -o $(OUT)/winestreamproxy.exe.so $(sources)
	$(CP) $(OBJ)/.version $(OUT)/.version
	$(TOUCH) $(OUT)/.version
	$(OBJCOPY) --only-keep-debug $(OUT)/winestreamproxy.exe.so $(OUT)/winestreamproxy.exe.dbg.o
	$(STRIP) --strip-debug --strip-unneeded $(OUT)/winestreamproxy.exe.so
	$(OBJCOPY) --add-gnu-debuglink=$(OUT)/winestreamproxy.exe.dbg.o $(OUT)/winestreamproxy.exe.so

$(OBJ)/version-debug.res: $(OBJ)/version.h src/version.rc
	$(MKDIR) $(OBJ)
	cat $(OBJ)/version.h src/version.rc | $(WRC) $(_DEBUG_CPPFLAGS) $(_DEBUG_WRCFLAGS) -o $(OBJ)/version-debug.res

$(OUT)/winestreamproxy-debug.exe.so $(OUT)/.version-debug: $(OBJ)/version.h $(OBJ)/version-debug.res $(OBJ)/.version \
                                     $(sources) $(headers) Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_CPPFLAGS) $(_DEBUG_CFLAGS) $(_DEBUG_LDFLAGS) \
	           $(OBJ)/version-debug.res -o $(OUT)/winestreamproxy-debug.exe.so $(sources)
	$(CP) $(OBJ)/.version $(OUT)/.version-debug
	$(TOUCH) $(OUT)/.version-debug

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

release-tarball: $(OUT)/release.tar.gz
debug-tarball: $(OUT)/debug.tar.gz
source-tarball: $(OUT)/source.tar.gz

$(OUT)/release.tar.gz: $(OUT)/.version $(OUT)/winestreamproxy.exe.so $(OUT)/winestreamproxy.exe.dbg.o \
                       $(OUT)/settings.conf $(OUT)/start.sh $(OUT)/wrapper.sh Makefile
	cd $(OUT) && \
	$(TAR) release.tar.gz .version winestreamproxy.exe.so winestreamproxy.exe.dbg.o \
	                      settings.conf start.sh wrapper.sh
$(OUT)/debug.tar.gz: $(OUT)/.version-debug $(OUT)/winestreamproxy-debug.exe.so $(OUT)/settings.conf \
                     $(OUT)/start-debug.sh $(OUT)/wrapper-debug.sh Makefile
	cd $(OUT) && \
	$(TAR) debug.tar.gz .version-debug winestreamproxy-debug.exe.so settings.conf \
	                    start-debug.sh wrapper-debug.sh
$(OUT)/source.tar.gz: gen-version.sh $(OBJ)/version.h src/version.rc $(OBJ)/version.res $(OBJ)/.version $(sources) \
                      $(headers) Makefile
	$(TAR) $(OUT)/source.tar.gz gen-version.sh $(OBJ)/version.h src/version.rc $(OBJ)/version.res $(OBJ)/.version \
	                            $(sources) $(headers) Makefile

install: install-release
install-release: $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.so \
                 $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.dbg.o $(PREFIX)/lib/winestreamproxy/settings.conf \
                 $(PREFIX)/bin/winestreamproxy $(PREFIX)/bin/winestreamproxy-wrapper
install-debug: $(PREFIX)/lib/winestreamproxy/winestreamproxy-debug.exe.so $(PREFIX)/lib/winestreamproxy/settings.conf \
               $(PREFIX)/bin/winestreamproxy-debug $(PREFIX)/bin/winestreamproxy-wrapper-debug

$(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.so $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.dbg.o: \
        $(OUT)/winestreamproxy.exe.so $(OUT)/winestreamproxy.exe.dbg.o
	$(MKDIR) $(PREFIX)/lib/winestreamproxy
	$(CP) $(OUT)/winestreamproxy.exe.so $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.so
	$(TOUCH) $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.so
	$(CP) $(OUT)/winestreamproxy.exe.dbg.o $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.dbg.o
	$(TOUCH) $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.dbg.o

$(PREFIX)/lib/winestreamproxy/winestreamproxy-debug.exe.so: $(OUT)/winestreamproxy-debug.exe.so
	$(MKDIR) $(PREFIX)/lib/winestreamproxy
	$(CP) $(OUT)/winestreamproxy-debug.exe.so $(PREFIX)/lib/winestreamproxy/winestreamproxy-debug.exe.so
	$(TOUCH) $(PREFIX)/lib/winestreamproxy/winestreamproxy-debug.exe.so

$(PREFIX)/lib/winestreamproxy/settings.conf: $(OUT)/settings.conf
	$(MKDIR) $(PREFIX)/lib/winestreamproxy
	$(CP) $(OUT)/settings.conf $(PREFIX)/lib/winestreamproxy/settings.conf
	$(TOUCH) $(PREFIX)/lib/winestreamproxy/settings.conf

$(PREFIX)/bin/winestreamproxy: $(OUT)/start.sh $(PREFIX)/lib/winestreamproxy/settings.conf
	$(MKDIR) $(PREFIX)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/start.sh | $(REPLACE) @prefix_defined@ true > $(PREFIX)/bin/winestreamproxy
	$(CHMODX) $(PREFIX)/bin/winestreamproxy
$(PREFIX)/bin/winestreamproxy-wrapper: $(OUT)/wrapper.sh $(PREFIX)/bin/winestreamproxy
	$(MKDIR) $(PREFIX)/bin
	$(CP) $(OUT)/wrapper.sh $(PREFIX)/bin/winestreamproxy-wrapper
	$(TOUCH) $(PREFIX)/bin/winestreamproxy-wrapper

$(PREFIX)/bin/winestreamproxy-debug: $(OUT)/start-debug.sh $(PREFIX)/lib/winestreamproxy/settings.conf
	$(MKDIR) $(PREFIX)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/start-debug.sh | $(REPLACE) @prefix_defined@ true > \
	    $(PREFIX)/bin/winestreamproxy-debug
	$(CHMODX) $(PREFIX)/bin/winestreamproxy-debug
$(PREFIX)/bin/winestreamproxy-wrapper-debug: $(OUT)/wrapper-debug.sh $(PREFIX)/bin/winestreamproxy-debug
	$(MKDIR) $(PREFIX)/bin
	$(CP) $(OUT)/wrapper-debug.sh $(PREFIX)/bin/winestreamproxy-wrapper-debug
	$(TOUCH) $(PREFIX)/bin/winestreamproxy-wrapper-debug

uninstall: uninstall-release uninstall-debug
uninstall-release:
	$(RM) $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.so
	$(RM) $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.dbg.o
	$(RM) $(PREFIX)/lib/winestreamproxy/settings.conf
	-$(RMDIR) $(PREFIX)/lib/winestreamproxy 2>/dev/null || :
	$(RM) $(PREFIX)/bin/winestreamproxy
	$(RM) $(PREFIX)/bin/winestreamproxy-wrapper
uninstall-debug:
	$(RM) $(PREFIX)/lib/winestreamproxy/winestreamproxy-debug.exe.so
	$(RM) $(PREFIX)/lib/winestreamproxy/settings.conf
	-$(RMDIR) $(PREFIX)/lib/winestreamproxy 2>/dev/null || :
	$(RM) $(PREFIX)/bin/winestreamproxy-debug
	$(RM) $(PREFIX)/bin/winestreamproxy-wrapper-debug

clean:
	$(RM) $(OBJ)/.version
	$(RM) $(OBJ)/version.h
	$(RM) $(OBJ)/version.res
	$(RM) $(OBJ)/version-debug.res
	$(RM) $(OUT)/.version
	$(RM) $(OUT)/.version-debug
	$(RM) $(OUT)/winestreamproxy.exe.so
	$(RM) $(OUT)/winestreamproxy.exe.dbg.o
	$(RM) $(OUT)/winestreamproxy-debug.exe.so
	$(RM) $(OUT)/settings.conf
	$(RM) $(OUT)/start.sh
	$(RM) $(OUT)/wrapper.sh
	$(RM) $(OUT)/start-debug.sh
	$(RM) $(OUT)/wrapper-debug.sh
	$(RM) $(OUT)/release.tar.gz
	$(RM) $(OUT)/debug.tar.gz
	$(RM) $(OUT)/source.tar.gz
	-$(RMDIR) $(OBJ) 2>/dev/null || :
	-$(RMDIR) $(OUT) 2>/dev/null || :

.PHONY: all release debug release-tarball debug-tarball source-tarball install install-release install-debug uninstall \
        uninstall-release uninstall-debug clean
.ONESHELL:
