INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
gl= libsprite_gl.la
else
gl=
endif

if BUILD_SW
sw= libsprite_sw.la
else
sw=
endif

if BUILD_SW32
sw32= libsprite_sw32.la
else
sw32=
endif

noinst_LTLIBRARIES=	$(gl) $(sw) $(sw32)

sprite_src= model_sprite.c
gl_src= gl_model_sprite.c
sw_src= sw_model_sprite.c
sw32_src= sw32_model_sprite.c

libsprite_gl_la_SOURCES=	$(gl_src) $(sprite_src)

libsprite_sw_la_SOURCES=	$(sw_src) $(sprite_src)

libsprite_sw32_la_SOURCES=	$(sw32_src) $(sprite_src)

EXTRA_DIST= $(gl_src) $(sw_src) $(sw32_src) $(sprite_src)
