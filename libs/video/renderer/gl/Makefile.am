INCLUDES= -I$(top_srcdir)/include $(GLX_CFLAGS)

noinst_LTLIBRARIES=	libgl.la

EXTRA_libgl_la_SOURCES= \
	gl_draw.c gl_dyn_fires.c gl_dyn_lights.c \
	gl_dyn_part.c gl_dyn_textures.c gl_graph.c gl_rmain.c gl_rmisc.c \
	gl_rsurf.c gl_screen.c gl_skin.c gl_sky.c gl_sky_clip.c \
	gl_textures.c gl_warp.c gl_funcs.c noisetextures.c

if BUILD_GL
 libgl_la_SOURCES= $(EXTRA_libgl_la_SOURCES)
else
 libgl_la_SOURCES=
endif
