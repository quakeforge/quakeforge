AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include
SDL_LIBS = @SDL_LIBS@
plugin_ldflags= @plugin_ldflags@ -avoid-version -module -rpath $(plugindir)
plugin_libadd= @plugin_libadd@
EXEEXT=
snd_deps=$(top_builddir)/libs/util/libQFutil.la

plugin_LTLIBRARIES=	@snd_output_plugins@
noinst_LTLIBRARIES=	@snd_output_static_plugins@
EXTRA_LTLIBRARIES= \
	snd_output_sdl.la snd_output_alsa.la snd_output_oss.la snd_output_sgi.la \
	snd_output_sun.la snd_output_win.la snd_output_dx.la snd_output_disk.la

snd_output_sdl_la_LDFLAGS=		$(plugin_ldflags)
snd_output_sdl_la_LIBADD=		$(snd_deps) $(SDL_LIBS) $(plugin_libadd)
snd_output_sdl_la_DEPENDENCIES=	$(snd_deps)
snd_output_sdl_la_CFLAGS=		$(SDL_CFLAGS)
snd_output_sdl_la_SOURCES=		snd_sdl.c

snd_output_alsa_la_LDFLAGS=		$(plugin_ldflags)
snd_output_alsa_la_LIBADD=		$(snd_deps)
snd_output_alsa_la_DEPENDENCIES=$(snd_deps)
snd_output_alsa_la_CFLAGS=		$(ALSA_CFLAGS)
snd_output_alsa_la_SOURCES=		snd_alsa.c

snd_output_oss_la_LDFLAGS=		$(plugin_ldflags)
snd_output_oss_la_LIBADD=		$(snd_deps) $(OSS_LIBS)
snd_output_oss_la_DEPENDENCIES=	$(snd_deps)
snd_output_oss_la_CFLAGS=		$(OSS_CFLAGS)
snd_output_oss_la_SOURCES=		snd_oss.c

snd_output_sgi_la_LDFLAGS=		$(plugin_ldflags)
snd_output_sgi_la_LIBADD=		$(snd_deps) $(SGISND_LIBS)
snd_output_sgi_la_DEPENDENCIES=	$(snd_deps)
snd_output_sgi_la_CFLAGS=		$(SGISND_CFLAGS)
snd_output_sgi_la_SOURCES=		snd_sgi.c

snd_output_sun_la_LDFLAGS=		$(plugin_ldflags)
snd_output_sun_la_DEPENDENCIES=	$(snd_deps)
snd_output_sun_la_CFLAGS=		$(SUNSND_CFLAGS)
snd_output_sun_la_SOURCES=		snd_sun.c

snd_output_win_la_LDFLAGS=		$(plugin_ldflags)
snd_output_win_la_LIBADD=		$(snd_deps) $(WINSND_LIBS) $(plugin_libadd)
snd_output_win_la_DEPENDENCIES=	$(snd_deps)
snd_output_win_la_CFLAGS=		$(WIN32SND_CFLAGS)
snd_output_win_la_SOURCES=		snd_win.c

snd_output_dx_la_LDFLAGS=		$(plugin_ldflags)
snd_output_dx_la_LIBADD=		$(snd_deps) $(WINSND_LIBS) $(plugin_libadd)
snd_output_dx_la_DEPENDENCIES=	$(snd_deps)
snd_output_dx_la_CFLAGS=		$(WIN32SND_CFLAGS)
snd_output_dx_la_SOURCES=		snd_dx.c

snd_output_disk_la_LDFLAGS=		$(plugin_ldflags)
snd_output_disk_la_LIBADD=		$(snd_deps) $(plugin_libadd)
snd_output_disk_la_DEPENDENCIES=$(snd_deps)
snd_output_disk_la_CFLAGS=
snd_output_disk_la_SOURCES=		snd_disk.c
