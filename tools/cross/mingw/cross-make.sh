#!/bin/sh -x
set -e
if test -d native; then
	cd native
	make $*
	cd ../i686-w64-mingw32.static
	ln -fs ../native/qfcc .
	ln -fs ../native/pak .
	ln -fs ../native/ruamoko/qwaq/qwaq-cmd .
fi
export MINGW=/opt/mxe
export MINGW_USR=$MINGW/usr/i686-w64-mingw32.static
export PKG_CONFIG_LIBDIR=$MINGW_USR/lib/pkgconfig
export PKG_CONFIG_PATH=$MINGW_USR/local/lib/pkgconfig
export PATH=$MINGW/usr/bin:$PATH

make PAK='$(top_builddir)/pak' QFCC='$(top_builddir)/qfcc' QWAQ='$(top_builddir)/qwaq-cmd' $*
