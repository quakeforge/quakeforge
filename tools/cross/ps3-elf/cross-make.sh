#!/bin/sh -x
set -e
if test -d native; then
	cd native
	make $*
	cd ../powerpc64-ps3-elf
	ln -fs ../native/tools/qfcc/source/qfcc .
	ln -fs ../native/tools/pak/pak .
fi
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
export PKG_CONFIG_LIBDIR=$PS3DEV/portlibs/ppu/lib/pkgconfig
export PATH=$PS3DEV/spu/bin:$PS3DEV/ppu/bin:$PS3DEV/bin:$PATH

make PAK='$(top_builddir)/pak' QFCC='$(top_builddir)/qfcc' $*
