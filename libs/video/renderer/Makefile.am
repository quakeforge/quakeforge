AUTOMAKE_OPTIONS= foreign

SUBDIRS= @vid_render_dirs@
DIST_SUBDIRS= gl glsl sw sw32
INCLUDES= -I$(top_srcdir)/include

lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined
plugin_ldflags= @plugin_ldflags@ -avoid-version -module -rpath $(plugindir)
plugin_libadd= @plugin_libadd@
EXEEXT=

#lib_LTLIBRARIES= @VID_REND_TARGETS@
plugin_LTLIBRARIES= @vid_render_plugins@
noinst_LTLIBRARIES= libQFrenderer.la @vid_render_static_plugins@

EXTRA_LTLIBRARIES=	\
					vid_render_sw.la vid_render_sw32.la \
					vid_render_gl.la vid_render_glsl.la

common_sources= \
	crosshair.c noisetextures.c r_alias.c r_bsp.c r_cvar.c r_dyn_textures.c \
	r_efrag.c r_ent.c r_graph.c r_light.c r_main.c r_part.c r_screen.c \
	vid_common.c

renderer_libs= \
	@vid_render_static_plugin_libs@ \
	$(top_builddir)/libs/util/libQFutil.la

libQFrenderer_la_LDFLAGS=		$(lib_ldflags)
libQFrenderer_la_LIBADD=		$(renderer_libs)
libQFrenderer_la_DEPENDENCIES=	$(renderer_libs)
libQFrenderer_la_SOURCES=		r_init.c r_progs.c

gl_libs= \
	gl/libgl.la \
	$(top_builddir)/libs/models/libmodels_gl.la
vid_render_gl_la_LDFLAGS=		$(plugin_ldflags)
vid_render_gl_la_LIBADD=		$(gl_libs)
vid_render_gl_la_DEPENDENCIES=	$(gl_libs)
vid_render_gl_la_SOURCES=		$(common_sources) vid_render_gl.c

glsl_libs= \
	glsl/libglsl.la \
	$(top_builddir)/libs/models/libmodels_glsl.la
vid_render_glsl_la_LDFLAGS=		$(plugin_ldflags)
vid_render_glsl_la_LIBADD=		$(glsl_libs)
vid_render_glsl_la_DEPENDENCIES=$(glsl_libs)
vid_render_glsl_la_SOURCES=		$(common_sources) vid_render_glsl.c

sw_libs= \
	sw/libsw.la \
	$(top_builddir)/libs/models/libmodels_sw.la
vid_render_sw_la_LDFLAGS=		$(plugin_ldflags)
vid_render_sw_la_LIBADD=		$(sw_libs)
vid_render_sw_la_DEPENDENCIES=	$(sw_libs)
vid_render_sw_la_SOURCES=		$(common_sources) vid_render_sw.c

sw32_libs= \
	sw32/libsw32.la \
	$(top_builddir)/libs/models/libmodels_sw.la
vid_render_sw32_la_LDFLAGS=		$(plugin_ldflags)
vid_render_sw32_la_LIBADD=		$(sw32_libs)
vid_render_sw32_la_DEPENDENCIES=$(sw32_libs)
vid_render_sw32_la_SOURCES=		$(common_sources) vid_render_sw32.c
