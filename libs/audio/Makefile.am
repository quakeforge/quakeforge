SUBDIRS=	cd targets

clean-local:
	rm -f *.a

INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES = libQFsound.la

libQFsound_la_SOURCES= snd_common.c snd.c
libQFsound_la_LDFLAGS = -version-info 1:0:0
libQFsound.la: $(libQFsound_la_OBJECTS) $(libQFsound_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFsound_la_LDFLAGS) $(libQFsound_la_OBJECTS) $(libQFsound_la_LIBADD) $(LIBS)

LIBLIST = libQFsound.la @LIBRARY_SEARCH_PATH@
