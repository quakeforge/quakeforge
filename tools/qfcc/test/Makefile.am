AUTOMAKE_OPTIONS= foreign
INCLUDES= -I$(top_srcdir)/include $(QFCC_INCS)

QFCC_DEP=$(builddir)/../source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)

QCFLAGS=-qq -g --no-default-paths
QCPPFLAGS=

SUFFIXES=.qfo .r
.r.qfo:
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

QFCC_TEST_LIBS=@QFCC_TEST_LIBS@
QFCC_TEST_DEPS=@QFCC_TEST_DEPS@
QFCC_TEST_INCS=@QFCC_TEST_INCS@

test_progs_dat=structptr.dat while.dat

TESTS=$(test_progs_dat:.dat=.run)

check_PROGRAMS=test-harness $(test_progs_dat)

test_harness_SOURCES=		test-bi.c test-harness.c
test_harness_LDADD=			$(QFCC_TEST_LIBS)
test_harness_DEPENDENCIES=	$(QFCC_TEST_DEPS)

structptr_dat_SOURCES=structptr.r
structptr_obj=$(structptr_dat_SOURCES:.r=.qfo)
structptr.dat: $(structptr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structptr_obj)
structptr.run: Makefile build-run
	$(srcdir)/build-run $@

while_dat_SOURCES=while.r
while_obj=$(while_dat_SOURCES:.r=.qfo)
while.dat: $(while_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(while_obj)
while.run: Makefile build-run
	$(srcdir)/build-run $@

EXTRA_DIST= test-bi.h build-run
CLEANFILES= *.dat *.sym *.qfo *.run
