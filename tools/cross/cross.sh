#!/bin/sh

# This is my script for building a complete cross-compiler
# toolchain. It is based partly on Mo Dejong's script for doing
# the same, but with some added fixes.
#
# Written by Ray Kelm <rhk@newimage.com>

# what flavor are we building?

TARGET=i386-mingw32msvc

# where does it go?

PREFIX=/usr/local/cross-tools

# you probably don't need to change anything from here down

TOPDIR=`pwd`
SRCDIR=$TOPDIR/source

# these files are available from Mumit Khan's archive

BINUTILS=binutils-19990818
BINUTILS_TAR=$BINUTILS-1-src.tar.gz
GCC=gcc-2.95.2
GCC_TAR=$GCC-1-src.tar.gz
MSVCRT_ZIP=bin-msvcrt-2000-03-27.zip
MUMIT_URL=ftp://ftp.xraylith.wisc.edu/pub/khan/gnu-win32/mingw32/snapshots/gcc-2.95.2-1
MUMIT_URL2=ftp://ftp.xraylith.wisc.edu/pub/khan/gnu-win32/mingw32/runtime

# taniwha's binutils patch
BINUTILS_PATCH=binutils-19990818-bison-patch
TAN_URL=http://quakeforge.net/~taniwha/

# this is from my archive

GCC_PATCH=gcc-2.95.2-libio-patch
RHK_URL=http://www.newimage.com/~rhk/crossgcc/
OPENGL_TAR=opengl.tar.gz
DIRECTX_TAR=directx.tar.gz

# need install directory first on the path so gcc can find binutils

PATH="$PREFIX/bin:$PATH"

#
# download a file from a given url, only if it is not present
#

download_file()
{
	cd $SRCDIR
	if test ! -f $1 ; then
		echo "Downloading $1"
		wget $2/$1
		if test ! -f $1 ; then
			echo "Could not download $1"
			exit 1
		fi
	else
		echo "Found $1 in the srcdir $SRCDIR"
	fi
  	cd $TOPDIR
}

download_files()
{
	mkdir -p $SRCDIR
	
	# Make sure wget is installed
	if test "x`which wget`" = "x" ; then
		echo "You need to install wget."
		exit 1
	fi
	download_file $BINUTILS_TAR $MUMIT_URL
	download_file $BINUTILS_PATCH $TAN_URL
	download_file $GCC_TAR $MUMIT_URL
	download_file $MSVCRT_ZIP $MUMIT_URL2
	download_file $GCC_PATCH $RHK_URL
	download_file $OPENGL_TAR $RHK_URL
	download_file $DIRECTX_TAR $RHK_URL
}

install_libs()
{
	echo "Installing binary cross libs and includes"
	mkdir -p $PREFIX/$TARGET
	cd $PREFIX
	unzip -q -o $SRCDIR/$MSVCRT_ZIP
	cd $PREFIX/$TARGET
	gzip -dc $SRCDIR/opengl.tar.gz | tar xf -
	gzip -dc $SRCDIR/directx.tar.gz | tar xf -
	cd $TOPDIR
}

extract_binutils()
{
	cd $SRCDIR
	rm -rf $BINUTILS
	echo "Extracting binutils"
	gzip -dc $SRCDIR/$BINUTILS_TAR | tar xf -
	cd $TOPDIR
}

patch_binutils()
{
	echo "Patching binutils"
	cd $SRCDIR/$BINUTILS
	patch -p1 < $SRCDIR/$BINUTILS_PATCH
	cd $TOPDIR
}

configure_binutils()
{
	cd $TOPDIR
	rm -rf binutils-$TARGET
	mkdir binutils-$TARGET
	cd binutils-$TARGET
	echo "Configuring binutils"
	$SRCDIR/$BINUTILS/configure --prefix=$PREFIX --target=$TARGET &> configure.log
	cd $TOPDIR
}

build_binutils()
{
	cd $TOPDIR/binutils-$TARGET
	echo "Building binutils"
	make &> make.log
	if test $? -ne 0; then
		echo "make failed"
		exit 1
	fi
	cd $TOPDIR
}

install_binutils()
{
	cd $TOPDIR/binutils-$TARGET
	echo "Installing binutils"
	make install &> make-install.log
	if test $? -ne 0; then
		echo "install failed"
		exit 1
	fi
	cd $TOPDIR
}

extract_gcc()
{
	cd $SRCDIR
	rm -rf $GCC
	echo "Extracting gcc"
	gzip -dc $SRCDIR/$GCC_TAR | tar xf -
	cd $TOPDIR
}

patch_gcc()
{
	echo "Patching gcc"
	cd $SRCDIR/$GCC
	patch -p1 < $SRCDIR/$GCC_PATCH
	cd $TOPDIR
}

configure_gcc()
{
	cd $TOPDIR
	rm -rf gcc-$TARGET
	mkdir gcc-$TARGET
	cd gcc-$TARGET
	echo "Configuring gcc"
	$SRCDIR/$GCC/configure -v --prefix=$PREFIX --target=$TARGET \
		--with-gnu-as --with-gnu-ld --disable-multilib &> configure.log
	cd $TOPDIR
}

build_gcc()
{
	cd $TOPDIR/gcc-$TARGET
	echo "Building gcc"
	make LANGUAGES="c c++" &> make.log
	if test $? -ne 0; then
		echo "make failed"
		exit 1
	fi
	cd $TOPDIR
}

install_gcc()
{
	cd $TOPDIR/gcc-$TARGET
	echo "Installing gcc"
	make LANGUAGES="c c++" install &> make-install.log
	if test $? -ne 0; then
		echo "install failed"
		exit 1
	fi
	cd $TOPDIR
}

download_files

install_libs

extract_binutils
patch_binutils
configure_binutils
build_binutils
install_binutils

extract_gcc
patch_gcc
configure_gcc
build_gcc
install_gcc

