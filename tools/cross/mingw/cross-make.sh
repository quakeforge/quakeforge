#!/bin/sh -x
set -e
if test -d native; then
	cd native
	make $*
	cd ../i686-pc-mingw32
	ln -fs ../native/tools/qfcc/source/qfcc .
	ln -fs ../native/tools/pak/pak .
fi
export MINGW=/home/bill/src/mingw/mingw-cross-env-2.18
export MINGW_USR=$MINGW/usr/i686-pc-mingw32
export PKG_CONFIG_LIBDIR=$MINGW_USR/lib/pkgconfig
export PKG_CONFIG_PATH=$MINGW_USR/local/lib/pkgconfig
export PATH=$MINGW/usr/bin:$PATH

make PAK='$(top_builddir)/pak' QFCC='$(top_builddir)/qfcc' $*
