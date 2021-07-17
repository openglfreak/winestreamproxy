#!/bin/sh
exec make CFLAGS='-m32' LDFLAGS='-m32' CROSSTARGET='i686-w64-mingw32' clean ${1:-all}
