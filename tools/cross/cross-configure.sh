#!/bin/sh
MINGW=/home/bill/src/mingw/mingw-cross-env-2.18
export PKG_CONFIG_PATH=$MINGW/usr/lib/pkgconfig
export PATH=$MINGW/usr/bin:$PATH
../configure --host=i686-pc-mingw32 \
	SDL_CONFIG=$MINGW/usr/i686-pc-mingw32/bin/sdl-config $*

