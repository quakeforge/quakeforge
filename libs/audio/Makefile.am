AUTOMAKE_OPTIONS= foreign

SUBDIRS= cd targets renderer 
CFLAGS+= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES=	@CD_TARGETS@ @SND_TARGETS@
EXTRA_LTLIBRARIES=	libQFsound.la libQFcd.la

libQFsound_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFsound_la_LIBADD=	@SND_PLUGIN_STATIC_LIBS@ @SND_REND_STATIC_LIBS@ $(top_builddir)/libs/util/libQFutil.la
libQFsound_la_SOURCES=	snd.c
libQFsound_la_DEPENDENCIES=	@SND_PLUGIN_STATIC_LIBS@ @SND_REND_STATIC_LIBS@

libQFcd_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFcd_la_LIBADD=		@CD_PLUGIN_STATIC_LIBS@ $(top_builddir)/libs/util/libQFutil.la
libQFcd_la_SOURCES=		cd.c
libQFcd_la_DEPENDENCIES= @CD_PLUGIN_STATIC_LIBS@
