#!/bin/sh -x
set -e
mkdir -p native android-ndk-r7
cd native
../../configure \
	--disable-shared \
	--without-clients \
	--without-servers \
	--with-tools=qfcc,pak
cd ../android-ndk-r7
export ANDROID_NDK_ROOT=/opt/android-ndk-r7
export ANDROID_SYSROOT=$ANDROID_NDK_ROOT/android-14-toolchain/sysroot
export PKG_CONFIG_LIBDIR=$ANDROID_SYSROOT
export PKG_CONFIG_PATH=$ANDROID_SYSROOT/usr/local/lib/pkgconfig
export PATH=$ANDROID_NDK_ROOT/android-14-toolchain/bin:$PATH
../../configure \
	--build=x86_64-unknown-linux-gnu \
	--with-sysroot=$ANDROID_SYSROOT \
	--host=arm-linux-androideabi \
	--with-endian=little \
	--disable-sdl \
	$*

