INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES = @CD_TARGETS@
EXTRA_LTLIBRARIES = libQFcd.la

libQFcd_la_LDFLAGS = -version-info 1:0:0 $(CD_LIBS)

#
# ... CD audio
#
if CDTYPE_LINUX
libQFcd_la_SOURCES=	cd_linux.c
endif
if CDTYPE_SDL
libQFcd_la_SOURCES=	cd_sdl.c
endif
if CDTYPE_SGI
libQFcd_la_SOURCES=	cd_sgi.c
endif
if CDTYPE_WIN32
libQFcd_la_SOURCES=	cd_win.c
endif
if CDTYPE_NULL
libQFcd_la_SOURCES=	cd_null.c
endif
libQFcd.la: $(libQFcd_la_OBJECTS) $(libQFcd_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFcd_la_LDFLAGS) $(libQFcd_la_OBJECTS) $(libQFcd_la_LIBADD) $(LIBS)


LIBLIST = libQFcd.la @LIBRARY_SEARCH_PATH@
