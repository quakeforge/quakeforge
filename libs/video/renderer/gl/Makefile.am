AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include $(GLX_CFLAGS)

noinst_LTLIBRARIES=	libgl.la

gl_src = \
	gl_draw.c gl_dyn_lights.c gl_dyn_part.c gl_dyn_textures.c \
	gl_fog.c gl_graph.c gl_lightmap.c gl_mod_alias.c gl_mod_iqm.c \
	gl_mod_sprite.c gl_rmain.c gl_rmisc.c gl_rsurf.c gl_screen.c gl_sky.c \
	gl_sky_clip.c gl_textures.c gl_warp.c qfgl_ext.c vid_common_gl.c vtxarray.c

libgl_la_SOURCES= $(gl_src)

EXTRA_DIST = $(gl_src)
