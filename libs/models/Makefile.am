SUBDIRS= alias brush sprite
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES = libQFmodels.la libQFmodels_gl.la libQFmodels_sw.la

libQFmodels_la_LDFLAGS = -version-info 1:0:0 brush/model_brush.lo
libQFmodels_la_SOURCES = model.c

libQFmodels_gl_la_LDFLAGS = -version-info 1:0:0 alias/libalias_gl.la brush/libbrush_gl.la sprite/libsprite_gl.la
libQFmodels_gl_la_SOURCES = model.c gl_model_fullbright.c

libQFmodels_sw_la_LDFLAGS = -version-info 1:0:0 alias/libalias_sw.la brush/libbrush_sw.la sprite/libsprite_sw.la
libQFmodels_sw_la_SOURCES = model.c

LIBLIST = libQFmodels.la libQFmodels_gl.la libQFmodels_sw.la
