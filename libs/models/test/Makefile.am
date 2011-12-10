AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(srcdir) -I$(top_srcdir)/include

check_PROGRAMS=testclip testcontents testportals
EXTRA_DIST=	trace-id.c trace-qf-bad.c hulls.h main.c

testclip_SOURCES=testclip.c hulls.c
testclip_LDADD= $(top_builddir)/libs/models/libQFmodels.la $(top_builddir)/libs/util/libQFutil.la
testclip_DEPENDENCIES= $(top_builddir)/libs/models/libQFmodels.la $(top_builddir)/libs/util/libQFutil.la

testcontents_SOURCES=testcontents.c hulls.c
testcontents_LDADD= $(top_builddir)/libs/models/libQFmodels.la $(top_builddir)/libs/util/libQFutil.la
testcontents_DEPENDENCIES= $(top_builddir)/libs/models/libQFmodels.la $(top_builddir)/libs/util/libQFutil.la

testportals_SOURCES=testportals.c hulls.c
testportals_LDADD= $(top_builddir)/libs/models/libQFmodels.la $(top_builddir)/libs/util/libQFutil.la
testportals_DEPENDENCIES= $(top_builddir)/libs/models/libQFmodels.la $(top_builddir)/libs/util/libQFutil.la

TESTS=$(check_PROGRAMS)
