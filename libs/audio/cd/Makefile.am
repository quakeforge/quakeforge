AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include
SDL_LIBS= @SDL_LIBS@
XMMS_LIBS= @XMMS_LIBS@
plugin_version= 1:0:0
plugin_ldflags= @plugin_ldflags@
plugin_libadd= @plugin_libadd@
EXEEXT=

plugin_PROGRAMS=	@CD_PLUGIN_TARGETS@
noinst_PROGRAMS=	@CD_PLUGIN_STATIC@
EXTRA_PROGRAMS=	cd_file.la cd_linux.la cd_sdl.la cd_sgi.la cd_win.la cd_xmms.la

cd_file_la_LDFLAGS= $(plugin_ldflags)
cd_file_la_SOURCES= cd_file.c

cd_linux_la_LDFLAGS=	$(plugin_ldflags)
cd_linux_la_SOURCES=	cd_linux.c

cd_sdl_la_LDFLAGS=	$(plugin_ldflags)
cd_sdl_la_LDADD=	$(SDL_LIBS) $(plugin_libadd)
cd_sdl_la_CFLAGS=	$(SDL_CFLAGS)
cd_sdl_la_SOURCES=	cd_sdl.c

cd_sgi_la_LDFLAGS=	$(plugin_ldflags)
cd_sgi_la_LDADD=	$(SGI_CD_LIBS)
cd_sgi_la_SOURCES=	cd_sgi.c

cd_win_la_LDFLAGS=	$(plugin_ldflags)
cd_win_la_LDADD=	$(plugin_libadd)
cd_win_la_SOURCES=	cd_win.c

cd_xmms_la_LDFLAGS=	$(plugin_ldflags)
cd_xmms_la_LDADD=	$(XMMS_LIBS)
cd_xmms_la_CFLAGS=	$(XMMS_CFLAGS)
cd_xmms_la_SOURCES=	cd_xmms.c
