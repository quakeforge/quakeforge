AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	libbrush.la @brush_libs@
EXTRA_LTLIBRARIES=libbrush_gl.la libbrush_glsl.la libbrush_sw.la

brush_src= model_brush.c
gl_src= gl_model_brush.c
glsl_src= glsl_model_brush.c
sw_src= sw_model_brush.c

libbrush_la_SOURCES= $(brush_src)

libbrush_gl_la_SOURCES= $(gl_src) $(brush_src)

libbrush_glsl_la_SOURCES= $(glsl_src) $(brush_src)

libbrush_sw_la_SOURCES= $(sw_src) $(brush_src)

EXTRA_DIST= $(gl_src) $(glsl_src) $(sw_src) $(brush_src)
