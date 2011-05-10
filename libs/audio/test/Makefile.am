AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include

noinst_PROGRAMS= testsound

testsound_SOURCES= testsound.c
testsound_LDADD= $(top_builddir)/libs/audio/libQFsound.la $(top_builddir)/libs/util/libQFutil.la
testsound_DEPENDENCIES= $(top_builddir)/libs/audio/libQFsound.la $(top_builddir)/libs/util/libQFutil.la
