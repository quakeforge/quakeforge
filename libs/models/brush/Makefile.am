INCLUDES= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	libbrush.la libbrush_gl.la libbrush_sw.la libbrush_sw32.la

libbrush_la_SOURCES=	model_brush.c

if BUILD_GL
libbrush_gl_la_SOURCES= gl_model_brush.c model_brush.c
else
libbrush_gl_la_SOURCES=
endif

if BUILD_SW
libbrush_sw_la_SOURCES= sw_model_brush.c model_brush.c
else
libbrush_sw_la_SOURCES=
endif

if BUILD_SW32
libbrush_sw32_la_SOURCES=sw32_model_brush.c model_brush.c
else
libbrush_sw32_la_SOURCES=
endif
