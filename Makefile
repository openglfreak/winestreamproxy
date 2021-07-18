#!/usr/bin/make -f
# Copyright (C) 2020-2021 Torge Matthies
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
DESTDIR = /usr/local
PREFIX = $(DESTDIR)

CROSSTARGET = x86_64-w64-mingw32

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

_CPPFLAGS = -DWIN32_LEAN_AND_MEAN=1 -DWINVER=0x0600 -D_WIN32_WINNT=0x0600 -DPSAPI_VERSION=2 $(CPPFLAGS)
_RELEASE_CPPFLAGS = -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 $(_CPPFLAGS) $(RELEASE_CPPFLAGS) -UNDEBUG -DNDEBUG=1
_DEBUG_CPPFLAGS = -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 $(_CPPFLAGS) $(DEBUG_CPPFLAGS) -UNDEBUG

_WRCFLAGS = $(WRCFLAGS)
_RELEASE_WRCFLAGS = $(RELEASE_WRCFLAGS)
_DEBUG_WRCFLAGS = $(DEBUG_WRCFLAGS)

_CFLAGS = -g3 -gdwarf-4 -fPIC -pie -Iinclude -Wall -Wextra -pedantic -pipe $(CFLAGS) $(_include_stdarg)
_RELEASE_CFLAGS = -O2 -fomit-frame-pointer -fno-stack-protector -fuse-linker-plugin -fgraphite-identity \
                  -floop-nest-optimize -fipa-pta -fno-semantic-interposition -fno-common -fdevirtualize-at-ltrans \
                  -fno-plt -fgcse-after-reload -fipa-cp-clone -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
                  -ffile-prefix-map="$${PWD:-$$(pwd)}"=. $(_CFLAGS) $(RELEASE_CFLAGS) -DNDEBUG=1
_DEBUG_CFLAGS = -Og -std=gnu89 -Werror -Wno-long-long -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 $(_CFLAGS) $(DEBUG_CFLAGS) \
                -UNDEBUG

_LDFLAGS = -mconsole $(LDFLAGS)
_RELEASE_LDFLAGS = $(_LDFLAGS) $(RELEASE_LDFLAGS)
_DEBUG_LDFLAGS = $(_LDFLAGS) $(DEBUG_LDFLAGS)

sources = src/logger/logger.c src/main/argparser.c src/main/double_spawn.c src/main/main.c src/main/misc.c \
          src/main/service.c src/main/standalone.c src/proxy/connection.c src/proxy/connection_list.c src/proxy/misc.c \
          src/proxy/name_to_path.c src/proxy/pipe.c src/proxy/proxy.c src/proxy/socket.c src/proxy/thread.c
headers = include/winestreamproxy/logger.h include/winestreamproxy/winestreamproxy.h src/main/argparser.h \
          src/main/double_spawn.h src/main/misc.h src/main/service.h src/main/standalone.h src/proxy/connection.h \
          src/proxy/connection_list.h src/proxy/data/connection_data.h src/proxy/data/connection_list.h \
          src/proxy/misc.h src/proxy/data/pipe_data.h src/proxy/data/proxy_data.h src/proxy/data/socket_data.h \
          src/proxy/data/thread_data.h src/proxy/pipe.h  src/proxy/proxy.h src/proxy/socket.h src/proxy/thread.h

spec_unixlib = src/proxy_unixlib/winestreamproxy_unixlib.def
sources_unixlib = src/proxy_unixlib/main.c src/proxy_unixlib/socket.c
headers_unixlib = src/proxy_unixlib/socket.h

