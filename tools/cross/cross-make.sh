#!/bin/sh -x
HOST_CC=gcc
PATH=/usr/i586-mingw32msvc/bin:$PATH
export HOST_CC
export PATH
make $*

