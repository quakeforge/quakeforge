INCLUDES= -I$(top_srcdir)/include $(SOUND_CFLAGS)

lib_LTLIBRARIES = @SND_TARGETS@
EXTRA_LTLIBRARIES = libQFsound.la

libQFsound_la_LDFLAGS = -version-info 1:0:0 $(CD_LIBS)

#
# ... Audio targets
#
common_sound_files=	snd_common.c snd_dma.c snd_mem.c snd_mix.c snd_mixa.S
if SNDTYPE_SDL
libQFsound_la_SOURCES=	$(common_sound_files) snd_sdl.c
endif
if SNDTYPE_ALSA_0_5
libQFsound_la_SOURCES=	$(common_sound_files) snd_alsa_0_5.c
endif
if SNDTYPE_ALSA_0_9
libQFsound_la_SOURCES=	$(common_sound_files) snd_alsa_0_9.c
endif
# No idea what this is.  it's in configure.in though...
#if SNDTYPE_MME
#libQFsound_la_SOURCES=	$(common_sound_files) snd_mme.c
#endif
if SNDTYPE_OSS
libQFsound_la_SOURCES=	$(common_sound_files) snd_oss.c
endif
if SNDTYPE_SGI
libQFsound_la_SOURCES=	$(common_sound_files) snd_sgi.c
endif
if SNDTYPE_SUN
libQFsound_la_SOURCES=	$(common_sound_files) snd_sun.c
endif
if SNDTYPE_WIN32
libQFsound_la_SOURCES=	$(common_sound_files) snd_win.c
endif
if SNDTYPE_NULL
libQFsound_la_SOURCES=	snd_common.c snd_null.c
endif
libQFsound.la: $(libQFsound_la_OBJECTS) $(libQFsound_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFsound_la_LDFLAGS) $(libQFsound_la_OBJECTS) $(libQFsound_la_LIBADD) $(LIBS)


LIBLIST = libQFsound.la @LIBRARY_SEARCH_PATH@
