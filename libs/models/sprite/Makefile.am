INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
SPRITE_GL = libsprite_gl.la
else
SPRITE_GL =
endif

if BUILD_SW
SPRITE_SW = libsprite_sw.la
else
SPRITE_SW =
endif

if BUILD_SW32
SPRITE_SW32 = libsprite_sw32.la
else
SPRITE_SW32 =
endif

noinst_LTLIBRARIES = $(SPRITE_GL) $(SPRITE_SW) $(SPRITE_SW32)

libsprite_gl_la_LDFLAGS = -version-info 1:0:0
libsprite_gl_la_SOURCES = gl_model_sprite.c model_sprite.c

libsprite_sw_la_LDFLAGS = -version-info 1:0:0
libsprite_sw_la_SOURCES = sw_model_sprite.c model_sprite.c

libsprite_sw32_la_LDFLAGS = -version-info 1:0:0
libsprite_sw32_la_SOURCES = sw32_model_sprite.c model_sprite.c
