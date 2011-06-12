AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include

check_PROGRAMS=test-qfs

test_qfs_SOURCES=test-qfs.c
test_qfs_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_qfs_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

TESTS=$(check_PROGRAMS)
