INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES = libQFcd.la

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

LIBLIST = libQFcd.la @LIBRARY_SEARCH_PATH@
