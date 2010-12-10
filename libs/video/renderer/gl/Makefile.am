AUTOMAKE_OPTIONS= foreign

CFLAGS+= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include $(GLX_CFLAGS)

if BUILD_GL
noinst_LTLIBRARIES=	libgl.la
else
noinst_LTLIBRARIES=
endif

gl_src = \
	gl_draw.c gl_dyn_lights.c gl_dyn_part.c gl_dyn_textures.c \
	gl_fog.c gl_graph.c gl_lightmap.c gl_mod_alias.c gl_mod_sprite.c \
	gl_rmain.c gl_rmisc.c gl_rsurf.c gl_screen.c gl_skin.c gl_sky.c \
	gl_sky_clip.c gl_textures.c gl_warp.c noisetextures.c

libgl_la_SOURCES= $(gl_src)

EXTRA_DIST = $(gl_src)
