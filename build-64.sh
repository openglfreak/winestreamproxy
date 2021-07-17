#!/bin/sh
exec make CFLAGS='-m64' LDFLAGS='-m64' CROSSTARGET='x86_64-w64-mingw32' clean ${1:-all}
