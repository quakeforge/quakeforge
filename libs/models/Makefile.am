AUTOMAKE_OPTIONS= foreign

SUBDIRS= alias brush iqm sprite . test
AM_CFLAGS= @PREFER_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include

lib_LTLIBRARIES=	libQFmodels.la
noinst_LTLIBRARIES= @models_libs@
EXTRA_LTLIBRARIES=	libmodels_gl.la libmodels_glsl.la libmodels_sw.la

lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined

models_sources = clip_hull.c model.c portal.c trace.c winding.c

common_libs = \
	$(top_builddir)/libs/util/libQFutil.la

models_libs=brush/libbrush.la $(common_libs)

libQFmodels_la_LDFLAGS= 			$(lib_ldflags)
libQFmodels_la_LIBADD=				$(models_libs)
libQFmodels_la_DEPENDENCIES=		$(models_libs)
libQFmodels_la_SOURCES= 			$(models_sources)

gl_sources=gl_model_fullbright.c gl_skin.c skin.c
gl_libs= \
	alias/libalias_gl.la \
	brush/libbrush_gl.la \
	iqm/libiqm_gl.la \
	sprite/libsprite_gl.la \
	$(top_builddir)/libs/image/libQFimage.la
libmodels_gl_la_LDFLAGS=		
libmodels_gl_la_LIBADD=			$(gl_libs)
libmodels_gl_la_DEPENDENCIES=	$(gl_libs)
libmodels_gl_la_SOURCES=		$(gl_sources)

glsl_libs= \
	alias/libalias_glsl.la \
	brush/libbrush_glsl.la \
	iqm/libiqm_glsl.la \
	sprite/libsprite_glsl.la \
	$(top_builddir)/libs/image/libQFimage.la
libmodels_glsl_la_LDFLAGS=		
libmodels_glsl_la_LIBADD=		$(glsl_libs)
libmodels_glsl_la_DEPENDENCIES=	$(glsl_libs)
libmodels_glsl_la_SOURCES=		glsl_skin.c skin.c

sw_libs= \
	alias/libalias_sw.la \
	brush/libbrush_sw.la \
	iqm/libiqm_sw.la \
	sprite/libsprite_sw.la \
	$(top_builddir)/libs/image/libQFimage.la
libmodels_sw_la_LDFLAGS=		
libmodels_sw_la_LIBADD=			$(sw_libs)
libmodels_sw_la_DEPENDENCIES=	$(sw_libs)
libmodels_sw_la_SOURCES=		sw_skin.c skin.c
