INCLUDES= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	libalias_gl.la libalias_sw.la libalias_sw32.la

if BUILD_GL
libalias_gl_la_SOURCES= gl_mesh.c gl_model_alias.c model_alias.c
else
libalias_gl_la_SOURCES=
endif

if BUILD_SW
libalias_sw_la_SOURCES= sw_model_alias.c model_alias.c
else
libalias_sw_la_SOURCES=
endif

if BUILD_SW32
libalias_sw32_la_SOURCES=	sw32_model_alias.c model_alias.c
else
libalias_sw32_la_SOURCES=
endif
