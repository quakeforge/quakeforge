INCLUDES= -I$(top_srcdir)/include $(SDL_CFLAGS)

pkglibdir = @PLUGINDIR@

pkglib_LTLIBRARIES=	@CD_PLUGIN_TARGETS@
EXTRA_LTLIBRARIES=	libcd_linux.la libcd_sdl.la libcd_sgi.la libcd_win.la libcd_null.la

libcd_linux_la_LDFLAGS= -version-info 1:0:0 -rpath $(pluginpath)
libcd_linux_la_SOURCES= cd_linux.c

libcd_sdl_la_LDFLAGS=	-version-info 1:0:0 -rpath $(pluginpath)
libcd_sdl_la_LIBADD=	$(SDL_LIBS)
libcd_sdl_la_SOURCES=	cd_sdl.c

libcd_sgi_la_LDFLAGS=	-version-info 1:0:0 -rpath $(pluginpath)
libcd_sgi_la_LIBADD=	$(SGI_CD_LIBS)
libcd_sgi_la_SOURCES=	cd_sgi.c

libcd_win_la_LDFLAGS=	-version-info 1:0:0 -rpath $(pluginpath)
libcd_win_la_SOURCES=	cd_win.c

libcd_null_la_LDFLAGS= -version-info 1:0:0 -rpath $(pluginpath)
libcd_null_la_SOURCES= cd_null.c
