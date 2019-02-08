#!/bin/sh

LIB_CFLAGS="-O1 -g -flto"
XLIBS="-lX11 -lXext"
CFLAGS="-Os -g -flto"
CC=gcc
AR=gcc-ar
RANLIB=gcc-ranlib

"${CC}" ${LIB_CFLAGS} -g -c -o bin/build/s4g-x11.o src/platforms/x11/s4g-x11.c -Iinclude && \
"${AR}" cr bin/libs4g-x11.a bin/build/s4g-x11.o && \
"${RANLIB}" bin/libs4g-x11.a && \
"${CC}" ${CFLAGS} -o bin/snap src/examples/snap.c -Iinclude -Lbin -ls4g-x11 ${XLIBS} && \
true

