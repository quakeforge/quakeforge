INCLUDES= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	libsprite_gl.la libsprite_sw.la libsprite_sw32.la

if BUILD_GL
libsprite_gl_la_SOURCES=	gl_model_sprite.c model_sprite.c
else
libsprite_gl_la_SOURCES=
endif

if BUILD_SW
libsprite_sw_la_SOURCES=	sw_model_sprite.c model_sprite.c
else
libsprite_sw_la_SOURCES=
endif

if BUILD_SW32
libsprite_sw32_la_SOURCES=	sw32_model_sprite.c model_sprite.c
else
libsprite_sw32_la_SOURCES=
endif
