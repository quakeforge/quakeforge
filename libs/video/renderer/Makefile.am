AUTOMAKE_OPTIONS= foreign

SUBDIRS= gl sw sw32
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= @VID_REND_TARGETS@
noinst_LTLIBRARIES= @VID_REND_NOINST_TARGETS@

EXTRA_LTLIBRARIES= libQFrenderer_sw.la libQFrenderer_sw32.la libQFrenderer_gl.la

common_sources= \
	r_cvar.c r_efrag.c r_ent.c r_graph.c r_light.c r_main.c \
	r_part.c r_progs.c r_screen.c

libQFrenderer_gl_la_LDFLAGS=		-version-info 1:0:0 -rpath $(libdir)
libQFrenderer_gl_la_LIBADD=			gl/libgl.la
libQFrenderer_gl_la_SOURCES=		$(common_sources)
libQFrenderer_gl_la_DEPENDENCIES=	gl/libgl.la

libQFrenderer_sw_la_LDFLAGS=		@STATIC@
libQFrenderer_sw_la_LIBADD=			sw/libsw.la
libQFrenderer_sw_la_SOURCES=		$(common_sources)
libQFrenderer_sw_la_DEPENDENCIES=	sw/libsw.la

libQFrenderer_sw32_la_LDFLAGS=		-version-info 1:0:0 -rpath $(libdir)
libQFrenderer_sw32_la_LIBADD=		sw32/libsw32.la
libQFrenderer_sw32_la_SOURCES=		$(common_sources)
libQFrenderer_sw32_la_DEPENDENCIES=	sw32/libsw32.la 
