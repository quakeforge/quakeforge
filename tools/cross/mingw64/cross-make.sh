#!/bin/sh -x
set -e
if test -d native; then
	cd native
	make $*
	cd ../x86_64-w64-mingw32.static
fi
ln -fs ../native/qfcc .
ln -fs ../native/pak .
ln -fs ../native/ruamoko/qwaq/qwaq-cmd .
export MINGW=/opt/mxe
export MINGW_USR=$MINGW/usr/x86_64-w64-mingw32.static
export PKG_CONFIG_LIBDIR=$MINGW_USR/lib/pkgconfig
export PKG_CONFIG_PATH=$MINGW_USR/local/lib/pkgconfig
export PATH=$MINGW/usr/bin:$PATH
export VULKAN_SDK=/opt/vulkan/current/x86_64

make PAK='$(top_builddir)/pak' QFCC='$(top_builddir)/qfcc' QWAQ='$(top_builddir)/qwaq-cmd' VKGENUSRINC=$MINGW_USR/include $*
