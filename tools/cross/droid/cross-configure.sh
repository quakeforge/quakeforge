#!/bin/sh -x
export ANDROID_NDK_ROOT=/home/bill/Downloads/android-ndk-r7
export ANDROID_SYSROOT=$ANDROID_NDK_ROOT/android-14-toolchain/sysroot
export PATH=$ANDROID_NDK_ROOT/android-14-toolchain/bin:$PATH
../configure \
	--build=x86_64-unknown-linux-gnu \
	--with-sysroot=$ANDROID_SYSROOT \
	--host=arm-linux-androideabi --with-endian=little \
	$*

