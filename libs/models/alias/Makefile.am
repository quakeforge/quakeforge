AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	@alias_libs@
EXTRA_LTLIBRARIES=libalias_gl.la libalias_glsl.la libalias_sw.la

alias_src= model_alias.c
gl_src= gl_mesh.c gl_model_alias.c floodfill.c
glsl_src= glsl_model_alias.c floodfill.c
sw_src= sw_model_alias.c

libalias_gl_la_SOURCES= $(gl_src) $(alias_src)

libalias_glsl_la_SOURCES= $(glsl_src) $(alias_src)

libalias_sw_la_SOURCES= $(sw_src) $(alias_src)

EXTRA_DIST= $(gl_src) $(glsl_src) $(sw_src) $(alias_src)
