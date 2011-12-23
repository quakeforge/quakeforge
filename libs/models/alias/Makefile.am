AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
gl= libalias_gl.la
else
gl=
endif

if BUILD_GLSL
glsl= libalias_glsl.la
else
glsl=
endif

if BUILD_SW_MOD
sw= libalias_sw.la
else
sw=
endif

noinst_LTLIBRARIES=	$(gl) $(glsl) $(sw)

alias_src= model_alias.c
gl_src= gl_mesh.c gl_model_alias.c mod_flood.c
glsl_src= glsl_mesh.c glsl_model_alias.c mod_flood.c
sw_src= sw_model_alias.c

libalias_gl_la_SOURCES= $(gl_src) $(alias_src)

libalias_glsl_la_SOURCES= $(glsl_src) $(alias_src)

libalias_sw_la_SOURCES= $(sw_src) $(alias_src)

EXTRA_DIST= $(gl_src) $(glsl_src) $(sw_src) $(alias_src)
