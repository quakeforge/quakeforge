AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@ @VORBIS_CFLAGS@ @OGG_CFLAGS@
INCLUDES= -I$(top_srcdir)/include
plugin_version= 1:0:0
plugin_ldflags= @plugin_ldflags@
plugin_libadd= @plugin_libadd@
EXEEXT=

plugin_PROGRAMS=	@SND_REND_TARGETS@
EXTRA_PROGRAMS=	snd_render_default.la

if ASM_ARCH
asm= libasm.la
else
asm=
endif

noinst_LTLIBRARIES= $(asm)
noinst_PROGRAMS=	@SND_REND_STATIC@

libasm_la_SOURCES=	snd_mixa.S

snd_render_default_la_LDFLAGS=	$(plugin_ldflags)
snd_render_default_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c vorbis.c
if ASM_ARCH
snd_render_default_la_LDADD=	$(asm) $(VORBISFILE_LIBS) $(VORBIS_LIBS) $(OGG_LIBS)
snd_render_default_la_DEPENDENCIES=	$(asm)
else
snd_render_default_la_LDADD=	$(VORBISFILE_LIBS) $(VORBIS_LIBS) $(OGG_LIBS)
snd_render_default_la_DEPENDENCIES=
endif

EXTRA_DIST= $(libasm_la_SOURCES)
