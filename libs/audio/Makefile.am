SUBDIRS=cd targets renderer 
INCLUDES=-I$(top_srcdir)/include

lib_LTLIBRARIES=	@CD_TARGETS@ @SND_TARGETS@
EXTRA_LTLIBRARIES=	libQFsound.la libQFcd.la

libQFsound_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFsound_la_SOURCES=	snd.c

libQFcd_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFcd_la_SOURCES=	cd.c
