SUBDIRS= gl sw sw32
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFrenderer_sw.la libQFrenderer_sw32.la libQFrenderer_gl.la

common_sources= r_cvar.c r_efrag.c r_ent.c r_graph.c r_light.c r_main.c r_part.c

libQFrenderer_gl_la_LDFLAGS=		-version-info 1:0:0
if BUILD_GL
libQFrenderer_gl_la_LIBADD=		gl/libgl.la
libQFrenderer_gl_la_SOURCES=		$(common_sources)
libQFrenderer_gl_la_DEPENDENCIES=	gl/libgl.la
else
libQFrenderer_gl_la_SOURCES=
endif

libQFrenderer_sw_la_LDFLAGS=		-version-info 1:0:0
if BUILD_SW
libQFrenderer_sw_la_LIBADD=		sw/libsw.la
libQFrenderer_sw_la_SOURCES=		$(common_sources)
libQFrenderer_sw_la_DEPENDENCIES=	sw/libsw.la
else
libQFrenderer_sw_la_SOURCES=
endif

libQFrenderer_sw32_la_LDFLAGS=		-version-info 1:0:0
if BUILD_SW32
libQFrenderer_sw32_la_LIBADD=		sw32/libsw32.la 
libQFrenderer_sw32_la_SOURCES=		$(common_sources)
libQFrenderer_sw32_la_DEPENDENCIES=	sw32/libsw32.la 
else
libQFrenderer_sw32_la_SOURCES=
endif

#LIBLIST = libQFrenderer_gl.la libQFrenderer_sw.la libQFrenderer_sw32.la
