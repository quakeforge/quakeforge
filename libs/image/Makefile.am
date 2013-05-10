AUTOMAKE_OPTIONS= foreign
AM_CFLAGS= @PREFER_PIC@ $(Z_CFLAGS) $(PNG_CFLAGS)
AM_CPPFLAGS= -I$(top_srcdir)/include

lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined

image_deps=$(top_builddir)/libs/util/libQFutil.la

lib_LTLIBRARIES= libQFimage.la

libQFimage_la_LDFLAGS=		$(lib_ldflags)
libQFimage_la_LIBADD=		$(image_deps) $(PNG_LIBS)
libQFimage_la_DEPENDENCIES=	$(pmage_deps)
libQFimage_la_SOURCES= \
	convert.c image.c pcx.c png.c tga.c

EXTRA_DIST=
