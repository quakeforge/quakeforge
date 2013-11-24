AUTOMAKE_OPTIONS= foreign

AM_CPPFLAGS= -I$(srcdir) -I$(top_srcdir)/include

check_PROGRAMS=testclip testcontents testportals
EXTRA_DIST=	trace-id.c trace-qf-bad.c hulls.h main.c

test_libs= \
	$(top_builddir)/libs/models/libQFmodels.la \
	$(top_builddir)/libs/util/libQFutil.la

testclip_SOURCES=			testclip.c hulls.c
testclip_LDADD=				$(test_libs)
testclip_DEPENDENCIES=		$(test_libs)

testcontents_SOURCES=		testcontents.c hulls.c
testcontents_LDADD=			$(test_libs)
testcontents_DEPENDENCIES=	$(test_libs)

testportals_SOURCES=		testportals.c hulls.c
testportals_LDADD=			$(test_libs)
testportals_DEPENDENCIES=	$(test_libs)

TESTS=$(check_PROGRAMS)
