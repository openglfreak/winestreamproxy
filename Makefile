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
WINDRES = x86_64-w64-mingw32-windres
WINEBUILD = winebuild
WINEGCC = winegcc
CROSSCC = x86_64-w64-mingw32-gcc
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
          src/main/printf.c src/main/service.c src/main/standalone.c src/proxy/connection.c src/proxy/connection_list.c \
          src/proxy/misc.c src/proxy/name_to_path.c src/proxy/pipe.c src/proxy/proxy.c src/proxy/socket.c \
          src/proxy/thread.c
headers = include/winestreamproxy/logger.h include/winestreamproxy/winestreamproxy.h src/main/argparser.h \
          src/main/double_spawn.h src/main/misc.h src/main/printf.h src/main/service.h src/main/standalone.h \
          src/proxy/connection.h src/proxy/connection_list.h src/proxy/data/connection_data.h \
          src/proxy/data/connection_list.h src/proxy/misc.h src/proxy/data/pipe_data.h src/proxy/data/proxy_data.h \
          src/proxy/data/socket_data.h src/proxy/data/thread_data.h src/proxy/pipe.h  src/proxy/proxy.h \
          src/proxy/socket.h src/proxy/thread.h

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

$(OBJ)/version.exe: $(OBJ)/version.res
	$(MKDIR) $(OBJ)
	$(WINDRES) -O coff -i $(OBJ)/version.res -o $(OBJ)/version.exe

$(OUT)/.version: $(OBJ)/.version
	$(MKDIR) $(OUT)
	$(CP) $(OBJ)/.version $(OUT)/.version
	$(TOUCH) $(OUT)/.version

$(OBJ)/winestreamproxy_unixlib.a: $(spec_unixlib) Makefile
	$(MKDIR) $(OBJ)
	$(WINEBUILD) -b x86_64-w64-mingw32 --implib -o $(OBJ)/winestreamproxy_unixlib.a --export $(spec_unixlib)

$(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy_unixlib.dll.dbg.o: \
        $(OUT)/.version $(OBJ)/version.h $(OBJ)/version.res $(OBJ)/winestreamproxy_unixlib.a $(spec_unixlib) \
        $(sources_unixlib) $(headers_unixlib) Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_CPPFLAGS) $(_RELEASE_CFLAGS) $(_RELEASE_LDFLAGS) -nodefaultlibs -lkernel32 \
	           $(OBJ)/version.res -shared -o $(OUT)/winestreamproxy_unixlib.dll.so $(spec_unixlib) $(sources_unixlib)
	$(OBJCOPY) --only-keep-debug $(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy_unixlib.dll.dbg.o
	$(STRIP) --strip-debug --strip-unneeded $(OUT)/winestreamproxy_unixlib.dll.so
	$(OBJCOPY) --add-gnu-debuglink=$(OUT)/winestreamproxy_unixlib.dll.dbg.o $(OUT)/winestreamproxy_unixlib.dll.so

$(OUT)/winestreamproxy.exe $(OUT)/winestreamproxy.exe.dbg.o: $(OUT)/.version $(OBJ)/version.h $(OBJ)/version.exe \
                                                                $(sources) $(headers) Makefile \
                                                                $(OBJ)/winestreamproxy_unixlib.a
	$(MKDIR) $(OUT)
	$(CROSSCC) -include $(OBJ)/version.h $(_CPPFLAGS) $(_RELEASE_CFLAGS) $(_RELEASE_LDFLAGS) \
	           $(OBJ)/version.exe -o $(OUT)/winestreamproxy.exe $(sources) $(OBJ)/winestreamproxy_unixlib.a
	$(OBJCOPY) --only-keep-debug $(OUT)/winestreamproxy.exe $(OUT)/winestreamproxy.exe.dbg.o
	$(STRIP) --strip-debug --strip-unneeded $(OUT)/winestreamproxy.exe
	$(OBJCOPY) --add-gnu-debuglink=$(OUT)/winestreamproxy.exe.dbg.o $(OUT)/winestreamproxy.exe

$(OBJ)/version-debug.res: $(OBJ)/version.h src/version.rc
	$(MKDIR) $(OBJ)
	cat $(OBJ)/version.h src/version.rc | $(WRC) $(_DEBUG_CPPFLAGS) $(_DEBUG_WRCFLAGS) -o $(OBJ)/version-debug.res

$(OBJ)/version-debug.exe: $(OBJ)/version-debug.res
	$(MKDIR) $(OBJ)
	$(WINDRES) -O coff -i $(OBJ)/version-debug.res -o $(OBJ)/version-debug.exe

$(OUT)/.version-debug: $(OBJ)/.version
	$(MKDIR) $(OUT)
	$(CP) $(OBJ)/.version $(OUT)/.version-debug
	$(TOUCH) $(OUT)/.version-debug

$(OBJ)/winestreamproxy_unixlib-debug.a: $(spec_unixlib) Makefile
	$(MKDIR) $(OBJ)
	$(WINEBUILD) -b x86_64-w64-mingw32 --implib -w -o $(OBJ)/winestreamproxy_unixlib-debug.a --export $(spec_unixlib)

$(OUT)/winestreamproxy_unixlib-debug.dll.so: $(OUT)/.version-debug $(OBJ)/version.h $(OBJ)/version-debug.res \
                                             $(spec_unixlib) $(sources_unixlib) $(headers_unixlib) Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_CPPFLAGS) $(_DEBUG_CFLAGS) $(_DEBUG_LDFLAGS) -nodefaultlibs -lkernel32 \
	           $(OBJ)/version-debug.res -shared -o $(OUT)/winestreamproxy_unixlib-debug.dll.so $(spec_unixlib) \
	           $(sources_unixlib)

