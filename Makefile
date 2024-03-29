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
_RELEASE_CPPFLAGS_UNIX = $(_RELEASE_CPPFLAGS) $(RELEASE_CPPFLAGS_UNIX)
_RELEASE_CPPFLAGS_PE = $(_RELEASE_CPPFLAGS) $(RELEASE_CPPFLAGS_PE)
_DEBUG_CPPFLAGS_UNIX = $(_DEBUG_CPPFLAGS) $(DEBUG_CPPFLAGS_UNIX)
_DEBUG_CPPFLAGS_PE = $(_DEBUG_CPPFLAGS) $(DEBUG_CPPFLAGS_PE)

_WRCFLAGS = $(WRCFLAGS)
_RELEASE_WRCFLAGS = $(RELEASE_WRCFLAGS)
_DEBUG_WRCFLAGS = $(DEBUG_WRCFLAGS)

_CFLAGS = -g3 -gdwarf-4 -fPIC -pie -Iinclude -Wall -Wextra -pedantic -pipe $(CFLAGS) $(_include_stdarg)
_RELEASE_CFLAGS = -O3 -fomit-frame-pointer -fno-stack-protector -fuse-linker-plugin -fgraphite-identity \
                  -floop-nest-optimize -fipa-pta -fno-semantic-interposition -fno-common -fdevirtualize-at-ltrans \
                  -fno-plt -fgcse-after-reload -fipa-cp-clone -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
                  -ffile-prefix-map="$${PWD:-$$(pwd)}"=. $(_CFLAGS) $(RELEASE_CFLAGS) -DNDEBUG=1
_DEBUG_CFLAGS = -Og -std=gnu89 -Werror -Wno-long-long -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 $(_CFLAGS) $(DEBUG_CFLAGS) \
                -UNDEBUG
_RELEASE_CFLAGS_UNIX = $(_RELEASE_CFLAGS) -fuse-ld=gold $(RELEASE_CFLAGS_UNIX)
_RELEASE_CFLAGS_PE = $(_RELEASE_CFLAGS) -fwhole-program -flto=auto -fuse-linker-plugin -flto-partition=one \
                     $(RELEASE_CFLAGS_PE)
_DEBUG_CFLAGS_UNIX = $(_DEBUG_CFLAGS) $(DEBUG_CFLAGS_UNIX)
_DEBUG_CFLAGS_PE = $(_DEBUG_CFLAGS) $(DEBUG_CFLAGS_PE)

