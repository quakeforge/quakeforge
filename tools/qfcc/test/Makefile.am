AUTOMAKE_OPTIONS= foreign
INCLUDES= -I$(top_srcdir)/include $(QFCC_INCS)

QFCC_DEP=$(builddir)/../source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)

QCFLAGS=-qq -O -g --no-default-paths -Werror
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

test_progs_dat=\
	chewed-alias.dat \
	chewed-return.dat \
	func-expr.dat \
	func-static.dat \
	deadbool.dat \
	infloop.dat \
	modulo.dat \
	paramret.dat \
	return-ivar.dat \
	sendv.dat \
	state.dat \
	structlive.dat \
	structptr.dat \
	vecinit.dat \
	while.dat \
	voidfor.dat

fail_progs_dat=\
	$E

TESTS=$(test_progs_dat:.dat=.run)
XFAIL_TESTS=$(fail_progs_dat:.dat=.run)

check_PROGRAMS=test-harness $(test_progs_dat)

test_harness_SOURCES=		test-bi.c test-harness.c
test_harness_LDADD=			$(QFCC_TEST_LIBS)
test_harness_DEPENDENCIES=	$(QFCC_TEST_DEPS)

chewed_alias_dat_SOURCES=chewed-alias.r
chewed_alias_obj=$(chewed_alias_dat_SOURCES:.r=.qfo)
chewed-alias.dat: $(chewed_alias_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(chewed_alias_obj)
chewed-alias.run: Makefile build-run
	$(srcdir)/build-run $@

chewed_return_dat_SOURCES=chewed-return.r
chewed_return_obj=$(chewed_return_dat_SOURCES:.r=.qfo)
chewed-return.dat: $(chewed_return_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(chewed_return_obj)
chewed-return.run: Makefile build-run
	TEST_HARNESS_OPTS=--float $(srcdir)/build-run $@

func_expr_dat_SOURCES=func-expr.r
func_expr_obj=$(func_expr_dat_SOURCES:.r=.qfo)
func-expr.dat: $(func_expr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(func_expr_obj)
func-expr.run: Makefile build-run
	$(srcdir)/build-run $@

func_static_dat_SOURCES=func-static.r
func_static_obj=$(func_static_dat_SOURCES:.r=.qfo)
func-static.dat: $(func_static_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(func_static_obj)
func-static.run: Makefile build-run
	$(srcdir)/build-run $@

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

paramret_dat_SOURCES=paramret.r
paramret_obj=$(paramret_dat_SOURCES:.r=.qfo)
paramret.dat: $(paramret_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(paramret_obj)
paramret.run: Makefile build-run
	TEST_HARNESS_OPTS=--float $(srcdir)/build-run $@

return_ivar_dat_SOURCES=return-ivar.r
return_ivar_obj=$(return_ivar_dat_SOURCES:.r=.qfo)
return-ivar.dat: $(return_ivar_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(return_ivar_obj)
return-ivar.run: Makefile build-run
	$(srcdir)/build-run $@

sendv_dat_SOURCES=sendv.r
sendv_obj=$(sendv_dat_SOURCES:.r=.qfo)
sendv.dat: $(sendv_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(sendv_obj)
sendv.run: Makefile build-run
	$(srcdir)/build-run $@

state_dat_SOURCES=state.r
state_obj=$(state_dat_SOURCES:.r=.qfo)
state.dat: $(state_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(state_obj)
state.run: Makefile build-run
	$(srcdir)/build-run $@

structlive_dat_SOURCES=structlive.r
structlive_obj=$(structlive_dat_SOURCES:.r=.qfo)
structlive.dat: $(structlive_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structlive_obj)
structlive.run: Makefile build-run
	$(srcdir)/build-run $@

structptr_dat_SOURCES=structptr.r
structptr_obj=$(structptr_dat_SOURCES:.r=.qfo)
structptr.dat: $(structptr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structptr_obj)
structptr.run: Makefile build-run
	$(srcdir)/build-run $@

vecinit_dat_SOURCES=vecinit.r
vecinit_obj=$(vecinit_dat_SOURCES:.r=.qfo)
vecinit.dat: $(vecinit_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(vecinit_obj)
vecinit.run: Makefile build-run
	$(srcdir)/build-run $@

while_dat_SOURCES=while.r
while_obj=$(while_dat_SOURCES:.r=.qfo)
while.dat: $(while_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(while_obj)
while.run: Makefile build-run
	$(srcdir)/build-run $@

voidfor_dat_SOURCES=voidfor.r
voidfor_obj=$(voidfor_dat_SOURCES:.r=.qfo)
voidfor.dat: $(voidfor_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(voidfor_obj)
voidfor.run: Makefile build-run
	$(srcdir)/build-run $@

include ./$(DEPDIR)/*.Qo

EXTRA_DIST= test-bi.h build-run
CLEANFILES= *.dat *.sym *.qfo *.run
