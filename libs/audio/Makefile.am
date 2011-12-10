AUTOMAKE_OPTIONS= foreign

SUBDIRS= targets renderer . test
AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include
SDL_LIBS= @SDL_LIBS@
XMMS_LIBS= @XMMS_LIBS@
plugin_ldflags= @plugin_ldflags@ -avoid-version -module -rpath $(plugindir)
plugin_libadd= @plugin_libadd@
EXEEXT=

plugin_LTLIBRARIES=	@CD_PLUGIN_TARGETS@
noinst_LTLIBRARIES=	@CD_PLUGIN_STATIC@
lib_LTLIBRARIES=	@CD_TARGETS@ @SND_TARGETS@
EXTRA_LTLIBRARIES=	libQFsound.la libQFcd.la \
					cd_file.la cd_linux.la cd_sdl.la cd_sgi.la cd_win.la \
					cd_xmms.la

cd_file_la_LDFLAGS= $(plugin_ldflags)
cd_file_la_LIBADD= $(top_builddir)/libs/util/libQFutil.la $(top_builddir)/libs/audio/libQFsound.la
cd_file_la_SOURCES= cd_file.c
cd_file_la_DEPENDENCIES=libQFsound.la

cd_linux_la_LDFLAGS=	$(plugin_ldflags)
cd_linux_la_LIBADD=	$(top_builddir)/libs/util/libQFutil.la
cd_linux_la_SOURCES=	cd_linux.c

cd_sdl_la_LDFLAGS=	$(plugin_ldflags)
cd_sdl_la_LIBADD=	$(SDL_LIBS) $(plugin_libadd) $(top_builddir)/libs/util/libQFutil.la
cd_sdl_la_CFLAGS=	$(SDL_CFLAGS)
cd_sdl_la_SOURCES=	cd_sdl.c

cd_sgi_la_LDFLAGS=	$(plugin_ldflags)
cd_sgi_la_LIBADD=	$(SGI_CD_LIBS)
cd_sgi_la_SOURCES=	cd_sgi.c

cd_win_la_LDFLAGS=	$(plugin_ldflags)
cd_win_la_LIBADD=	$(plugin_libadd)
cd_win_la_SOURCES=	cd_win.c

cd_xmms_la_LDFLAGS=	$(plugin_ldflags)
cd_xmms_la_LIBADD=	$(XMMS_LIBS) $(top_builddir)/libs/util/libQFutil.la
cd_xmms_la_CFLAGS=	$(XMMS_CFLAGS)
cd_xmms_la_SOURCES=	cd_xmms.c

libQFsound_la_LDFLAGS=	-version-info $(QUAKE_LIBRARY_VERSION_INFO) -rpath $(libdir)
libQFsound_la_LIBADD=	@SND_PLUGIN_STATIC_LIBS@ @SND_REND_STATIC_LIBS@ $(top_builddir)/libs/util/libQFutil.la $(top_builddir)/libs/gamecode/engine/libQFgamecode.la
libQFsound_la_SOURCES=	snd.c snd_progs.c
libQFsound_la_DEPENDENCIES=	@SND_PLUGIN_STATIC_LIBS@ @SND_REND_STATIC_LIBS@

libQFcd_la_LDFLAGS=	-version-info $(QUAKE_LIBRARY_VERSION_INFO) -rpath $(libdir)
libQFcd_la_LIBADD=		@CD_PLUGIN_STATIC_LIBS@ $(top_builddir)/libs/util/libQFutil.la
libQFcd_la_SOURCES=		cd.c
libQFcd_la_DEPENDENCIES= @CD_PLUGIN_STATIC_LIBS@
