INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
GL = libgl.la
AM_CFLAGS = $(GLX_CFLAGS)
else
GL =
endif

noinst_LTLIBRARIES = $(GL)

libgl_la_LDFLAGS = -version-info 1:0:0
libgl_la_SOURCES = gl_draw.c gl_dyn_fires.c gl_dyn_part.c gl_dyn_textures.c \
	gl_graph.c gl_rlight.c gl_rmain.c gl_rmisc.c gl_rsurf.c gl_screen.c \
	gl_skin.c gl_sky.c gl_sky_clip.c gl_textures.c gl_warp.c gl_funcs.c \
	noisetextures.c
