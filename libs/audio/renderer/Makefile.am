INCLUDES= -I$(top_srcdir)/include

libdir= 		@PLUGINDIR@

lib_LTLIBRARIES=	@SND_REND_TARGETS@
EXTRA_LTLIBRARIES=	libsnd_render_default.la
noinst_LTLIBRARIES=	libasm.la

if ASM_ARCH
libasm_la_SOURCES=	snd_mixa.S
else
libasm_la_SOURCES=
endif

libsnd_render_default_la_LDFLAGS=	-version-info 1:0:0 -rpath $(pluginpath)
libsnd_render_default_la_LIBADD=	libasm.la
libsnd_render_default_la_DEPENDENCIES=	libasm.la
libsnd_render_default_la_SOURCES=	snd_dma.c snd_mem.c snd_mix.c
