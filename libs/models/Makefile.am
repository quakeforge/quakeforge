AUTOMAKE_OPTIONS= foreign

SUBDIRS= alias brush sprite . test
AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES=	libQFmodels.la @QFmodels_libs@
EXTRA_LTLIBRARIES=	libQFmodels_gl.la libQFmodels_glsl.la libQFmodels_sw.la

lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined

models_sources = clip_hull.c model.c portal.c trace.c winding.c

common_libs = \
	$(top_builddir)/libs/image/libQFimage.la \
	$(top_builddir)/libs/util/libQFutil.la

models_libs=brush/libbrush.la $(common_libs)

libQFmodels_la_LDFLAGS= 			$(lib_ldflags)
libQFmodels_la_LIBADD=				$(models_libs)
libQFmodels_la_DEPENDENCIES=		$(models_libs)
libQFmodels_la_SOURCES= 			$(models_sources) null_model.c

gl_sources=gl_model_fullbright.c gl_skin.c skin.c
gl_libs= \
	alias/libalias_gl.la \
	brush/libbrush_gl.la \
	sprite/libsprite_gl.la
libQFmodels_gl_la_LDFLAGS=			$(lib_ldflags)
libQFmodels_gl_la_LIBADD=			$(gl_libs)
libQFmodels_gl_la_DEPENDENCIES=		$(gl_libs)
libQFmodels_gl_la_SOURCES=			$(models_sources) $(gl_sources)

glsl_libs= \
	alias/libalias_glsl.la \
	brush/libbrush_glsl.la \
	sprite/libsprite_glsl.la \
	$(common_libs)
libQFmodels_glsl_la_LDFLAGS=		$(lib_ldflags)
libQFmodels_glsl_la_LIBADD=			$(glsl_libs)
libQFmodels_glsl_la_DEPENDENCIES=	$(glsl_libs)
libQFmodels_glsl_la_SOURCES=		$(models_sources) glsl_skin.c skin.c

sw_libs= \
	alias/libalias_sw.la \
	brush/libbrush_sw.la \
	sprite/libsprite_sw.la \
	$(common_libs)
libQFmodels_sw_la_LDFLAGS=			$(lib_ldflags)
libQFmodels_sw_la_LIBADD=			$(sw_libs)
libQFmodels_sw_la_DEPENDENCIES=		$(sw_libs)
libQFmodels_sw_la_SOURCES=			$(models_sources) sw_skin.c skin.c
