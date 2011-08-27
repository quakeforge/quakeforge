AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include

check_PROGRAMS=test-qfs test-quat

test_qfs_SOURCES=test-qfs.c
test_qfs_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_qfs_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_quat_SOURCES=test-quat.c
test_quat_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_quat_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

TESTS=$(check_PROGRAMS)
