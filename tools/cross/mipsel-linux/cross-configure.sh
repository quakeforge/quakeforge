#!/bin/sh -x
mkdir native mipsel-linux
cd native
../../configure --without-clients --without-servers --with-tools=qfcc,pak
cd ../mipsel-linux
export MIPSEL=/opt/gcw0-toolchain
export PKG_CONFIG_LIBDIR=$MIPSEL/usr/mipsel-gcw0-linux-uclibc/sysroot/lib/pkgconfig
export PATH=$MIPSEL/usr/bin:$PATH
../../configure --host=mipsel-linux \
	--disable-sdl-cd \
	--with-endian=little \
	SDL_CONFIG=$MIPSEL/usr/mipsel-gcw0-linux-uclibc/sysroot/usr/bin/sdl-config \
	$*