_LDFLAGS = -mconsole $(LDFLAGS)
_RELEASE_LDFLAGS = $(_LDFLAGS) $(RELEASE_LDFLAGS)
_DEBUG_LDFLAGS = $(_LDFLAGS) $(DEBUG_LDFLAGS)
_RELEASE_LDFLAGS_UNIX = $(_RELEASE_LDFLAGS) $(RELEASE_LDFLAGS_UNIX)
_RELEASE_LDFLAGS_PE = $(_RELEASE_CFLAGS_PE) $(_RELEASE_LDFLAGS) $(RELEASE_LDFLAGS_PE)
_DEBUG_LDFLAGS_UNIX = $(_DEBUG_LDFLAGS) $(DEBUG_LDFLAGS_UNIX)
_DEBUG_LDFLAGS_PE = $(_DEBUG_LDFLAGS) $(DEBUG_LDFLAGS_PE)

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
release: $(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy.exe $(OUT)/start.sh $(OUT)/stop.sh \
         $(OUT)/wrapper.sh $(OUT)/install.sh $(OUT)/uninstall.sh
debug: $(OUT)/winestreamproxy_unixlib-debug.dll.so $(OUT)/winestreamproxy-debug.exe $(OUT)/start-debug.sh \
       $(OUT)/stop-debug.sh $(OUT)/wrapper-debug.sh $(OUT)/install-debug.sh $(OUT)/uninstall-debug.sh

$(OBJ)/version.h $(OBJ)/.version: Makefile gen-version.sh
	$(MKDIR) $(OBJ)
	. ./gen-version.sh && get_version && \
	output_version_defines >$(OBJ)/version.h && \
	output_version_assignments >$(OBJ)/.version

$(OBJ)/version.res: $(OBJ)/version.h src/version.rc
	$(MKDIR) $(OBJ)
	cat $(OBJ)/version.h src/version.rc | $(WRC) $(_RELEASE_CPPFLAGS) $(_RELEASE_WRCFLAGS) -o $(OBJ)/version.res

$(OBJ)/winestreamproxy_unixlib_unity_source.c: $(sources_unixlib) $(headers_unixlib) Makefile
	$(MKDIR) $(OUT)
	for file in $(sources_unixlib); do \
		printf '#include "%s"\n' "$${PWD:-$$(pwd)}/$${file}"; \
	done >$(OBJ)/winestreamproxy_unixlib_unity_source.c

$(OBJ)/winestreamproxy_unity_source.c: $(sources) $(headers) Makefile
	$(MKDIR) $(OUT)
	for file in $(sources); do \
		printf '#include "%s"\n' "$${PWD:-$$(pwd)}/$${file}"; \
	done >$(OBJ)/winestreamproxy_unity_source.c

$(OUT)/.version: $(OBJ)/.version
	$(MKDIR) $(OUT)
	$(CP) $(OBJ)/.version $(OUT)/.version
	$(TOUCH) $(OUT)/.version

$(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy_unixlib.dll.dbg.o: \
        $(OUT)/.version $(OBJ)/version.h $(OBJ)/version.res $(spec_unixlib) \
        $(OBJ)/winestreamproxy_unixlib_unity_source.c Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_RELEASE_CPPFLAGS_UNIX) $(_RELEASE_CFLAGS_UNIX) $(_RELEASE_LDFLAGS_UNIX) \
	           $(OBJ)/version.res -shared -o $(OUT)/winestreamproxy_unixlib.dll.so $(spec_unixlib) \
	           $(OBJ)/winestreamproxy_unixlib_unity_source.c
	$(OBJCOPY) --only-keep-debug $(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy_unixlib.dll.dbg.o
	$(STRIP) --strip-debug --strip-unneeded $(OUT)/winestreamproxy_unixlib.dll.so
	$(OBJCOPY) --add-gnu-debuglink=$(OUT)/winestreamproxy_unixlib.dll.dbg.o $(OUT)/winestreamproxy_unixlib.dll.so

$(OUT)/winestreamproxy.exe $(OUT)/winestreamproxy.exe.dbg.o: $(OUT)/.version $(OBJ)/version.h $(OBJ)/version.res \
                                                             $(OBJ)/winestreamproxy_unity_source.c Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_RELEASE_CPPFLAGS_PE) $(_RELEASE_CFLAGS_PE) $(_RELEASE_LDFLAGS_PE) \
	           -mno-cygwin -b $(CROSSTARGET) $(OBJ)/version.res -o $(OUT)/winestreamproxy.exe \
	           $(OBJ)/winestreamproxy_unity_source.c
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
	$(WINEGCC) -include $(OBJ)/version.h $(_DEBUG_CPPFLAGS_UNIX) $(_DEBUG_CFLAGS_UNIX) $(_DEBUG_LDFLAGS_UNIX) \
	           $(OBJ)/version-debug.res -shared -o $(OUT)/winestreamproxy_unixlib-debug.dll.so $(spec_unixlib) \
	           $(sources_unixlib)

$(OUT)/winestreamproxy-debug.exe: $(OUT)/.version-debug $(OBJ)/version.h $(OBJ)/version-debug.res $(sources) $(headers) \
                                  Makefile
	$(MKDIR) $(OUT)
	$(WINEGCC) -include $(OBJ)/version.h $(_DEBUG_CPPFLAGS_PE) $(_DEBUG_CFLAGS_PE) $(_DEBUG_LDFLAGS_PE) -mno-cygwin \
	           -b $(CROSSTARGET) $(OBJ)/version-debug.res -o $(OUT)/winestreamproxy-debug.exe $(sources)

$(OUT)/settings.conf: scripts/settings.conf
	$(CP) scripts/settings.conf $(OUT)/settings.conf
	$(TOUCH) $(OUT)/settings.conf
$(OUT)/common.sh $(OUT)/common-debug.sh: scripts/common.sh $(OUT)/settings.conf $(OUT)/winestreamproxy.exe
	$(CP) scripts/common.sh $@
	$(TOUCH) $@
$(OUT)/start.sh: scripts/start.sh $(OUT)/common.sh
	$(REPLACE) @debug@ false scripts/start.sh >$(OUT)/start.sh
	$(CHMODX) $(OUT)/start.sh
$(OUT)/start-debug.sh: scripts/start.sh $(OUT)/common-debug.sh
	$(REPLACE) @debug@ true scripts/start.sh >$(OUT)/start-debug.sh
	$(CHMODX) $(OUT)/start-debug.sh
$(OUT)/stop.sh: scripts/stop.sh $(OUT)/common.sh
	$(REPLACE) @debug@ false scripts/stop.sh >$(OUT)/stop.sh
	$(CHMODX) $(OUT)/stop.sh
$(OUT)/stop-debug.sh: scripts/stop.sh $(OUT)/common-debug.sh
	$(REPLACE) @debug@ true scripts/stop.sh >$(OUT)/stop-debug.sh
	$(CHMODX) $(OUT)/stop-debug.sh
$(OUT)/wrapper.sh: scripts/wrapper.sh $(OUT)/start.sh
	$(REPLACE) @debug@ false scripts/wrapper.sh >$(OUT)/wrapper.sh
	$(CHMODX) $(OUT)/wrapper.sh
$(OUT)/wrapper-debug.sh: scripts/wrapper.sh $(OUT)/start-debug.sh
	$(REPLACE) @debug@ false scripts/wrapper.sh >$(OUT)/wrapper-debug.sh
	$(CHMODX) $(OUT)/wrapper-debug.sh
$(OUT)/install.sh: scripts/install.sh $(OUT)/common.sh
	$(REPLACE) @debug@ false scripts/install.sh >$(OUT)/install.sh
	$(CHMODX) $(OUT)/install.sh
$(OUT)/install-debug.sh: scripts/install.sh $(OUT)/common-debug.sh
	$(REPLACE) @debug@ true scripts/install.sh >$(OUT)/install-debug.sh
	$(CHMODX) $(OUT)/install-debug.sh
$(OUT)/uninstall.sh: scripts/uninstall.sh $(OUT)/common.sh
	$(REPLACE) @debug@ false scripts/uninstall.sh >$(OUT)/uninstall.sh
	$(CHMODX) $(OUT)/uninstall.sh
$(OUT)/uninstall-debug.sh: scripts/uninstall.sh $(OUT)/common-debug.sh
	$(REPLACE) @debug@ true scripts/uninstall.sh >$(OUT)/uninstall-debug.sh
	$(CHMODX) $(OUT)/uninstall-debug.sh

release-tarball: $(OUT)/release.tar.gz
debug-tarball: $(OUT)/debug.tar.gz

$(OUT)/release.tar.gz: $(OUT)/.version $(OUT)/winestreamproxy_unixlib.dll.so $(OUT)/winestreamproxy_unixlib.dll.dbg.o \
                       $(OUT)/winestreamproxy.exe $(OUT)/winestreamproxy.exe.dbg.o $(OUT)/settings.conf \
                       $(OUT)/common.sh $(OUT)/start.sh $(OUT)/stop.sh $(OUT)/wrapper.sh $(OUT)/install.sh \
                       $(OUT)/uninstall.sh Makefile
	cd $(OUT) && \
	$(TAR) release.tar.gz .version winestreamproxy_unixlib.dll.so winestreamproxy_unixlib.dll.dbg.o winestreamproxy.exe \
	                      winestreamproxy.exe.dbg.o settings.conf common.sh start.sh stop.sh wrapper.sh install.sh \
	                      uninstall.sh
$(OUT)/debug.tar.gz: $(OUT)/.version-debug $(OUT)/winestreamproxy_unixlib-debug.dll.so $(OUT)/winestreamproxy-debug.exe \
                     $(OUT)/settings.conf $(OUT)/common-debug.sh $(OUT)/start-debug.sh $(OUT)/stop-debug.sh \
                     $(OUT)/wrapper-debug.sh $(OUT)/install-debug.sh $(OUT)/uninstall-debug.sh Makefile
	cd $(OUT) && \
	$(TAR) debug.tar.gz .version-debug winestreamproxy_unixlib-debug.dll.so winestreamproxy-debug.exe settings.conf \
	                    common-debug.sh start-debug.sh stop-debug.sh wrapper-debug.sh install-debug.sh \
	                    uninstall-debug.sh

install: install-release
install-release: $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe \
                 $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe.dbg.o $(DESTDIR)/lib/winestreamproxy/settings.conf \
                 $(DESTDIR)/bin/winestreamproxy $(DESTDIR)/bin/winestreamproxy-stop \
                 $(DESTDIR)/bin/winestreamproxy-wrapper $(DESTDIR)/bin/winestreamproxy-install \
                 $(DESTDIR)/bin/winestreamproxy-uninstall
install-debug: $(DESTDIR)/lib/winestreamproxy/winestreamproxy-debug.exe $(DESTDIR)/lib/winestreamproxy/settings.conf \
               $(DESTDIR)/bin/winestreamproxy-debug $(DESTDIR)/bin/winestreamproxy-stop-debug \
               $(DESTDIR)/bin/winestreamproxy-wrapper-debug $(DESTDIR)/bin/winestreamproxy-install-debug \
               $(DESTDIR)/bin/winestreamproxy-uninstall-debug

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

$(DESTDIR)/lib/winestreamproxy/common.sh: $(OUT)/common.sh
	$(MKDIR) $(DESTDIR)/lib/winestreamproxy
	$(CP) $(OUT)/common.sh $(DESTDIR)/lib/winestreamproxy/common.sh
	$(TOUCH) $(DESTDIR)/lib/winestreamproxy/common.sh

$(DESTDIR)/bin/winestreamproxy: $(OUT)/start.sh $(DESTDIR)/lib/winestreamproxy/settings.conf \
                                $(DESTDIR)/lib/winestreamproxy/common.sh
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/start.sh | $(REPLACE) @prefix_defined@ true | $(REPLACE) @installed@ true \
	    >$(DESTDIR)/bin/winestreamproxy
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy
$(DESTDIR)/bin/winestreamproxy-stop: $(OUT)/stop.sh $(DESTDIR)/lib/winestreamproxy/settings.conf \
                                     $(DESTDIR)/lib/winestreamproxy/common.sh
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/stop.sh | $(REPLACE) @prefix_defined@ true | $(REPLACE) @installed@ true \
	    >$(DESTDIR)/bin/winestreamproxy-stop
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy-stop
$(DESTDIR)/bin/winestreamproxy-wrapper: $(OUT)/wrapper.sh $(DESTDIR)/bin/winestreamproxy
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/wrapper.sh | $(REPLACE) @prefix_defined@ true | $(REPLACE) @installed@ true \
	    >$(DESTDIR)/bin/winestreamproxy-wrapper
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy-wrapper
$(DESTDIR)/bin/winestreamproxy-install: $(OUT)/install.sh $(DESTDIR)/lib/winestreamproxy/settings.conf \
                                        $(DESTDIR)/lib/winestreamproxy/common.sh
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/install.sh | $(REPLACE) @prefix_defined@ true | $(REPLACE) @installed@ true \
	    >$(DESTDIR)/bin/winestreamproxy-install
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy-install
$(DESTDIR)/bin/winestreamproxy-uninstall: $(OUT)/uninstall.sh $(DESTDIR)/lib/winestreamproxy/common.sh
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/uninstall.sh | $(REPLACE) @prefix_defined@ true | $(REPLACE) @installed@ true \
	    >$(DESTDIR)/bin/winestreamproxy-uninstall
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy-uninstall

$(DESTDIR)/lib/winestreamproxy/common-debug.sh: $(OUT)/common-debug.sh
	$(MKDIR) $(DESTDIR)/lib/winestreamproxy
	$(CP) $(OUT)/common-debug.sh $(DESTDIR)/lib/winestreamproxy/common-debug.sh
	$(TOUCH) $(DESTDIR)/lib/winestreamproxy/common-debug.sh

$(DESTDIR)/bin/winestreamproxy-debug: $(OUT)/start-debug.sh $(DESTDIR)/lib/winestreamproxy/settings.conf \
                                      $(DESTDIR)/lib/winestreamproxy/common-debug.sh
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/start-debug.sh | $(REPLACE) @prefix_defined@ true | $(REPLACE) @installed@ \
	    true >$(DESTDIR)/bin/winestreamproxy-debug
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy-debug
$(DESTDIR)/bin/winestreamproxy-stop-debug: $(OUT)/stop-debug.sh $(DESTDIR)/lib/winestreamproxy/settings.conf \
                                           $(DESTDIR)/lib/winestreamproxy/common-debug.sh
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/stop-debug.sh | $(REPLACE) @prefix_defined@ true | $(REPLACE) @installed@ true \
	    >$(DESTDIR)/bin/winestreamproxy-stop-debug
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy-stop-debug
$(DESTDIR)/bin/winestreamproxy-wrapper-debug: $(OUT)/wrapper-debug.sh $(DESTDIR)/bin/winestreamproxy-debug
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/wrapper-debug.sh | $(REPLACE) @prefix_defined@ true | $(REPLACE) @installed@ \
	    true >$(DESTDIR)/bin/winestreamproxy-wrapper-debug
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy-wrapper-debug
$(DESTDIR)/bin/winestreamproxy-install-debug: $(OUT)/install-debug.sh $(DESTDIR)/lib/winestreamproxy/settings.conf \
                                        $(DESTDIR)/lib/winestreamproxy/common-debug.sh
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/install-debug.sh | $(REPLACE) @prefix_defined@ true | $(REPLACE) @installed@ \
	    true >$(DESTDIR)/bin/winestreamproxy-install-debug
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy-install-debug
$(DESTDIR)/bin/winestreamproxy-uninstall-debug: $(OUT)/uninstall-debug.sh $(DESTDIR)/lib/winestreamproxy/common-debug.sh
	$(MKDIR) $(DESTDIR)/bin
	$(REPLACE) @prefix@ $(PREFIX) $(OUT)/uninstall-debug.sh | $(REPLACE) @prefix_defined@ true | $(REPLACE) @installed@ \
	    true >$(DESTDIR)/bin/winestreamproxy-uninstall-debug
	$(CHMODX) $(DESTDIR)/bin/winestreamproxy-uninstall-debug

uninstall: uninstall-release uninstall-debug
uninstall-release:
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.so
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib.dll.dbg.o
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy.exe.dbg.o
	$(RM) $(DESTDIR)/lib/winestreamproxy/settings.conf
	$(RM) $(DESTDIR)/lib/winestreamproxy/common.sh
	-$(RMDIR) $(DESTDIR)/lib/winestreamproxy 2>/dev/null || :
	$(RM) $(DESTDIR)/bin/winestreamproxy
	$(RM) $(DESTDIR)/bin/winestreamproxy-stop
	$(RM) $(DESTDIR)/bin/winestreamproxy-wrapper
	$(RM) $(DESTDIR)/bin/winestreamproxy-install
	$(RM) $(DESTDIR)/bin/winestreamproxy-uninstall
uninstall-debug:
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy_unixlib-debug.dll.so
	$(RM) $(DESTDIR)/lib/winestreamproxy/winestreamproxy-debug.exe
	$(RM) $(DESTDIR)/lib/winestreamproxy/settings.conf
	$(RM) $(DESTDIR)/lib/winestreamproxy/common-debug.sh
	-$(RMDIR) $(DESTDIR)/lib/winestreamproxy 2>/dev/null || :
	$(RM) $(DESTDIR)/bin/winestreamproxy-debug
	$(RM) $(DESTDIR)/bin/winestreamproxy-stop-debug
	$(RM) $(DESTDIR)/bin/winestreamproxy-wrapper-debug
	$(RM) $(DESTDIR)/bin/winestreamproxy-install-debug
	$(RM) $(DESTDIR)/bin/winestreamproxy-uninstall-debug

clean:
	$(RM) $(OBJ)/.version
	$(RM) $(OBJ)/version.h
	$(RM) $(OBJ)/version.res
	$(RM) $(OBJ)/winestreamproxy_unixlib_unity_source.c
	$(RM) $(OBJ)/winestreamproxy_unity_source.c
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
	$(RM) $(OUT)/common.sh
	$(RM) $(OUT)/start.sh
	$(RM) $(OUT)/stop.sh
	$(RM) $(OUT)/wrapper.sh
	$(RM) $(OUT)/install.sh
	$(RM) $(OUT)/uninstall.sh
	$(RM) $(OUT)/common-debug.sh
	$(RM) $(OUT)/start-debug.sh
	$(RM) $(OUT)/stop-debug.sh
	$(RM) $(OUT)/wrapper-debug.sh
	$(RM) $(OUT)/install-debug.sh
	$(RM) $(OUT)/uninstall-debug.sh
	$(RM) $(OUT)/release.tar.gz
	$(RM) $(OUT)/debug.tar.gz
	-$(RMDIR) $(OBJ) 2>/dev/null || :
	-$(RMDIR) $(OUT) 2>/dev/null || :

.PHONY: all release debug release-tarball debug-tarball install install-release install-debug uninstall \
        uninstall-release uninstall-debug clean
.ONESHELL:
