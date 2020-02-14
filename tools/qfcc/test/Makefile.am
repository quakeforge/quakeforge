AUTOMAKE_OPTIONS= foreign

QFCC_LIBS=@QFCC_LIBS@
QFCC_DEPS=@QFCC_DEPS@
QFCC_INCS=@QFCC_INCS@

AM_CPPFLAGS= -I$(top_srcdir)/include $(QFCC_INCS)

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

test_bins=\
	test-defspace

fail_bins=

test_progs_dat=\
	chewed-alias.dat \
	chewed-return.dat \
	comma-expr.dat \
	deadbool.dat \
	double.dat \
	enum.dat \
	fordecl.dat \
	func-expr.dat \
	func-static.dat \
	infloop.dat \
	modulo.dat \
	paramret.dat \
	quaternion.dat \
	return-ivar.dat \
	sendv.dat \
	state.dat \
	structarray.dat \
	structlive.dat \
	structptr.dat \
	structstruct.dat \
	swap.dat \
	triangle.dat \
	vecexpr.dat \
	vecinit.dat \
	voidfor.dat \
	while.dat

fail_progs_dat=

test_defspace_src=\
	tw-defspace.c tw-diagnostic.c tw-strpool.c

TESTS=$(test_bins) $(test_progs_dat:.dat=.run)
XFAIL_TESTS=$(fail_bins) $(fail_progs_dat:.dat=.run)

check_PROGRAMS=test-harness $(test_progs_dat) $(test_bins)

test_defspace_SOURCES=		test-defspace.c $(test_defspace_src)
test_defspace_LDADD=		$(QFCC_LIBS)
test_defspace_DEPENDENCIES=	$(QFCC_DEPS)

test_harness_SOURCES=		test-bi.c test-harness.c
test_harness_LDADD=			$(QFCC_TEST_LIBS)
test_harness_DEPENDENCIES=	$(QFCC_TEST_DEPS)

r_depfiles_remade=

