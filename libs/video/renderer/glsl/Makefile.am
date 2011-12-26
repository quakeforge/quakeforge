AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include $(GLX_CFLAGS)
shader_src= quaketxt.frag quaketxt.vert
shader_gen= quaketxt.fc quaketxt.vc

if BUILD_GL
noinst_LTLIBRARIES=	libglsl.la
BUILT_SOURCES= $(shader_gen)
else
noinst_LTLIBRARIES=
BUILT_SOURCES=
endif

SUFFICES=.frag .vert .fc .vc
.frag.fc:
	sed -e 's/^/"/' -e 's/$$/\\n"/' $< > $@
.vert.vc:
	sed -e 's/^/"/' -e 's/$$/\\n"/' $< > $@

glsl_src = \
	glsl_draw.c glsl_main.c glsl_particles.c glsl_screen.c

libglsl_la_SOURCES= $(glsl_src)

EXTRA_DIST = $(glsl_src) $(shader_src)
CLEANFILES= *.vc *.fc
