AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
gl= libsprite_gl.la
else
gl=
endif

if BUILD_SW_MOD
sw= libsprite_sw.la
else
sw=
endif

noinst_LTLIBRARIES=	$(gl) $(sw)

sprite_src= model_sprite.c
gl_src= gl_model_sprite.c
sw_src= sw_model_sprite.c

libsprite_gl_la_SOURCES=	$(gl_src) $(sprite_src)

libsprite_sw_la_SOURCES=	$(sw_src) $(sprite_src)

EXTRA_DIST= $(gl_src) $(sw_src) $(sprite_src)
