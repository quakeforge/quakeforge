SUBDIRS= gl sw
INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
RENDERER_GL = libQFrenderer_gl.la
else
RENDERER_GL =
endif

if BUILD_SW
RENDERER_SW = libQFrenderer_sw.la
else
RENDERER_SW =
endif

lib_LTLIBRARIES = $(RENDERER_GL) $(RENDERER_SW)

common_SOURCES = r_cvar.c r_efrag.c r_ent.c r_graph.c r_main.c r_part.c

libQFrenderer_gl_la_LDFLAGS = -version-info 1:0:0
libQFrenderer_gl_la_LIBADD = gl/libgl.la
libQFrenderer_gl_la_SOURCES = $(common_SOURCES)
libQFrenderer_gl_la_DEPENDENCIES = gl/libgl.la

libQFrenderer_sw_la_LDFLAGS = -version-info 1:0:0
libQFrenderer_sw_la_LIBADD = sw/libsw.la
libQFrenderer_sw_la_SOURCES = $(common_SOURCES)
libQFrenderer_sw_la_DEPENDENCIES = sw/libsw.la

LIBLIST = libQFrenderer_gl.la libQFrenderer_sw.la
