AUTOMAKE_OPTIONS= foreign

SUBDIRS= alias brush sprite
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES=	libQFmodels.la @VID_MODEL_TARGETS@
EXTRA_LTLIBRARIES= \
	libQFmodels_gl.la libQFmodels_sw.la

models_sources = clip_hull.c model.c

libQFmodels_la_LDFLAGS= 	-version-info 1:0:0
libQFmodels_la_LIBADD=		brush/libbrush.la
libQFmodels_la_SOURCES= 	$(models_sources)
libQFmodels_la_DEPENDENCIES=	brush/libbrush.la

gl_LIBS= \
	alias/libalias_gl.la brush/libbrush_gl.la sprite/libsprite_gl.la
libQFmodels_gl_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFmodels_gl_la_LIBADD=	$(gl_LIBS)
libQFmodels_gl_la_SOURCES=	$(models_sources) gl_model_fullbright.c
libQFmodels_gl_la_DEPENDENCIES= $(gl_LIBS)

sw_LIBS= \
	alias/libalias_sw.la brush/libbrush_sw.la sprite/libsprite_sw.la
libQFmodels_sw_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFmodels_sw_la_LIBADD=	$(sw_LIBS)
libQFmodels_sw_la_SOURCES=	$(models_sources)
libQFmodels_sw_la_DEPENDENCIES= $(sw_LIBS)
