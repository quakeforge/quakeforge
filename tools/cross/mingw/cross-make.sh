#!/bin/sh -x
HOST_CC=gcc
export HOST_CC
MINGW=/home/bill/src/mingw/mingw-cross-env-2.18
export PATH=$MINGW/usr/bin:$PATH

make PAK=pak QFCC=qfcc $*

