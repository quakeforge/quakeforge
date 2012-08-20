#!/bin/sh -x
MINGW=/home/bill/src/mingw/mingw-cross-env-2.18
export PKG_CONFIG_LIBDIR=$MINGW/usr/lib/pkgconfig
export PATH=$MINGW/usr/bin:$PATH

make PAK=pak QFCC=qfcc $*
