INCLUDES= -I$(top_srcdir)/include $(SDL_CFLAGS)

pkglib_LTLIBRARIES=	@CD_PLUGIN_TARGETS@ libcd_null.la
EXTRA_LTLIBRARIES=	libcd_linux.la libcd_sdl.la libcd_sgi.la libcd_win.la


libcd_linux_la_LDFLAGS=	-version-info 1:0:0
libcd_linux_la_SOURCES=	cd_linux.c
libcd_linux.la:	$(libcd_linux_la_OBJECTS) $(libcd_linux_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libcd_linux_la_LDFLAGS) $(libcd_linux_la_OBJECTS) $(libcd_linux_la_LIBADD) $(LIBS)

libcd_sdl_la_LDFLAGS= -version-info 1:0:0 $(SDL_LIBS)
libcd_sdl_la_SOURCES= cd_sdl.c
libcd_sdl.la: $(libcd_sdl_la_OBJECTS) $(libcd_sdl_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libcd_sdl_la_LDFLAGS) $(libcd_sdl_la_OBJECTS) $(libcd_sdl_la_LIBADD) $(LIBS)

libcd_sgi_la_LDFLAGS= -version-info 1:0:0 $(SGI_CD_LIBS)
libcd_sgi_la_SOURCES= cd_sgi.c
libcd_sgi.la: $(libcd_sgi_la_OBJECTS) $(libcd_sgi_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libcd_sgi_la_LDFLAGS) $(libcd_sgi_la_OBJECTS) $(libcd_sgi_la_LIBADD) $(LIBS)

libcd_win_la_LDFLAGS= -version-info 1:0:0
libcd_win_la_SOURCES= cd_win.c
libcd_win.la: $(libcd_win_la_OBJECTS) $(libcd_win_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libcd_win_la_LDFLAGS) $(libcd_win_la_OBJECTS) $(libcd_win_la_LIBADD) $(LIBS)

libcd_null_la_LDFLAGS= -version-info 1:0:0
libcd_null_la_SOURCES= cd_null.c
libcd_null.la: $(libcd_null_la_OBJECTS) $(libcd_null_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libcd_null_la_LDFLAGS) $(libcd_null_la_OBJECTS) $(libcd_null_la_LIBADD) $(LIBS)

LIBLIST = $(pkglib_LTLIBRARIES) @LIBRARY_SEARCH_PATH@
