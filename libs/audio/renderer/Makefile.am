INCLUDES= -I$(top_srcdir)/include

libdir = @PLUGINDIR@

if BUILD_SND_RENDERER
SND_RENDERER = libsnd_render_default.la
else
SND_RENDERER =
endif

lib_LTLIBRARIES = $(SND_RENDERER)
EXTRA_LTLIBRARIES =

if ASM_ARCH
noinst_LTLIBRARIES = libasm.la
else
noinst_LTLIBRARIES = 
endif

libasm_la_SOURCES = snd_mixa.S

libsnd_render_default_la_LDFLAGS= -version-info 1:0:0
libsnd_render_default_la_LIBADD = libasm.la
libsnd_render_default_la_DEPENDENCIES = libasm.la
libsnd_render_default_la_SOURCES= snd_dma.c snd_mem.c snd_mix.c
libsnd_render_default.la: $(libsnd_render_default_la_OBJECTS) $(libsnd_render_default_la_DEPENDENCIES)
	$(LINK) -rpath $(pluginpath) $(libsnd_render_default_la_LDFLAGS) $(libsnd_render_default_la_OBJECTS) $(libsnd_render_default_la_LIBADD) $(LIBS)

LIBLIST = $(pkglib_LTLIBRARIES) libsnd_default_render.la @LIBRARY_SEARCH_PATH@
