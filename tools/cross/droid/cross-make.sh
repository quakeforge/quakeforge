#!/bin/sh -x
HOST_CC=gcc
export HOST_CC
export ANDROID_NDK_ROOT=/home/bill/Downloads/android-ndk-r7
export ANDROID_SYSROOT=$ANDROID_NDK_ROOT/android-14-toolchain/sysroot
export PATH=$ANDROID_NDK_ROOT/android-14-toolchain/bin:$PATH

make PAK=pak QFCC=qfcc $*

