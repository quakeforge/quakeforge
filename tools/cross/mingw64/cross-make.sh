#!/bin/sh -x
set -e
if test -d native; then
	cd native
	make $*
	cd ../x86_64-w64-mingw32
	ln -fs ../native/tools/qfcc/source/qfcc .
	ln -fs ../native/tools/pak/pak .
fi
export MINGW=/opt/mxe
export MINGW_USR=$MINGW/usr/x86_64-w64-mingw32
export PKG_CONFIG_LIBDIR=$MINGW_USR/lib/pkgconfig
export PKG_CONFIG_PATH=$MINGW_USR/local/lib/pkgconfig
export PATH=$MINGW/usr/bin:$PATH

make PAK='$(top_builddir)/pak' QFCC='$(top_builddir)/qfcc' $*
