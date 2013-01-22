#!/bin/sh -x
set -e
mkdir -p native x86_64-w64-mingw32
cd native
../../configure \
	--disable-shared \
	--without-clients \
	--without-servers \
	--with-tools=qfcc,pak
cd ../x86_64-w64-mingw32
export MINGW=/opt/mxe
export MINGW_USR=$MINGW/usr/x86_64-w64-mingw32
export PKG_CONFIG_LIBDIR=$MINGW_USR/lib/pkgconfig
export PKG_CONFIG_PATH=$MINGW_USR/local/lib/pkgconfig
export PATH=$MINGW/usr/bin:$PATH
../../configure \
	--host=x86_64-w64-mingw32 \
	--disable-shared \
	$*
