AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include
SDL_LIBS = @SDL_LIBS@
plugin_version= 1:0:0
plugin_ldflags= @plugin_ldflags@
plugin_libadd= @plugin_libadd@
EXEEXT=

plugin_PROGRAMS=	@SND_PLUGIN_TARGETS@
noinst_PROGRAMS=	@SND_PLUGIN_STATIC@
EXTRA_PROGRAMS= \
	snd_output_sdl.la snd_output_alsa.la snd_output_alsa0_9.la\
	snd_output_oss.la snd_output_sgi.la snd_output_sun.la \
	snd_output_win.la snd_output_dx.la snd_output_disk.la

snd_output_sdl_la_LDFLAGS=	$(plugin_ldflags)
snd_output_sdl_la_LDADD=	$(SDL_LIBS) $(plugin_libadd)
snd_output_sdl_la_CFLAGS=	$(SDL_CFLAGS)
snd_output_sdl_la_SOURCES=	snd_sdl.c

snd_output_alsa_la_LDFLAGS=	$(plugin_ldflags)
snd_output_alsa_la_LDADD=
snd_output_alsa_la_CFLAGS=	$(ALSA_CFLAGS)
snd_output_alsa_la_SOURCES=	snd_alsa.c

snd_output_alsa0_9_la_LDFLAGS=	$(plugin_ldflags)
snd_output_alsa0_9_la_LDADD=
snd_output_alsa0_9_la_CFLAGS=	$(ALSA_CFLAGS)
snd_output_alsa0_9_la_SOURCES=	snd_alsa_0_9.c

snd_output_oss_la_LDFLAGS=	$(plugin_ldflags)
snd_output_oss_la_LDADD=	$(OSS_LIBS)
snd_output_oss_la_CFLAGS=	$(OSS_CFLAGS)
snd_output_oss_la_SOURCES=	snd_oss.c

snd_output_sgi_la_LDFLAGS=	$(plugin_ldflags)
snd_output_sgi_la_LDADD=	$(SGISND_LIBS)
snd_output_sgi_la_CFLAGS=	$(SGISND_CFLAGS)
snd_output_sgi_la_SOURCES=	snd_sgi.c

snd_output_sun_la_LDFLAGS=	$(plugin_ldflags)
snd_output_sun_la_CFLAGS=	$(SUNSND_CFLAGS)
snd_output_sun_la_SOURCES=	snd_sun.c

snd_output_win_la_LDFLAGS=	$(plugin_ldflags)
snd_output_win_la_LDADD=	$(WINSND_LIBS) $(plugin_libadd)
snd_output_win_la_CFLAGS=	$(WIN32SND_CFLAGS)
snd_output_win_la_SOURCES=	snd_win.c

snd_output_dx_la_LDFLAGS=	$(plugin_ldflags)
snd_output_dx_la_LDADD=		$(WINSND_LIBS) $(plugin_libadd)
snd_output_dx_la_CFLAGS=	$(WIN32SND_CFLAGS)
snd_output_dx_la_SOURCES=	snd_dx.c

snd_output_disk_la_LDFLAGS=	$(plugin_ldflags)
snd_output_disk_la_LDADD=	$(plugin_libadd)
snd_output_disk_la_CFLAGS=
snd_output_disk_la_SOURCES=	snd_disk.c
