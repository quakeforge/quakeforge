AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include $(GLX_CFLAGS)

shader_src= \
	iqm.frag iqm.vert \
	quake2d.frag quakebsp.frag quakebsp.vert quakeico.vert quakemdl.frag \
	quakemdl.vert quakepar.frag quakepar.vert quakepnt.frag quakepnt.vert \
	quakeskb.frag quakeski.frag quakesky.vert quakespr.frag quakespr.vert \
	quaketrb.frag quaketxt.vert
shader_gen= \
	iqm.fc iqm.vc \
	quake2d.fc quakebsp.fc quakebsp.vc quakeico.vc quakemdl.fc quakemdl.vc \
	quakepar.fc quakepar.vc quakepnt.fc quakepnt.vc quakeskb.fc quakeski.fc \
	quakesky.vc quakespr.fc quakespr.vc quaketrb.fc quaketxt.vc

glsl_src = \
	glsl_alias.c glsl_bsp.c glsl_draw.c glsl_fog.c glsl_iqm.c glsl_lightmap.c \
	glsl_main.c glsl_particles.c glsl_screen.c glsl_shader.c glsl_sprite.c \
	glsl_textures.c qfglsl.c vid_common_glsl.c

noinst_LTLIBRARIES=	libglsl.la
BUILT_SOURCES= $(shader_gen)

SUFFICES=.frag .vert .fc .vc
.frag.fc:
	sed -e 's/^/"/' -e 's/$$/\\n"/' $< > $@
.vert.vc:
	sed -e 's/^/"/' -e 's/$$/\\n"/' $< > $@

libglsl_la_SOURCES= $(glsl_src)

EXTRA_DIST = $(glsl_src) $(shader_src) namehack.h
CLEANFILES= *.vc *.fc
