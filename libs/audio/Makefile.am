SUBDIRS=	cd targets renderer

clean-local:
	rm -f *.a

INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= @CD_TARGETS@ @SND_TARGETS@
EXTRA_LTLIBRARIES= libQFsound.la libQFcd.la

libQFsound_la_LDFLAGS= -version-info 1:0:0
libQFsound_la_SOURCES= snd.c
libQFsound.la: $(libQFsound_la_OBJECTS) $(libQFsound_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFsound_la_LDFLAGS) $(libQFsound_la_OBJECTS) $(libQFsound_la_LIBADD) $(LIBS)

libQFcd_la_LDFLAGS= -version-info 1:0:0
libQFcd_la_SOURCES= cd.c
libQFcd.la: $(libQFcd_la_OBJECTS) $(libQFcd_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFcd_la_LDFLAGS) $(libQFcd_la_OBJECTS) $(libQFcd_la_LIBADD) $(LIBS)

LIBLIST = libQFsound.la libQFcd.la @LIBRARY_SEARCH_PATH@
