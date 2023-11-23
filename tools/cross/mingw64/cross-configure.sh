#!/bin/sh -x
set -e
mkdir -p native x86_64-w64-mingw32.static
cd native
../../configure \
	--enable-silent-rules \
	--disable-shared \
	--disable-optimize \
	--without-clients \
	--without-servers \
	--with-tools=qfcc,pak,qwaq
cd ../x86_64-w64-mingw32.static
export MINGW=/opt/mxe
export MINGW_USR=$MINGW/usr/x86_64-w64-mingw32.static
export PKG_CONFIG_LIBDIR=$MINGW_USR/lib/pkgconfig
export PKG_CONFIG_PATH=$MINGW_USR/local/lib/pkgconfig
export PATH=$MINGW/usr/bin:$PATH
export QCSYSPREFIX=$MINGW_USR
../../configure \
	--enable-silent-rules \
	--host=x86_64-w64-mingw32.static \
	--disable-shared \
	$*
