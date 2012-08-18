AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include

check_PROGRAMS= \
	test-dq test-half test-mat3 test-mat4 test-qfs test-quat test-vrect

test_dq_SOURCES=test-dq.c
test_dq_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_dq_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_half_SOURCES=test-half.c
test_half_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_half_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_mat3_SOURCES=test-mat3.c
test_mat3_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_mat3_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_mat4_SOURCES=test-mat4.c
test_mat4_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_mat4_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_qfs_SOURCES=test-qfs.c
test_qfs_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_qfs_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_quat_SOURCES=test-quat.c
test_quat_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_quat_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_vrect_SOURCES=test-vrect.c
test_vrect_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_vrect_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

TESTS=$(check_PROGRAMS)
