INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES = libQFjs.la

libQFjs_la_LDFLAGS = -version-info 1:0:0 $(CD_LIBS)

if JOYTYPE_LINUX
libQFjs_la_SOURCES=      joy_linux.c
endif
if JOYTYPE_WIN32
libQFjs_la_SOURCES=      joy_win.c
endif
if JOYTYPE_NULL
libQFjs_la_SOURCES=      joy_null.c
endif
libQFjs_la_CFLAGS=       $(JOY_CFLAGS)

LIBLIST = libQFjs.la @LIBRARY_SEARCH_PATH@
