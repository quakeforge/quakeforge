AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include $(GLX_CFLAGS)

shader_src= quake2d.frag quakeico.vert quakespr.frag quakespr.vert quaketxt.vert
shader_gen= quake2d.fc quakeico.vc quakespr.fc quakespr.vc quaketxt.vc

glsl_src = \
	glsl_draw.c glsl_main.c glsl_particles.c glsl_screen.c glsl_sprite.c \
	glsl_textures.c

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

libglsl_la_SOURCES= $(glsl_src)

EXTRA_DIST = $(glsl_src) $(shader_src)
CLEANFILES= *.vc *.fc
