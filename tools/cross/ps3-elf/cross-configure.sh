#!/bin/sh -x
mkdir native powerpc64-ps3-elf
cd native
../../configure --without-clients --without-servers --with-tools=qfcc,pak
cd ../powerpc64-ps3-elf
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
export PKG_CONFIG_LIBDIR=$PS3DEV/portlibs/ppu/lib/pkgconfig
export PATH=$PS3DEV/spu/bin:$PS3DEV/ppu/bin:$PS3DEV/bin:$PATH
../../configure --host=powerpc64-ps3-elf \
	--disable-sdl-cd \
	--with-endian=big \
	SDL_CONFIG=$PS3DEV/portlibs/ppu/bin/sdl-config \
	$*
