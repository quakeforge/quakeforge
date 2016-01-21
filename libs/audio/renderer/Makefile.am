AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@ \
	$(VORBIS_CFLAGS) $(OGG_CFLAGS) $(SAMPLERATE_CFLAGS) $(JACK_CFLAGS)
AM_CPPFLAGS= -I$(top_srcdir)/include
plugin_ldflags= @plugin_ldflags@ -avoid-version -module -rpath $(plugindir)
plugin_libadd= @plugin_libadd@
EXEEXT=

plugin_LTLIBRARIES=	@snd_render_plugins@
noinst_LTLIBRARIES=	@snd_render_static_plugins@
EXTRA_LTLIBRARIES=	snd_render_default.la snd_render_jack.la

flac_src=flac.c
midi_src=midi.c
vorbis_src=vorbis.c
wav_src=wav.c

if HAVE_FLAC
have_flac_src=$(flac_src)
else
have_flac_src=
endif
if HAVE_MIDI
have_midi_src=$(midi_src)
else
have_midi_src=
endif
if HAVE_VORBIS
have_vorbis_src=$(vorbis_src)
else
have_vorbis_src=
endif
have_wav_src=$(wav_src)

format_src=$(have_flac_src) $(have_midi_src) $(have_vorbis_src) $(have_wav_src)
format_libs= \
	$(SAMPLERATE_LIBS) $(VORBISFILE_LIBS) $(VORBIS_LIBS) $(FLAC_LIBS) \
	$(OGG_LIBS) $(WM_LIBS)
extra_format_src=flac.c midi.c vorbis.c wav.c
snd_common=snd_channels.c snd_mem.c snd_mix.c snd_resample.c snd_sfx.c
snd_libs= \
	$(top_builddir)/libs/util/libQFutil.la

snd_render_default_la_LDFLAGS=		$(plugin_ldflags)
snd_render_default_la_SOURCES=		snd_dma.c $(snd_common) $(format_src)
snd_render_default_la_LIBADD=		$(snd_libs) $(format_libs)
snd_render_default_la_DEPENDENCIES=	$(snd_libs)
EXTRA_snd_render_default_la_SOURCES=$(extra_format_src)

snd_render_jack_la_LDFLAGS=			$(plugin_ldflags)
snd_render_jack_la_SOURCES=			snd_jack.c $(snd_common) $(format_src)
snd_render_jack_la_LIBADD=			$(snd_libs) $(format_libs) $(JACK_LIBS)
snd_render_jack_la_DEPENDENCIES=	$(snd_libs)
EXTRA_snd_render_jack_la_SOURCES=	$(extra_format_src)
