INCLUDES= -I$(top_srcdir)/include $(ALSA_CFLAGS) $(MME_CFLAGS) $(OSS_CFLAGS) $(SGISND_CFLAGS) $(SUNSND_CFLAGS) $(WIN32SND_CFLAGS) $(SDL_CFLAGS)

pkglib_LTLIBRARIES = @SND_PLUGIN_TARGETS@ libsound_null.la libsound_disk.la
EXTRA_LTLIBRARIES = libsound_sdl.la libsound_alsa0_5.la libsound_alsa0_9.la libsound_mme.la libsound_oss.la libsound_sgi.la libsound_sun.la libsound_win.la

common_sound_files= snd_dma.c snd_mem.c snd_mix.c snd_mixa.S

libsound_sdl_la_LDFLAGS= -version-info 1:0:0 $(SDL_LIBS)
libsound_alsa0_5_la_LDFLAGS= -version-info 1:0:0 $(ALSA_LIBS)
libsound_alsa0_9_la_LDFLAGS= -version-info 1:0:0 $(ALSA_LIBS)
libsound_mme_la_LDFLAGS= -version-info 1:0:0 $(MME_LIBS)
libsound_oss_la_LDFLAGS= -version-info 1:0:0 $(OSS_LIBS)
libsound_sgi_la_LDFLAGS= -version-info 1:0:0 $(SGISND_LIBS)
libsound_sun_la_LDFLAGS= -version-info 1:0:0 $(SUN_LIBS)
libsound_win_la_LDFLAGS= -version-info 1:0:0 $(WIN_LIBS)
libsound_disk_la_LDFLAGS= -version-info 1:0:0
libsound_null_la_LDFLAGS= -version-info 1:0:0

libsound_sdl_la_SOURCES= $(common_sound_files) snd_sdl.c
libsound_sdl.la: $(libsound_sdl_la_OBJECTS) $(libsound_sdl_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsound_sdl_la_LDFLAGS) $(libsound_sdl_la_OBJECTS) $(libsound_sdl_la_LIBADD) $(LIBS)

libsound_alsa0_5_la_SOURCES= $(common_sound_files) snd_alsa_0_5.c
libsound_alsa0_5.la: $(libsound_alsa0_5_la_OBJECTS) $(libsound_alsa0_5_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsound_alsa0_5_la_LDFLAGS) $(libsound_alsa0_5_la_OBJECTS) $(libsound_alsa0_5_la_LIBADD) $(LIBS)

libsound_alsa0_9_la_SOURCES= $(common_sound_files) snd_alsa_0_9.c
libsound_alsa0_9.la: $(libsound_alsa0_9_la_OBJECTS) $(libsound_alsa0_9_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsound_alsa0_9_la_LDFLAGS) $(libsound_alsa0_9_la_OBJECTS) $(libsound_alsa0_9_la_LIBADD) $(LIBS)

libsound_mme_la_SOURCES= $(common_sound_files) snd_mme.c
libsound_mme.la: $(libsound_mme_la_OBJECTS) $(libsound_mme_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsound_mme_la_LDFLAGS) $(libsound_mme_la_OBJECTS) $(libsound_mme_la_LIBADD) $(LIBS)

libsound_oss_la_SOURCES= $(common_sound_files) snd_oss.c
libsound_oss.la: $(libsound_oss_la_OBJECTS) $(libsound_oss_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsound_oss_la_LDFLAGS) $(libsound_oss_la_OBJECTS) $(libsound_oss_la_LIBADD) $(LIBS)

libsound_sgi_la_SOURCES= $(common_sound_files) snd_sgi.c
libsound_sgi.la: $(libsound_sgi_la_OBJECTS) $(libsound_sgi_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsound_sgi_la_LDFLAGS) $(libsound_sgi_la_OBJECTS) $(libsound_sgi_la_LIBADD) $(LIBS)

libsound_sun_la_SOURCES= $(common_sound_files) snd_sun.c
libsound_sun.la: $(libsound_sun_la_OBJECTS) $(libsound_sun_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsound_sun_la_LDFLAGS) $(libsound_sun_la_OBJECTS) $(libsound_sun_la_LIBADD) $(LIBS)

libsound_win_la_SOURCES= $(common_sound_files) snd_win.c
libsound_win.la: $(libsound_win_la_OBJECTS) $(libsound_win_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsound_win_la_LDFLAGS) $(libsound_win_la_OBJECTS) $(libsound_win_la_LIBADD) $(LIBS)

libsound_disk_la_SOURCES= $(common_sound_files) snd_disk.c
libsound_disk.la: $(libsound_disk_la_OBJECTS) $(libsound_disk_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsound_disk_la_LDFLAGS) $(libsound_disk_la_OBJECTS) $(libsound_disk_la_LIBADD) $(LIBS)

libsound_null_la_SOURCES= snd_null.c
libsound_null.la: $(libsound_null_la_OBJECTS) $(libsound_null_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsound_null_la_LDFLAGS) $(libsound_null_la_OBJECTS) $(libsound_null_la_LIBADD) $(LIBS)

LIBLIST = $(pkglib_LTLIBRARIES) @LIBRARY_SEARCH_PATH@
