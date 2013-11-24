AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	@sprite_libs@
EXTRA_LTLIBRARIES=libsprite_gl.la libsprite_glsl.la libsprite_sw.la

sprite_src= model_sprite.c
gl_src= gl_model_sprite.c
glsl_src= glsl_model_sprite.c
sw_src= sw_model_sprite.c

libsprite_gl_la_SOURCES=	$(gl_src) $(sprite_src)

libsprite_glsl_la_SOURCES=	$(glsl_src) $(sprite_src)

libsprite_sw_la_SOURCES=	$(sw_src) $(sprite_src)

EXTRA_DIST= $(gl_src) $(glsl_src) $(sw_src) $(sprite_src)
