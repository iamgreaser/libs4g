#!/bin/sh

LIB_CFLAGS="-O1 -g -flto"
W32LIBS="-lgdi32"
CFLAGS="-Os -g -flto"
CROSSPREFIX=i686-w64-mingw32-
CC=${CROSSPREFIX}gcc
AR=${CROSSPREFIX}gcc-ar
RANLIB=${CROSSPREFIX}gcc-ranlib

"${CC}" ${LIB_CFLAGS} -g -c -o winbin/build/s4g-w32.o src/platforms/w32/s4g-w32.c -Iinclude && \
"${AR}" cr winbin/libs4g-w32.a winbin/build/s4g-w32.o && \
"${RANLIB}" winbin/libs4g-w32.a && \
"${CC}" ${CFLAGS} -o winbin/snap.exe src/examples/snap.c -Iinclude -Lwinbin -ls4g-w32 ${W32LIBS} && \
true

