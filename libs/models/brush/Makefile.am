INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
gl= libbrush_gl.la
else
gl=
endif

if BUILD_SW
sw= libbrush_sw.la
else
sw=
endif

if BUILD_SW32
sw32= libbrush_sw32.la
else
sw32=
endif

noinst_LTLIBRARIES=	libbrush.la $(gl) $(sw) $(sw32)

brush_src= model_brush.c
gl_src= gl_model_brush.c
sw_src= sw_model_brush.c
sw32_src= sw32_model_brush.c

libbrush_la_SOURCES= $(brush_src)

libbrush_gl_la_SOURCES= $(gl_src) $(brush_src)

libbrush_sw_la_SOURCES= $(sw_src) $(brush_src)

libbrush_sw32_la_SOURCES= $(sw32_src) $(brush_src)

EXTRA_DIST= $(gl_src) $(sw_src) $(sw32_src) $(brush_src)