all: release
release: $(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy.exe $(OUT)/start.sh $(OUT)/wrapper.sh
debug: $(OUT)/winestreamproxy_unixlib-debug.dll.so $(OUT)/winestreamproxy-debug.exe $(OUT)/start-debug.sh \
       $(OUT)/wrapper-debug.sh

$(OBJ)/version.h $(OBJ)/.version: Makefile gen-version.sh
	$(MKDIR) $(OBJ)
	. ./gen-version.sh && get_version && \
	output_version_defines >$(OBJ)/version.h && \
	output_version_assignments >$(OBJ)/.version

$(OBJ)/version.res: $(OBJ)/version.h src/version.rc
	$(MKDIR) $(OBJ)
	cat $(OBJ)/version.h src/version.rc | $(WRC) $(_RELEASE_CPPFLAGS) $(_RELEASE_WRCFLAGS) -o $(OBJ)/version.res

$(OUT)/.version: $(OBJ)/.version
	$(MKDIR) $(OUT)
	$(CP) $(OBJ)/.version $(OUT)/.version
	$(TOUCH) $(OUT)/.version

$(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy_unixlib.dll.dbg.o: \
        $(OUT)/.version $(OBJ)/version.h $(OBJ)/version.res $(spec_unixlib) $(sources_unixlib) $(headers_unixlib) \
        Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_CPPFLAGS) $(_RELEASE_CFLAGS) $(_RELEASE_LDFLAGS) $(OBJ)/version.res \
	           -shared -o $(OUT)/winestreamproxy_unixlib.dll.so $(spec_unixlib) $(sources_unixlib)
	$(OBJCOPY) --only-keep-debug $(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy_unixlib.dll.dbg.o
	$(STRIP) --strip-debug --strip-unneeded $(OUT)/winestreamproxy_unixlib.dll.so
	$(OBJCOPY) --add-gnu-debuglink=$(OUT)/winestreamproxy_unixlib.dll.dbg.o $(OUT)/winestreamproxy_unixlib.dll.so

$(OUT)/winestreamproxy.exe $(OUT)/winestreamproxy.exe.dbg.o: $(OUT)/.version $(OBJ)/version.h $(OBJ)/version.res \
                                                             $(sources) $(headers) Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_CPPFLAGS) $(_RELEASE_CFLAGS) $(_RELEASE_LDFLAGS) -mno-cygwin \
	           -b $(CROSSTARGET) $(OBJ)/version.res -o $(OUT)/winestreamproxy.exe $(sources)
	$(OBJCOPY) --only-keep-debug $(OUT)/winestreamproxy.exe $(OUT)/winestreamproxy.exe.dbg.o
	$(STRIP) --strip-debug --strip-unneeded $(OUT)/winestreamproxy.exe
	$(OBJCOPY) --add-gnu-debuglink=$(OUT)/winestreamproxy.exe.dbg.o $(OUT)/winestreamproxy.exe

$(OBJ)/version-debug.res: $(OBJ)/version.h src/version.rc
	$(MKDIR) $(OBJ)
	cat $(OBJ)/version.h src/version.rc | $(WRC) $(_DEBUG_CPPFLAGS) $(_DEBUG_WRCFLAGS) -o $(OBJ)/version-debug.res

$(OUT)/.version-debug: $(OBJ)/.version
	$(MKDIR) $(OUT)
	$(CP) $(OBJ)/.version $(OUT)/.version-debug
	$(TOUCH) $(OUT)/.version-debug

$(OUT)/winestreamproxy_unixlib-debug.dll.so: $(OUT)/.version-debug $(OBJ)/version.h $(OBJ)/version-debug.res \
                                             $(spec_unixlib) $(sources_unixlib) $(headers_unixlib) Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_CPPFLAGS) $(_DEBUG_CFLAGS) $(_DEBUG_LDFLAGS) $(OBJ)/version-debug.res \
	           -shared -o $(OUT)/winestreamproxy_unixlib-debug.dll.so $(spec_unixlib) \
	           $(sources_unixlib)

$(OUT)/winestreamproxy-debug.exe: $(OUT)/.version-debug $(OBJ)/version.h $(OBJ)/version-debug.res $(sources) $(headers) \
                                  Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_CPPFLAGS) $(_DEBUG_CFLAGS) $(_DEBUG_LDFLAGS) -mno-cygwin \
	           -b $(CROSSTARGET) $(OBJ)/version-debug.res -o $(OUT)/winestreamproxy-debug.exe $(sources)

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

