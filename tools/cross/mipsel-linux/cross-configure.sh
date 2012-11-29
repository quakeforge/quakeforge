#!/bin/sh -x
set -e
mkdir -p native mipsel-linux
cd native
../../configure \
	--disable-shared \
	--without-clients \
	--without-servers \
	--with-tools=qfcc,pak
cd ../mipsel-linux
export MIPSEL=/opt/gcw0-toolchain
export MIPSEL_SYSROOT=$MIPSEL/usr/mipsel-gcw0-linux-uclibc/sysroot
export PKG_CONFIG_LIBDIR=$MIPSEL_SYSROOT/lib/pkgconfig
export PKG_CONFIG_PATH=$MIPSEL_SYSROOT/usr/local/lib/pkgconfig
export PATH=$MIPSEL/usr/bin:$PATH
../../configure --host=mipsel-linux \
	--with-sysroot=$MIPSEL_SYSROOT \
	--disable-sdl-cd \
	--with-endian=little \
	SDL_CONFIG=$MIPSEL_SYSROOT/usr/bin/sdl-config \
	$*
