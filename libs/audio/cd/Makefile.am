INCLUDES= -I$(top_srcdir)/include $(SDL_CFLAGS) $(XMMS_CFLAGS)
SDL_LIBS= @SDL_LIBS@
XMMS_LIBS= @XMMS_LIBS@
PLUGIN_RPATH= @PLUGIN_RPATH@

plugin_LTLIBRARIES=	@CD_PLUGIN_TARGETS@
noinst_LTLIBRARIES=	@CD_PLUGIN_STATIC@
EXTRA_LTLIBRARIES=	libcd_linux.la libcd_sdl.la libcd_sgi.la libcd_win.la libcd_null.la libcd_xmms.la

libcd_linux_la_LDFLAGS=	-version-info 1:0:0 $(PLUGIN_RPATH)
libcd_linux_la_SOURCES=	cd_linux.c

libcd_xmms_la_LDFLAGS=	-version-info 1:0:0 $(PLUGIN_RPATH)
libcd_xmms_la_LIBADD=	$(XMMS_LIBS)
libcd_xmms_la_SOURCES=	cd_xmms.c

libcd_sdl_la_LDFLAGS=	$(plugin_LDFLAGS) -version-info 1:0:0 $(PLUGIN_RPATH)
libcd_sdl_la_LIBADD=	$(SDL_LIBS) $(plugin_LIBADD)
libcd_sdl_la_SOURCES=	cd_sdl.c

libcd_sgi_la_LDFLAGS=	-version-info 1:0:0 $(PLUGIN_RPATH)
libcd_sgi_la_LIBADD=	$(SGI_CD_LIBS)
libcd_sgi_la_SOURCES=	cd_sgi.c

libcd_win_la_LDFLAGS=	$(plugin_LDFLAGS) -version-info 1:0:0 $(PLUGIN_RPATH)
libcd_win_la_LIBADD=	$(plugin_LIBADD)
libcd_win_la_SOURCES=	cd_win.c

libcd_null_la_LDFLAGS=	$(plugin_LDFLAGS) -version-info 1:0:0 $(PLUGIN_RPATH)
libcd_null_la_LIBADD=	$(plugin_LIBADD)
libcd_null_la_SOURCES=	cd_null.c
