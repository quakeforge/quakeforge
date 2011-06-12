AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(srcdir) -I$(top_srcdir)/include

check_PROGRAMS=testclip

testclip_SOURCES=testclip.c
testclip_LDADD= $(top_builddir)/libs/models/libQFmodels.la $(top_builddir)/libs/util/libQFutil.la
testclip_DEPENDENCIES= $(top_builddir)/libs/models/libQFmodels.la $(top_builddir)/libs/util/libQFutil.la

TESTS=$(check_PROGRAMS)
