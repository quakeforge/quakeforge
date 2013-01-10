#!/bin/sh -x
set -e
if test -d native; then
	cd native
	make
	cd ../mipsel-linux
	ln -fs ../native/tools/qfcc/source/qfcc .
	ln -fs ../native/tools/pak/pak .
fi
export MIPSEL=/opt/gcw0-toolchain
export MIPSEL_SYSROOT=$MIPSEL/usr/mipsel-gcw0-linux-uclibc/sysroot
export PKG_CONFIG_LIBDIR=$MIPSEL_SYSROOT/usr/lib/pkgconfig
export PKG_CONFIG_PATH=$MIPSEL_SYSROOT/usr/local/lib/pkgconfig
export PATH=$MIPSEL/usr/bin:$PATH

make PAK='$(top_builddir)/pak' QFCC='$(top_builddir)/qfcc' $*
