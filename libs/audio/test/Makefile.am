AUTOMAKE_OPTIONS= foreign

AM_CPPFLAGS= -I$(top_srcdir)/include

noinst_PROGRAMS= @AUDIO_TARGETS@

EXTRA_PROGRAMS= testsound

test_libs= \
	libs/audio/libQFsound.la \
	libs/ruamoko/libQFruamoko.la \
	libs/util/libQFutil.la

testsound_SOURCES=		testsound.c
testsound_LDADD=		$(test_libs)
testsound_DEPENDENCIES=	$(test_libs)
