#!/bin/sh -x
HOST_CC=gcc
export HOST_CC
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
export PKG_CONFIG_LIBDIR=$PS3DEV/portlibs/ppu/lib/pkgconfig
export PATH=$PS3DEV/spu/bin:$PS3DEV/ppu/bin:$PS3DEV/bin:$PATH

make PAK=pak QFCC=qfcc $*
