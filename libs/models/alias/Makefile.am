INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
ALIAS_GL = libalias_gl.la
else
ALIAS_GL =
endif

if BUILD_SW
ALIAS_SW = libalias_sw.la
else
ALIAS_SW =
endif

if BUILD_SW32
ALIAS_SW32 = libalias_sw32.la
else
ALIAS_SW32 =
endif

noinst_LTLIBRARIES = $(ALIAS_GL) $(ALIAS_SW) $(ALIAS_SW32)

libalias_gl_la_LDFLAGS = -version-info 1:0:0
libalias_gl_la_SOURCES = gl_mesh.c gl_model_alias.c model_alias.c

libalias_sw_la_LDFLAGS = -version-info 1:0:0
libalias_sw_la_SOURCES = sw_model_alias.c model_alias.c

libalias_sw32_la_LDFLAGS = -version-info 1:0:0
libalias_sw32_la_SOURCES = sw32_model_alias.c model_alias.c
