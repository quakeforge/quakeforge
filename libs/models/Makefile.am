AUTOMAKE_OPTIONS= foreign

SUBDIRS= alias brush sprite . test
AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES=	libQFmodels.la @VID_MODEL_TARGETS@
EXTRA_LTLIBRARIES=	libQFmodels_gl.la libQFmodels_glsl.la libQFmodels_sw.la

models_sources = clip_hull.c model.c portal.c skin.c trace.c winding.c

libQFmodels_la_LDFLAGS= 	-version-info $(QUAKE_LIBRARY_VERSION_INFO) -no-undefined
libQFmodels_la_LIBADD=		brush/libbrush.la $(top_builddir)/libs/util/libQFutil.la
libQFmodels_la_SOURCES= 	$(models_sources) null_model.c
libQFmodels_la_DEPENDENCIES=	brush/libbrush.la

gl_LIBS= \
	alias/libalias_gl.la brush/libbrush_gl.la sprite/libsprite_gl.la
libQFmodels_gl_la_LDFLAGS=	-version-info $(QUAKE_LIBRARY_VERSION_INFO) -rpath $(libdir)
libQFmodels_gl_la_LIBADD=	$(gl_LIBS) $(top_builddir)/libs/util/libQFutil.la
libQFmodels_gl_la_SOURCES=	$(models_sources) gl_model_fullbright.c gl_skin.c
libQFmodels_gl_la_DEPENDENCIES= $(gl_LIBS)

glsl_LIBS= \
	alias/libalias_glsl.la brush/libbrush_glsl.la sprite/libsprite_glsl.la
libQFmodels_glsl_la_LDFLAGS=	-version-info $(QUAKE_LIBRARY_VERSION_INFO) -rpath $(libdir)
libQFmodels_glsl_la_LIBADD=	$(glsl_LIBS) $(top_builddir)/libs/util/libQFutil.la
libQFmodels_glsl_la_SOURCES=	$(models_sources) glsl_skin.c
libQFmodels_glsl_la_DEPENDENCIES= $(glsl_LIBS)

sw_LIBS= \
	alias/libalias_sw.la brush/libbrush_sw.la sprite/libsprite_sw.la
libQFmodels_sw_la_LDFLAGS=	-version-info $(QUAKE_LIBRARY_VERSION_INFO) -rpath $(libdir) -no-undefined
libQFmodels_sw_la_LIBADD=	$(sw_LIBS) $(top_builddir)/libs/util/libQFutil.la
libQFmodels_sw_la_SOURCES=	$(models_sources) sw_skin.c
libQFmodels_sw_la_DEPENDENCIES= $(sw_LIBS)