$(OUT)/winestreamproxy-debug.exe: $(OUT)/.version-debug $(OBJ)/version.h $(OBJ)/version-debug.exe \
                                     $(sources) $(headers) Makefile $(OBJ)/winestreamproxy_unixlib-debug.a
	$(MKDIR) $(OUT)
	$(CROSSCC) -include $(OBJ)/version.h $(_CPPFLAGS) $(_DEBUG_CFLAGS) $(_DEBUG_LDFLAGS) $(OBJ)/version-debug.exe \
	           -o $(OUT)/winestreamproxy-debug.exe $(sources) $(OBJ)/winestreamproxy_unixlib-debug.a

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

$(OUT)/release.tar.gz: $(OUT)/.version $(OUT)/winestreamproxy.exe $(OUT)/winestreamproxy.exe.dbg.o \
                       $(OUT)/settings.conf $(OUT)/start.sh $(OUT)/wrapper.sh Makefile
	cd $(OUT) && \
	$(TAR) release.tar.gz .version winestreamproxy.exe winestreamproxy.exe.dbg.o \
	                      settings.conf start.sh wrapper.sh
$(OUT)/debug.tar.gz: $(OUT)/.version-debug $(OUT)/winestreamproxy-debug.exe $(OUT)/settings.conf \
                     $(OUT)/start-debug.sh $(OUT)/wrapper-debug.sh Makefile
	cd $(OUT) && \
	$(TAR) debug.tar.gz .version-debug winestreamproxy-debug.exe settings.conf \
	                    start-debug.sh wrapper-debug.sh
$(OUT)/source.tar.gz: gen-version.sh $(OBJ)/version.h src/version.rc $(OBJ)/version.res $(OBJ)/.version $(sources) \
                      $(headers) Makefile
	$(TAR) $(OUT)/source.tar.gz gen-version.sh $(OBJ)/version.h src/version.rc $(OBJ)/version.res $(OBJ)/.version \
	                            $(sources) $(headers) Makefile

install: install-release
install-release: $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe \
                 $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.dbg.o $(PREFIX)/lib/winestreamproxy/settings.conf \
                 $(PREFIX)/bin/winestreamproxy $(PREFIX)/bin/winestreamproxy-wrapper
install-debug: $(PREFIX)/lib/winestreamproxy/winestreamproxy-debug.exe $(PREFIX)/lib/winestreamproxy/settings.conf \
               $(PREFIX)/bin/winestreamproxy-debug $(PREFIX)/bin/winestreamproxy-wrapper-debug

$(PREFIX)/lib/winestreamproxy/winestreamproxy.exe $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.dbg.o: \
        $(OUT)/winestreamproxy.exe $(OUT)/winestreamproxy.exe.dbg.o
	$(MKDIR) $(PREFIX)/lib/winestreamproxy
	$(CP) $(OUT)/winestreamproxy.exe $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe
	$(TOUCH) $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe
	$(CP) $(OUT)/winestreamproxy.exe.dbg.o $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.dbg.o
	$(TOUCH) $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.dbg.o

$(PREFIX)/lib/winestreamproxy/winestreamproxy-debug.exe: $(OUT)/winestreamproxy-debug.exe
	$(MKDIR) $(PREFIX)/lib/winestreamproxy
	$(CP) $(OUT)/winestreamproxy-debug.exe $(PREFIX)/lib/winestreamproxy/winestreamproxy-debug.exe
	$(TOUCH) $(PREFIX)/lib/winestreamproxy/winestreamproxy-debug.exe

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
	$(RM) $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe
	$(RM) $(PREFIX)/lib/winestreamproxy/winestreamproxy.exe.dbg.o
	$(RM) $(PREFIX)/lib/winestreamproxy/settings.conf
	-$(RMDIR) $(PREFIX)/lib/winestreamproxy 2>/dev/null || :
	$(RM) $(PREFIX)/bin/winestreamproxy
	$(RM) $(PREFIX)/bin/winestreamproxy-wrapper
uninstall-debug:
	$(RM) $(PREFIX)/lib/winestreamproxy/winestreamproxy-debug.exe
	$(RM) $(PREFIX)/lib/winestreamproxy/settings.conf
	-$(RMDIR) $(PREFIX)/lib/winestreamproxy 2>/dev/null || :
	$(RM) $(PREFIX)/bin/winestreamproxy-debug
	$(RM) $(PREFIX)/bin/winestreamproxy-wrapper-debug

clean:
	$(RM) $(OBJ)/.version
	$(RM) $(OBJ)/version.h
	$(RM) $(OBJ)/version.res
	$(RM) $(OBJ)/version.exe
	$(RM) $(OUT)/.version
	$(RM) $(OBJ)/winestreamproxy_unixlib.a
	$(RM) $(OUT)/winestreamproxy_unixlib.dll.so
	$(RM) $(OUT)/winestreamproxy_unixlib.dll.dbg.o
	$(RM) $(OUT)/winestreamproxy.exe
	$(RM) $(OUT)/winestreamproxy.exe.dbg.o
	$(RM) $(OBJ)/version-debug.res
	$(RM) $(OBJ)/version-debug.exe
	$(RM) $(OUT)/.version-debug
	$(RM) $(OBJ)/winestreamproxy_unixlib-debug.a
	$(RM) $(OUT)/winestreamproxy_unixlib-debug.dll.so
	$(RM) $(OUT)/winestreamproxy-debug.exe
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
