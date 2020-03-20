AUTOMAKE_OPTIONS= foreign

AM_CPPFLAGS= -I$(top_srcdir)/include

check_PROGRAMS= \
	test-bary test-cs test-darray test-dq test-half test-mat3 test-mat4 \
	test-plist test-qfs test-quat test-seb test-seg test-set test-txtbuffer \
	test-vrect

test_bary_SOURCES=test-bary.c
test_bary_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_bary_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_cs_SOURCES=test-cs.c
test_cs_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_cs_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_darray_SOURCES=test-darray.c
test_darray_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_darray_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

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

test_plist_SOURCES=test-plist.c
test_plist_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_plist_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_qfs_SOURCES=test-qfs.c
test_qfs_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_qfs_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_quat_SOURCES=test-quat.c
test_quat_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_quat_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_seb_SOURCES=test-seb.c
test_seb_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_seb_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_seg_SOURCES=test-seg.c
test_seg_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_seg_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_set_SOURCES=test-set.c
test_set_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_set_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_txtbuffer_SOURCES=test-txtbuffer.c
test_txtbuffer_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_txtbuffer_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

test_vrect_SOURCES=test-vrect.c
test_vrect_LDADD=$(top_builddir)/libs/util/libQFutil.la
test_vrect_DEPENDENCIES=$(top_builddir)/libs/util/libQFutil.la

TESTS=$(check_PROGRAMS)