$(OUT)/release.tar.gz: $(OUT)/.version $(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy_unixlib.dll.dbg.o \
                       $(OUT)/winestreamproxy.exe $(OUT)/winestreamproxy.exe.dbg.o $(OUT)/settings.conf \
                       $(OUT)/start.sh $(OUT)/wrapper.sh Makefile
	cd $(OUT) && \
	$(TAR) release.tar.gz .version winestreamproxy_unixlib.dll.so winestreamproxy_unixlib.dll.dbg.o winestreamproxy.exe \
	                      winestreamproxy.exe.dbg.o settings.conf start.sh wrapper.sh
$(OUT)/debug.tar.gz: $(OUT)/.version-debug $(OUT)/winestreamproxy_unixlib-debug.dll.so $(OUT)/winestreamproxy-debug.exe \
                     $(OUT)/settings.conf $(OUT)/start-debug.sh $(OUT)/wrapper-debug.sh Makefile
	cd $(OUT) && \
	$(TAR) debug.tar.gz .version-debug winestreamproxy_unixlib-debug.dll.so winestreamproxy-debug.exe settings.conf \
	                    start-debug.sh wrapper-debug.sh

install: install-release
install-release: $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe \
                 $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe.dbg.o $(DESTDIR)/lib/winestreamproxy/settings.conf \
                 $(DESTDIR)/bin/winestreamproxy $(DESTDIR)/bin/winestreamproxy-wrapper
install-debug: $(DESTDIR)/lib/winestreamproxy/winestreamproxy-debug.exe $(DESTDIR)/lib/winestreamproxy/settings.conf \
               $(DESTDIR)/bin/winestreamproxy-debug $(DESTDIR)/bin/winestreamproxy-wrapper-debug

$(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.so \
$(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.dbg.o: \
        $(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy_unixlib.dll.dbg.o
	$(MKDIR) $(DESTDIR)/lib/winestreamproxy
	$(CP) $(OUT)/winestreamproxy_unixlib.dll.so $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.so
	$(TOUCH) $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.so
	$(CP) $(OUT)/winestreamproxy_unixlib.dll.dbg.o $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.dbg.o
	$(TOUCH) $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.dbg.o

$(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe.dbg.o: \
        $(OUT)/winestreamproxy.exe $(OUT)/winestreamproxy.exe.dbg.o \
        $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.so
	$(MKDIR) $(DESTDIR)/lib/winestreamproxy
	$(CP) $(OUT)/winestreamproxy.exe $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe
	$(TOUCH) $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe
	$(CP) $(OUT)/winestreamproxy.exe.dbg.o $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe.dbg.o
	$(TOUCH) $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe.dbg.o

$(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib-debug.dll.so: $(OUT)/winestreamproxy_unixlib-debug.dll.so
	$(MKDIR) $(DESTDIR)/lib/winestreamproxy
	$(CP) $(OUT)/winestreamproxy_unixlib-debug.dll.so \
	    $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib-debug.dll.so
	$(TOUCH) $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib-debug.dll.so

$(DESTDIR)/lib/winestreamproxy/winestreamproxy-debug.exe: $(OUT)/winestreamproxy-debug.exe \
        $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib-debug.dll.so
	$(MKDIR) $(DESTDIR)/lib/winestreamproxy
	$(CP) $(OUT)/winestreamproxy-debug.exe $(DESTDIR)/lib/winestreamproxy/winestreamproxy-debug.exe
	$(TOUCH) $(DESTDIR)/lib/winestreamproxy/winestreamproxy-debug.exe

$(DESTDIR)/lib/winestreamproxy/settings.conf: $(OUT)/settings.conf
	$(MKDIR) $(DESTDIR)/lib/winestreamproxy
	$(CP) $(OUT)/settings.conf $(DESTDIR)/lib/winestreamproxy/settings.conf
	$(TOUCH) $(DESTDIR)/lib/winestreamproxy/settings.conf

$(DESTDIR)/bin/winestreamproxy: $(OUT)/start.sh $(DESTDIR)/lib/winestreamproxy/settings.conf
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/start.sh | $(REPLACE) @prefix_defined@ true > $(DESTDIR)/bin/winestreamproxy
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy
$(DESTDIR)/bin/winestreamproxy-wrapper: $(OUT)/wrapper.sh $(DESTDIR)/bin/winestreamproxy
	$(MKDIR) $(DESTDIR)/bin
	$(CP) $(OUT)/wrapper.sh $(DESTDIR)/bin/winestreamproxy-wrapper
	$(TOUCH) $(DESTDIR)/bin/winestreamproxy-wrapper

$(DESTDIR)/bin/winestreamproxy-debug: $(OUT)/start-debug.sh $(DESTDIR)/lib/winestreamproxy/settings.conf
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/start-debug.sh | $(REPLACE) @prefix_defined@ true > \
	    $(DESTDIR)/bin/winestreamproxy-debug
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy-debug
$(DESTDIR)/bin/winestreamproxy-wrapper-debug: $(OUT)/wrapper-debug.sh $(DESTDIR)/bin/winestreamproxy-debug
	$(MKDIR) $(DESTDIR)/bin
	$(CP) $(OUT)/wrapper-debug.sh $(DESTDIR)/bin/winestreamproxy-wrapper-debug
	$(TOUCH) $(DESTDIR)/bin/winestreamproxy-wrapper-debug

uninstall: uninstall-release uninstall-debug
uninstall-release:
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.so
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.dbg.o
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe.dbg.o
	$(RM) $(DESTDIR)/lib/winestreamproxy/settings.conf
	-$(RMDIR) $(DESTDIR)/lib/winestreamproxy 2>/dev/null || :
	$(RM) $(DESTDIR)/bin/winestreamproxy
	$(RM) $(DESTDIR)/bin/winestreamproxy-wrapper
uninstall-debug:
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib-debug.dll.so
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy-debug.exe
	$(RM) $(DESTDIR)/lib/winestreamproxy/settings.conf
	-$(RMDIR) $(DESTDIR)/lib/winestreamproxy 2>/dev/null || :
	$(RM) $(DESTDIR)/bin/winestreamproxy-debug
	$(RM) $(DESTDIR)/bin/winestreamproxy-wrapper-debug

clean:
	$(RM) $(OBJ)/.version
	$(RM) $(OBJ)/version.h
	$(RM) $(OBJ)/version.res
	$(RM) $(OUT)/.version
	$(RM) $(OUT)/winestreamproxy_unixlib.dll.so
	$(RM) $(OUT)/winestreamproxy_unixlib.dll.dbg.o
	$(RM) $(OUT)/winestreamproxy.exe
	$(RM) $(OUT)/winestreamproxy.exe.dbg.o
	$(RM) $(OBJ)/version-debug.res
	$(RM) $(OUT)/.version-debug
	$(RM) $(OUT)/winestreamproxy_unixlib-debug.dll.so
	$(RM) $(OUT)/winestreamproxy-debug.exe
	$(RM) $(OUT)/settings.conf
	$(RM) $(OUT)/start.sh
	$(RM) $(OUT)/wrapper.sh
	$(RM) $(OUT)/start-debug.sh
	$(RM) $(OUT)/wrapper-debug.sh
	$(RM) $(OUT)/release.tar.gz
	$(RM) $(OUT)/debug.tar.gz
	-$(RMDIR) $(OBJ) 2>/dev/null || :
	-$(RMDIR) $(OUT) 2>/dev/null || :

.PHONY: all release debug release-tarball debug-tarball install install-release install-debug uninstall \
        uninstall-release uninstall-debug clean
.ONESHELL:
