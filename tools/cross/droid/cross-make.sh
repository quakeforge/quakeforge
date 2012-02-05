#!/bin/sh -x
HOST_CC=gcc
export HOST_CC
ANDROID_NDK_ROOT=/home/bill/Downloads/android-ndk-r7
export PATH=$ANDROID_NDK_ROOT/android-14-toolchain/bin:$PATH

make PAK=pak QFCC=qfcc $*

