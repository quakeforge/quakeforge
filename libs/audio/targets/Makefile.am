lib_LTLIBRARIES = libQFsound.la

libQFsound_la_LDFLAGS = -version-info 1:0:0 $(CD_LIBS)

#if ASM_ARCH
sound_ASM=	snd_mixa.S
#endif

#
# ... Audio targets
#
if SNDTYPE_SDL
libQFsound_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_sdl.c $(sound_ASM)
endif
if SNDTYPE_ALSA_0_5
libQFsound_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_5.c $(sound_ASM)
endif
if SNDTYPE_ALSA_0_9
libQFsound_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_9.c $(sound_ASM)
endif
# No idea what this is.  it's in configure.in though...
#if SNDTYPE_MME
#libQFsound_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_mme.c $(sound_ASM)
#endif
if SNDTYPE_OSS
libQFsound_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_oss.c $(sound_ASM)
endif
if SNDTYPE_SGI
libQFsound_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_sgi.c $(sound_ASM)
endif
if SNDTYPE_SUN
libQFsound_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_sun.c $(sound_ASM)
endif
if SNDTYPE_WIN32
libQFsound_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_win.c $(sound_ASM)
endif
if SNDTYPE_NULL
libQFsound_la_SOURCES=	snd_mem.c snd_mix.c snd_null.c $(sound_ASM)
endif

LIBLIST = libQFsound.la @LIBRARY_SEARCH_PATH@
