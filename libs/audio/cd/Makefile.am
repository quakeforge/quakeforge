AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include $(SDL_CFLAGS) $(XMMS_CFLAGS)
SDL_LIBS= @SDL_LIBS@
XMMS_LIBS= @XMMS_LIBS@
plugin_version= 1:0:0
plugin_ldflags= @plugin_ldflags@
plugin_libadd= @plugin_libadd@

plugin_LTLIBRARIES=	@CD_PLUGIN_TARGETS@
noinst_LTLIBRARIES=	@CD_PLUGIN_STATIC@
EXTRA_LTLIBRARIES=	libcd_linux.la libcd_sdl.la libcd_sgi.la libcd_win.la libcd_null.la libcd_xmms.la

libcd_linux_la_LDFLAGS=	$(plugin_ldflags)
libcd_linux_la_SOURCES=	cd_linux.c

libcd_xmms_la_LDFLAGS=	$(plugin_ldflags)
libcd_xmms_la_LIBADD=	$(XMMS_LIBS)
libcd_xmms_la_SOURCES=	cd_xmms.c

libcd_sdl_la_LDFLAGS=	$(plugin_ldflags)
libcd_sdl_la_LIBADD=	$(SDL_LIBS) $(plugin_libadd)
libcd_sdl_la_SOURCES=	cd_sdl.c

libcd_sgi_la_LDFLAGS=	$(plugin_ldflags)
libcd_sgi_la_LIBADD=	$(SGI_CD_LIBS)
libcd_sgi_la_SOURCES=	cd_sgi.c

libcd_win_la_LDFLAGS=	$(plugin_ldflags)
libcd_win_la_LIBADD=	$(plugin_libadd)
libcd_win_la_SOURCES=	cd_win.c

libcd_null_la_LDFLAGS=	$(plugin_ldflags)
libcd_null_la_LIBADD=	$(plugin_libadd)
libcd_null_la_SOURCES=	cd_null.c
