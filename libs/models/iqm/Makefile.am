AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	@iqm_libs@
EXTRA_LTLIBRARIES=libiqm_gl.la libiqm_glsl.la libiqm_sw.la

iqm_src= model_iqm.c
gl_src= gl_model_iqm.c
glsl_src= glsl_model_iqm.c
sw_src= sw_model_iqm.c

libiqm_gl_la_SOURCES= $(gl_src) $(iqm_src)

libiqm_glsl_la_SOURCES= $(glsl_src) $(iqm_src)

libiqm_sw_la_SOURCES= $(sw_src) $(iqm_src)

EXTRA_DIST= $(gl_src) $(glsl_src) $(sw_src) $(iqm_src)
