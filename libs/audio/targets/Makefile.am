INCLUDES= -I$(top_srcdir)/include $(ALSA_CFLAGS) $(MME_CFLAGS) $(OSS_CFLAGS) $(SGISND_CFLAGS) $(SUNSND_CFLAGS) $(WIN32SND_CFLAGS) $(SDL_CFLAGS)
SDL_LIBS = @SDL_LIBS@

plugin_LTLIBRARIES = @SND_PLUGIN_TARGETS@
noinst_LTLIBRARIES = @SND_PLUGIN_STATIC@
EXTRA_LTLIBRARIES = libsnd_output_sdl.la libsnd_output_alsa0_5.la libsnd_output_alsa0_9.la libsnd_output_oss.la libsnd_output_sgi.la libsnd_output_sun.la libsnd_output_win.la libsnd_output_null.la libsnd_output_disk.la

libsnd_output_sdl_la_LDFLAGS=	$(plugin_LDFLAGS) -version-info 1:0:0 -rpath $(plugindir)
libsnd_output_sdl_la_LIBADD=	$(SDL_LIBS) $(plugin_LIBADD)
libsnd_output_sdl_la_SOURCES=	snd_sdl.c

libsnd_output_alsa0_5_la_LDFLAGS=	-version-info 1:0:0 -rpath $(plugindir)
libsnd_output_alsa0_5_la_LIBADD=	$(ALSA_LIBS)
libsnd_output_alsa0_5_la_SOURCES=	snd_alsa_0_5.c

libsnd_output_alsa0_9_la_LDFLAGS=	-version-info 1:0:0 -rpath $(plugindir)
libsnd_output_alsa0_9_la_LIBADD=	$(ALSA_LIBS)
libsnd_output_alsa0_9_la_SOURCES=	snd_alsa_0_9.c

libsnd_output_oss_la_LDFLAGS=	-version-info 1:0:0 -rpath $(plugindir)
libsnd_output_oss_la_LIBADD=	$(OSS_LIBS)
libsnd_output_oss_la_SOURCES=	snd_oss.c

libsnd_output_sgi_la_LDFLAGS=	-version-info 1:0:0 -rpath $(plugindir)
libsnd_output_sgi_la_LIBADD=	$(SGISND_LIBS)
libsnd_output_sgi_la_SOURCES=	snd_sgi.c

libsnd_output_sun_la_LDFLAGS=	-version-info 1:0:0 -rpath $(plugindir)
libsnd_output_sun_la_SOURCES=	snd_sun.c

libsnd_output_win_la_LDFLAGS=	$(plugin_LDFLAGS) -version-info 1:0:0 -rpath $(plugindir)
libsnd_output_win_la_LIBADD=	$(WINSND_LIBS) $(plugin_LIBADD)
libsnd_output_win_la_SOURCES=	snd_win.c

libsnd_output_disk_la_LDFLAGS=	$(plugin_LDFLAGS) -verison-info 1:0:0 -rpath $(plugindir)
libsnd_output_disk_la_LIBADD=	$(plugin_LIBADD)
libsnd_output_disk_la_SOURCES=	snd_disk.c

libsnd_output_null_la_LDFLAGS=	$(plugin_LDFLAGS) -version-info 1:0:0 -rpath $(plugindir)
libsnd_output_null_la_LIBADD=	$(plugin_LIBADD)
libsnd_output_null_la_SOURCES=	snd_null.c
