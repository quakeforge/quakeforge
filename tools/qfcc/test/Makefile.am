AUTOMAKE_OPTIONS= foreign
INCLUDES= -I$(top_srcdir)/include $(QFCC_INCS)

QFCC_DEP=$(builddir)/../source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)

QCFLAGS=-qq -g --no-default-paths
QCPPFLAGS=
QCOMPILE=$(QFCC) $(QCFLAGS) $(QCPPFLAGS)

SUFFIXES=.qfo .r
.r.qfo:
	$(QCOMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tqo -c -o $@ $<
	sed -i -e '1s@:@: $(QFCC_DEP)@' $(DEPDIR)/$*.Tqo
	$(am__mv) $(DEPDIR)/$*.Tqo $(DEPDIR)/$*.Qo

QFCC_TEST_LIBS=@QFCC_TEST_LIBS@
QFCC_TEST_DEPS=@QFCC_TEST_DEPS@
QFCC_TEST_INCS=@QFCC_TEST_INCS@

test_progs_dat=deadbool.dat infloop.dat modulo.dat structptr.dat while.dat

TESTS=$(test_progs_dat:.dat=.run)

check_PROGRAMS=test-harness $(test_progs_dat)

test_harness_SOURCES=		test-bi.c test-harness.c
test_harness_LDADD=			$(QFCC_TEST_LIBS)
test_harness_DEPENDENCIES=	$(QFCC_TEST_DEPS)

deadbool_dat_SOURCES=deadbool.r
deadbool_obj=$(deadbool_dat_SOURCES:.r=.qfo)
deadbool.dat: $(deadbool_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(deadbool_obj)
deadbool.run: Makefile build-run
	$(srcdir)/build-run $@

infloop_dat_SOURCES=infloop.r
infloop_obj=$(infloop_dat_SOURCES:.r=.qfo)
infloop.dat: $(infloop_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(infloop_obj)
infloop.run: Makefile build-run
	$(srcdir)/build-run $@

modulo_dat_SOURCES=modulo.r
modulo_obj=$(modulo_dat_SOURCES:.r=.qfo)
modulo.dat: $(modulo_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(modulo_obj)
modulo.run: Makefile build-run
	TEST_HARNESS_OPTS=--float $(srcdir)/build-run $@

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

include ./$(DEPDIR)/*.Qo

EXTRA_DIST= test-bi.h build-run
CLEANFILES= *.dat *.sym *.qfo *.run
