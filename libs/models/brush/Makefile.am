INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
BRUSH_GL = libbrush_gl.la
AM_CFLAGS = $(GLX_CFLAGS)
else
BRUSH_GL =
endif

if BUILD_SW
BRUSH_SW = libbrush_sw.la
else
BRUSH_SW =
endif

noinst_LTLIBRARIES = $(BRUSH_GL) $(BRUSH_SW) libbrush.la

libbrush_la_LDFLAGS = -version-info 1:0:0
libbrush_la_SOURCES = model_brush.c

libbrush_gl_la_LDFLAGS = -version-info 1:0:0
libbrush_gl_la_SOURCES = gl_model_brush.c model_brush.c

libbrush_sw_la_LDFLAGS = -version-info 1:0:0
libbrush_sw_la_SOURCES = sw_model_brush.c model_brush.c
