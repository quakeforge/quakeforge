#!/bin/sh -x
set -e
mkdir -p native i686-w64-mingw32.static
cd native
../../configure \
	--disable-shared \
	--without-clients \
	--without-servers \
	--with-tools=qfcc,pak
cd ../i686-w64-mingw32.static
export MINGW=/opt/mxe
export MINGW_USR=$MINGW/usr/i686-w64-mingw32.static
export PKG_CONFIG_LIBDIR=$MINGW_USR/lib/pkgconfig
export PKG_CONFIG_PATH=$MINGW_USR/local/lib/pkgconfig
export PATH=$MINGW/usr/bin:$PATH
../../configure \
	--host=i686-w64-mingw32.static \
	--disable-shared \
	$*
