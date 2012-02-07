AUTOMAKE_OPTIONS= foreign

SUBDIRS= targets renderer . test
AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include
SDL_LIBS= @SDL_LIBS@
XMMS_LIBS= @XMMS_LIBS@
plugin_ldflags= @plugin_ldflags@ -avoid-version -module -rpath $(plugindir)
plugin_libadd= @plugin_libadd@
lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined
EXEEXT=

plugin_LTLIBRARIES=	@CD_PLUGIN_TARGETS@
noinst_LTLIBRARIES=	@CD_PLUGIN_STATIC@
lib_LTLIBRARIES=	@CD_TARGETS@ @SND_TARGETS@
EXTRA_LTLIBRARIES=	libQFsound.la libQFcd.la \
					cd_file.la cd_linux.la cd_sdl.la cd_sgi.la cd_win.la \
					cd_xmms.la

cd_plug_libs=$(top_builddir)/libs/util/libQFutil.la

cd_file_libs= \
	libQFsound.la \
	$(cd_plug_libs)

cd_file_la_LDFLAGS=			$(plugin_ldflags)
cd_file_la_LIBADD=			$(cd_file_libs)
cd_file_la_DEPENDENCIES=	$(cd_file_libs)
cd_file_la_SOURCES=			cd_file.c

cd_linux_la_LDFLAGS=		$(plugin_ldflags)
cd_linux_la_LIBADD=			$(cd_plug_libs)
cd_linux_la_DEPENDENCIES=	$(cd_plug_libs)
cd_linux_la_SOURCES=		cd_linux.c

cd_sdl_la_LDFLAGS=			$(plugin_ldflags)
cd_sdl_la_LIBADD=			$(cd_plug_libs) $(SDL_LIBS) $(plugin_libadd)
cd_sdl_la_DEPENDENCIES=		$(cd_plug_libs)
cd_sdl_la_CFLAGS=			$(SDL_CFLAGS)
cd_sdl_la_SOURCES=			cd_sdl.c

cd_sgi_la_LDFLAGS=			$(plugin_ldflags)
cd_sgi_la_LIBADD=			$(cd_plug_libs) $(SGI_CD_LIBS)
cd_sgi_la_DEPENDENCIES=		$(cd_plug_libs)
cd_sgi_la_SOURCES=			cd_sgi.c

cd_win_la_LDFLAGS=			$(plugin_ldflags)
cd_win_la_LIBADD=			$(cd_plug_libs) $(plugin_libadd)
cd_win_la_DEPENDENCIES=		$(cd_plug_libs)
cd_win_la_SOURCES=			cd_win.c

cd_xmms_la_LDFLAGS=			$(plugin_ldflags)
cd_xmms_la_LIBADD=			$(cd_plug_libs) $(XMMS_LIBS)
cd_xmms_la_DEPENDENCIES=	$(cd_plug_libs)
cd_xmms_la_CFLAGS=			$(XMMS_CFLAGS)
cd_xmms_la_SOURCES=			cd_xmms.c

sound_libs= \
	@SND_PLUGIN_STATIC_LIBS@ \
	@SND_REND_STATIC_LIBS@ \
	$(top_builddir)/libs/gamecode/libQFgamecode.la \
	$(top_builddir)/libs/util/libQFutil.la

cd_libs= \
	@CD_PLUGIN_STATIC_LIBS@ \
	$(top_builddir)/libs/util/libQFutil.la

libQFsound_la_LDFLAGS=		$(lib_ldflags)
libQFsound_la_LIBADD=		$(sound_libs)
libQFsound_la_DEPENDENCIES=	$(sound_libs)
libQFsound_la_SOURCES=		snd.c snd_progs.c

libQFcd_la_LDFLAGS=			$(lib_ldflags)
libQFcd_la_LIBADD=			$(cd_libs)
libQFcd_la_DEPENDENCIES=	$(cd_libs)
libQFcd_la_SOURCES=			cd.c
