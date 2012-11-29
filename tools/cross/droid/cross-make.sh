#!/bin/sh -x
set -e
if test -d native; then
	cd native
	make $*
	cd ../android-ndk-r7
	ln -fs ../native/tools/qfcc/source/qfcc .
	ln -fs ../native/tools/pak/pak .
fi
export ANDROID_NDK_ROOT=/opt/android-ndk-r7
export ANDROID_SYSROOT=$ANDROID_NDK_ROOT/android-14-toolchain/sysroot
export PKG_CONFIG_LIBDIR=$ANDROID_SYSROOT/lib/pkgconfig
export PKG_CONFIG_PATH=$ANDROID_SYSROOT/usr/local/lib/pkgconfig
export PATH=$ANDROID_NDK_ROOT/android-14-toolchain/bin:$PATH

make PAK='$(top_builddir)/pak' QFCC='$(top_builddir)/qfcc' $*
