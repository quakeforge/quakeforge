AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include

noinst_PROGRAMS= @AUDIO_TARGETS@

EXTRA_PROGRAMS= testsound

test_libs= \
	$(top_builddir)/libs/audio/libQFsound.la \
	$(top_builddir)/libs/ruamoko/libQFruamoko.la \
	$(top_builddir)/libs/util/libQFutil.la

testsound_SOURCES=		testsound.c
testsound_LDADD=		$(test_libs)
testsound_DEPENDENCIES=	$(test_libs)
