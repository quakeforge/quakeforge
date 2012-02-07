#!/bin/sh -x
HOST_CC=gcc
export HOST_CC
MINGW=/home/bill/src/mingw/mingw-cross-env-2.18
export PKG_CONFIG_PATH=$MINGW/usr/lib/pkgconfig
export PATH=$MINGW/usr/bin:$PATH

make PAK=pak QFCC=qfcc $*

