AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include
plugin_version= 1:0:0
plugin_ldflags= @plugin_ldflags@
plugin_libadd= @plugin_libadd@

plugin_LTLIBRARIES=	@SND_REND_TARGETS@
EXTRA_LTLIBRARIES=	libsnd_render_default.la

if ASM_ARCH
asm= libasm.la
else
asm=
endif

noinst_LTLIBRARIES=	$(asm) @SND_REND_STATIC@

libasm_la_SOURCES=	snd_mixa.S

libsnd_render_default_la_LDFLAGS=	$(plugin_ldflags)
libsnd_render_default_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c vorbis.c
if ASM_ARCH
libsnd_render_default_la_LIBADD=	$(asm) $(VORBISFILE_LIBS) $(VORBIS_LIBS) $(OGG_LIBS)
libsnd_render_default_la_DEPENDENCIES=	$(asm)
else
libsnd_render_default_la_LIBADD=	$(VORBISFILE_LIBS) $(VORBIS_LIBS) $(OGG_LIBS)
libsnd_render_default_la_DEPENDENCIES=
endif

EXTRA_DIST= $(libasm_la_SOURCES)
