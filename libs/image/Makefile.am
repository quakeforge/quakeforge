AUTOMAKE_OPTIONS= foreign
AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFimage.la

libQFimage_la_LDFLAGS=		-version-info 1:0:0
libQFimage_la_LIBADD=		-lpng
libQFimage_la_DEPENDENCIES=
libQFimage_la_SOURCES= \
	image.c pcx.c png.c tga.c

EXTRA_DIST=
