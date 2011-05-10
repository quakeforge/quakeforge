AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
gl= libalias_gl.la
else
gl=
endif

if BUILD_SW_MOD
sw= libalias_sw.la
else
sw=
endif

noinst_LTLIBRARIES=	$(gl) $(sw)

alias_src= model_alias.c
gl_src= gl_mesh.c gl_model_alias.c
sw_src= sw_model_alias.c

libalias_gl_la_SOURCES= $(gl_src) $(alias_src)

libalias_sw_la_SOURCES= $(sw_src) $(alias_src)

EXTRA_DIST= $(gl_src) $(sw_src) $(alias_src)
