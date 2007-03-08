#!/bin/sh

PKG_CONFIG_PATH=/usr/local/cross-tools/i386-mingw32msvc/lib/pkgconfig PATH=/usr/i586-mingw32msvc/bin:$PATH \
	./configure \
	--host=i586-mingw32msvc \
	--target=i586-mingw32msvc \
	--build=i586-linux \
	$*

