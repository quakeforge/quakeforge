SUBDIRS= alias brush sprite
INCLUDES= -I$(top_srcdir)/include

if BUILD_GL
MODELS_GL = libQFmodels_gl.la
AM_CFLAGS = $(GLX_CFLAGS)
else
MODELS_GL =
endif

if BUILD_SW
MODELS_SW = libQFmodels_sw.la
else
MODELS_SW =
endif

if BUILD_SW32
MODELS_SW32 = libQFmodels_sw32.la
else
MODELS_SW32 =
endif

lib_LTLIBRARIES = libQFmodels.la $(MODELS_GL) $(MODELS_SW) $(MODELS_SW32)

models_SOURCES = clip_hull.c model.c

libQFmodels_la_LDFLAGS = -version-info 1:0:0
libQFmodels_la_LIBADD = brush/libbrush.la
libQFmodels_la_SOURCES = $(models_SOURCES)
libQFmodels_la_DEPENDENCIES = brush/libbrush.la

libQFmodels_gl_la_LDFLAGS = -version-info 1:0:0
libQFmodels_gl_la_LIBADD = alias/libalias_gl.la brush/libbrush_gl.la sprite/libsprite_gl.la
libQFmodels_gl_la_SOURCES = $(models_SOURCES) gl_model_fullbright.c
libQFmodels_gl_la_DEPENDENCIES = alias/libalias_gl.la brush/libbrush_gl.la sprite/libsprite_gl.la

libQFmodels_sw_la_LDFLAGS = -version-info 1:0:0
libQFmodels_sw_la_LIBADD = alias/libalias_sw.la brush/libbrush_sw.la sprite/libsprite_sw.la
libQFmodels_sw_la_SOURCES = $(models_SOURCES)
libQFmodels_sw_la_DEPENDENCIES = alias/libalias_sw.la brush/libbrush_sw.la sprite/libsprite_sw.la

libQFmodels_sw32_la_LDFLAGS = -version-info 1:0:0
libQFmodels_sw32_la_LIBADD = alias/libalias_sw32.la brush/libbrush_sw32.la sprite/libsprite_sw32.la
libQFmodels_sw32_la_SOURCES = $(models_SOURCES)
libQFmodels_sw32_la_DEPENDENCIES = alias/libalias_sw32.la brush/libbrush_sw32.la sprite/libsprite_sw32.la

LIBLIST = libQFmodels.la libQFmodels_gl.la libQFmodels_sw.la libQFmodels_sw32.la
