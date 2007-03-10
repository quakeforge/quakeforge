AUTOMAKE_OPTIONS= foreign
CFLAGS+= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFimage.la

libQFimage_la_LDFLAGS=		-version-info 1:0:0 -no-undefined
libQFimage_la_LIBADD=		$(PNG_LIBS) $(top_builddir)/libs/util/libQFutil.la
libQFimage_la_DEPENDENCIES=
libQFimage_la_SOURCES= \
	image.c pcx.c png.c tga.c

EXTRA_DIST=
