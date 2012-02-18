AUTOMAKE_OPTIONS= foreign

SUBDIRS= @QFrenderer_dirs@
DIST_SUBDIRS= gl glsl sw sw32
INCLUDES= -I$(top_srcdir)/include

#lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
#			-rpath $(libdir) -no-undefined

#lib_LTLIBRARIES= @VID_REND_TARGETS@
noinst_LTLIBRARIES= @QFrenderer_libs@

EXTRA_LTLIBRARIES=	libQFrenderer_sw.la libQFrenderer_sw32.la \
					libQFrenderer_gl.la libQFrenderer_glsl.la

common_sources= \
	crosshair.c noisetextures.c r_alias.c r_bsp.c r_cvar.c r_dyn_textures.c \
	r_efrag.c r_ent.c r_graph.c r_light.c r_main.c r_part.c r_progs.c \
	r_screen.c

libQFrenderer_gl_la_LDFLAGS=		$(lib_ldflags)
libQFrenderer_gl_la_LIBADD=			gl/libgl.la
libQFrenderer_gl_la_DEPENDENCIES=	gl/libgl.la
libQFrenderer_gl_la_SOURCES=		$(common_sources)

libQFrenderer_glsl_la_LDFLAGS=		$(lib_ldflags)
libQFrenderer_glsl_la_LIBADD=		glsl/libglsl.la
libQFrenderer_glsl_la_DEPENDENCIES=	glsl/libglsl.la
libQFrenderer_glsl_la_SOURCES=		$(common_sources)

libQFrenderer_sw_la_LDFLAGS=		@STATIC@
libQFrenderer_sw_la_LIBADD=			sw/libsw.la
libQFrenderer_sw_la_DEPENDENCIES=	sw/libsw.la
libQFrenderer_sw_la_SOURCES=		$(common_sources)

libQFrenderer_sw32_la_LDFLAGS=		$(lib_ldflags)
libQFrenderer_sw32_la_LIBADD=		sw32/libsw32.la
libQFrenderer_sw32_la_DEPENDENCIES=	sw32/libsw32.la 
libQFrenderer_sw32_la_SOURCES=		$(common_sources)
