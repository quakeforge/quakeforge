AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include $(GLX_CFLAGS)

if BUILD_GL
noinst_LTLIBRARIES=	libglsl.la
else
noinst_LTLIBRARIES=
endif

glsl_src = \
	glsl_draw.c glsl_main.c

libglsl_la_SOURCES= $(glsl_src)

EXTRA_DIST = $(glsl_src)
