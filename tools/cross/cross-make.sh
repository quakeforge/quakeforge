#!/bin/sh
HOST_CC=gcc
PATH=$PATH:/usr/local/cross-tools/bin:/usr/local/cross-tools/i386-mingw32msvc/bin
export HOST_CC
export PATH
make $*

