INCLUDES= -I$(top_srcdir)/include

plugin_LTLIBRARIES=	@SND_REND_TARGETS@
EXTRA_LTLIBRARIES=	libsnd_render_default.la

if ASM_ARCH
asm= libasm.la
else
asm=
endif

noinst_LTLIBRARIES=	$(asm)

libasm_la_SOURCES=	snd_mixa.S

libsnd_render_default_la_LDFLAGS=	-version-info 1:0:0 -rpath $(plugindir)
libsnd_render_default_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c
if ASM_ARCH
libsnd_render_default_la_LIBADD=	$(asm)
libsnd_render_default_la_DEPENDENCIES=	$(asm)
endif

EXTRA_DIST= $(libasm_la_SOURCES)