chewed_alias_dat_SOURCES=chewed-alias.r
chewed_alias_obj=$(chewed_alias_dat_SOURCES:.r=.qfo)
chewed-alias.dat$(EXEEXT): $(chewed_alias_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(chewed_alias_obj)
chewed-alias.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/chewed-alias.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/chewed-alias.Qo

chewed_return_dat_SOURCES=chewed-return.r
chewed_return_obj=$(chewed_return_dat_SOURCES:.r=.qfo)
chewed-return.dat$(EXEEXT): $(chewed_return_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(chewed_return_obj)
chewed-return.run: Makefile build-run
	TEST_HARNESS_OPTS=--float $(srcdir)/build-run $@
include ./$(DEPDIR)/chewed-return.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/chewed-return.Qo

comma_expr_dat_SOURCES=comma-expr.r
comma_expr_obj=$(comma_expr_dat_SOURCES:.r=.qfo)
comma-expr.dat$(EXEEXT): $(comma_expr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(comma_expr_obj)
comma-expr.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/comma-expr.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/comma-expr.Qo

deadbool_dat_SOURCES=deadbool.r
deadbool_obj=$(deadbool_dat_SOURCES:.r=.qfo)
deadbool.dat$(EXEEXT): $(deadbool_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(deadbool_obj)
deadbool.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/deadbool.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/deadbool.Qo

double_dat_SOURCES=double.r
double_obj=$(double_dat_SOURCES:.r=.qfo)
double.dat$(EXEEXT): $(double_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(double_obj)
double.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/double.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/double.Qo

enum_dat_SOURCES=enum.r
enum_obj=$(enum_dat_SOURCES:.r=.qfo)
enum.dat$(EXEEXT): $(enum_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(enum_obj)
enum.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/enum.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/enum.Qo

fordecl_dat_SOURCES=fordecl.r
fordecl_obj=$(fordecl_dat_SOURCES:.r=.qfo)
fordecl.dat$(EXEEXT): $(fordecl_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(fordecl_obj)
fordecl.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/fordecl.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/fordecl.Qo

func_expr_dat_SOURCES=func-expr.r
func_expr_obj=$(func_expr_dat_SOURCES:.r=.qfo)
func-expr.dat$(EXEEXT): $(func_expr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(func_expr_obj)
func-expr.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/func-expr.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/func-expr.Qo

func_static_dat_SOURCES=func-static.r
func_static_obj=$(func_static_dat_SOURCES:.r=.qfo)
func-static.dat$(EXEEXT): $(func_static_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(func_static_obj)
func-static.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/func-static.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/func-static.Qo

infloop_dat_SOURCES=infloop.r
infloop_obj=$(infloop_dat_SOURCES:.r=.qfo)
infloop.dat$(EXEEXT): $(infloop_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(infloop_obj)
infloop.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/infloop.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/infloop.Qo

modulo_dat_SOURCES=modulo.r
modulo_obj=$(modulo_dat_SOURCES:.r=.qfo)
modulo.dat$(EXEEXT): $(modulo_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(modulo_obj)
modulo.run: Makefile build-run
	TEST_HARNESS_OPTS=--float $(srcdir)/build-run $@
include ./$(DEPDIR)/modulo.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/modulo.Qo

paramret_dat_SOURCES=paramret.r
paramret_obj=$(paramret_dat_SOURCES:.r=.qfo)
paramret.dat$(EXEEXT): $(paramret_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(paramret_obj)
paramret.run: Makefile build-run
	TEST_HARNESS_OPTS=--float $(srcdir)/build-run $@
include ./$(DEPDIR)/paramret.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/paramret.Qo

quaternion_dat_SOURCES=quaternion.r
quaternion_obj=$(quaternion_dat_SOURCES:.r=.qfo)
quaternion.dat$(EXEEXT): $(quaternion_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(quaternion_obj)
quaternion.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/quaternion.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/quaternion.Qo

return_ivar_dat_SOURCES=return-ivar.r
return_ivar_obj=$(return_ivar_dat_SOURCES:.r=.qfo)
return-ivar.dat$(EXEEXT): $(return_ivar_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(return_ivar_obj)
return-ivar.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/return-ivar.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/return-ivar.Qo

sendv_dat_SOURCES=sendv.r
sendv_obj=$(sendv_dat_SOURCES:.r=.qfo)
sendv.dat$(EXEEXT): $(sendv_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(sendv_obj)
sendv.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/sendv.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/sendv.Qo

state_dat_SOURCES=state.r
state_obj=$(state_dat_SOURCES:.r=.qfo)
state.dat$(EXEEXT): $(state_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(state_obj)
state.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/state.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/state.Qo

structarray_dat_SOURCES=structarray.r
structarray_obj=$(structarray_dat_SOURCES:.r=.qfo)
structarray.dat$(EXEEXT): $(structarray_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structarray_obj)
structarray.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/structarray.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/structarray.Qo

structlive_dat_SOURCES=structlive.r
structlive_obj=$(structlive_dat_SOURCES:.r=.qfo)
structlive.dat$(EXEEXT): $(structlive_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structlive_obj)
structlive.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/structlive.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/structlive.Qo

structptr_dat_SOURCES=structptr.r
structptr_obj=$(structptr_dat_SOURCES:.r=.qfo)
structptr.dat$(EXEEXT): $(structptr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structptr_obj)
structptr.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/structptr.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/structptr.Qo

structstruct_dat_SOURCES=structstruct.r
structstruct_obj=$(structstruct_dat_SOURCES:.r=.qfo)
structstruct.dat$(EXEEXT): $(structstruct_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structstruct_obj)
structstruct.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/structstruct.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/structstruct.Qo

swap_dat_SOURCES=swap.r
swap_obj=$(swap_dat_SOURCES:.r=.qfo)
swap.dat$(EXEEXT): $(swap_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(swap_obj)
swap.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/swap.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/swap.Qo

triangle_dat_SOURCES=triangle.r
triangle_obj=$(triangle_dat_SOURCES:.r=.qfo)
triangle.dat$(EXEEXT): $(triangle_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(triangle_obj)
triangle.run: Makefile build-run
	$(srcdir)/build-run $@ 100000 100000 1.00005 50002.4961
include ./$(DEPDIR)/triangle.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/triangle.Qo

vecexpr_dat_SOURCES=vecexpr.r
vecexpr_obj=$(vecexpr_dat_SOURCES:.r=.qfo)
vecexpr.dat$(EXEEXT): $(vecexpr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(vecexpr_obj)
vecexpr.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/vecexpr.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/vecexpr.Qo

vecinit_dat_SOURCES=vecinit.r
vecinit_obj=$(vecinit_dat_SOURCES:.r=.qfo)
vecinit.dat$(EXEEXT): $(vecinit_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(vecinit_obj)
vecinit.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/vecinit.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/vecinit.Qo

voidfor_dat_SOURCES=voidfor.r
voidfor_obj=$(voidfor_dat_SOURCES:.r=.qfo)
voidfor.dat$(EXEEXT): $(voidfor_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(voidfor_obj)
voidfor.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/voidfor.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/voidfor.Qo

while_dat_SOURCES=while.r
while_obj=$(while_dat_SOURCES:.r=.qfo)
while.dat$(EXEEXT): $(while_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(while_obj)
while.run: Makefile build-run
	$(srcdir)/build-run $@
include ./$(DEPDIR)/while.Qo # am--include-marker
r_depfiles_remade += ./$(DEPDIR)/while.Qo

$(r_depfiles_remade):
	@$(MKDIR_P) $(@D)
	@echo '# dummy' >$@-t && $(am__mv) $@-t $@


am--depfiles: $(am__depfiles_remade) $(r_depfiles_remade)

EXTRA_DIST= test-bi.h build-run test-defspace.h
CLEANFILES= *.dat *.sym *.qfo *.run *.frame
