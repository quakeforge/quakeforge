AUTOMAKE_OPTIONS= foreign

QFCC_LIBS=@QFCC_LIBS@
QFCC_DEPS=@QFCC_DEPS@
QFCC_INCS=@QFCC_INCS@

AM_CPPFLAGS= -I$(top_srcdir)/include $(QFCC_INCS)

QFCC_DEP=$(builddir)/../source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)

QCFLAGS=-qq -O -g -Werror
QCPPFLAGS=--no-default-paths -I$(top_srcdir)/ruamoko/include
QCOMPILE=$(QFCC) $(QCFLAGS) $(QCPPFLAGS)

SUFFIXES=.qfo .r .pas
.r.qfo:
	$(QCOMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tqo -c -o $@ $<
	@sed -i -e '1s@:@: $(QFCC_DEP)@' $(DEPDIR)/$*.Tqo
	@$(am__mv) $(DEPDIR)/$*.Tqo $(DEPDIR)/$*.Qo
.pas.qfo:
	$(QCOMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tqo -c -o $@ $<
	@sed -i -e '1s@:@: $(QFCC_DEP)@' $(DEPDIR)/$*.Tqo
	@$(am__mv) $(DEPDIR)/$*.Tqo $(DEPDIR)/$*.Qo

QFCC_TEST_LIBS=@QFCC_TEST_LIBS@
QFCC_TEST_DEPS=@QFCC_TEST_DEPS@
QFCC_TEST_INCS=@QFCC_TEST_INCS@

test_bins=\
	test-defspace

fail_bins=

test_progs_dat=\
	address-cast.dat \
	alignment.dat \
	assignchain.dat \
	anonstruct.dat \
	chewed-alias.dat \
	chewed-return.dat \
	comma-expr.dat \
	compound.dat \
	deadbool.dat \
	double.dat \
	double-alias.dat \
	enum.dat \
	fordecl.dat \
	func-expr.dat \
	func-expr2.dat \
	func-static.dat \
	gcd.dat \
	infloop.dat \
	ivar-struct-return.dat \
	methodparams.dat \
	modulo.dat \
	nilparamret.dat \
	overload.dat \
	paramret.dat \
	quaternion.dat \
	return-ivar.dat \
	sendv.dat \
	state.dat \
	struct-init-param.dat \
	struct-nil-init.dat \
	structarray.dat \
	structlive.dat \
	structptr.dat \
	structstruct.dat \
	swap.dat \
	triangle.dat \
	typedef.dat \
	typelinker.dat \
	unaryminus.dat \
	vecaddr.dat \
	vecexpr.dat \
	vecinit.dat \
	voidfor.dat \
	while.dat \
	zerolinker.dat \
	$e

fail_progs_dat=

test_build_errors=\
	classarray.r \
	double-demote-float.r \
	double-demote-float-ainit.r \
	double-demote-float-ginit.r \
	double-demote-float-linit.r \
	double-demote-int.r \
	double-demote-int-ainit.r \
	double-demote-int-ginit.r \
	double-demote-int-linit.r \
	double-int-compare.r \
	double-float-compare.r

fail_build_errors=

test_defspace_src=\
	tw-defspace.c tw-diagnostic.c tw-strpool.c

TESTS=\
	$(test_bins) \
	$(test_progs_dat:.dat=.run) \
	$(test_build_errors:.r=.run)
XFAIL_TESTS=\
	$(fail_bins) \
	$(fail_progs_dat:.dat=.run) \
	$(fail_build_errors:.r=.run)

check_PROGRAMS=test-harness $(test_progs_dat) $(test_bins)

test_defspace_SOURCES=		test-defspace.c $(test_defspace_src)
test_defspace_LDADD=		$(QFCC_LIBS)
test_defspace_DEPENDENCIES=	$(QFCC_DEPS)

test_harness_SOURCES=		test-bi.c test-harness.c
test_harness_LDADD=			$(QFCC_TEST_LIBS)
test_harness_DEPENDENCIES=	$(QFCC_TEST_DEPS)

r_depfiles_remade=

address_cast_dat_SOURCES=address-cast.r
address_cast_obj=$(address_cast_dat_SOURCES:.r=.qfo)
address_cast_dep=$(addprefix ./$(DEPDIR)/,$(address_cast_obj:.qfo=.Qo))
address-cast.dat$(EXEEXT): $(address_cast_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(address_cast_obj)
address-cast.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(address_cast_dep) # am--include-marker
r_depfiles_remade += $(address_cast_dep)

alignment_dat_SOURCES=alignment.r
alignment_obj=$(alignment_dat_SOURCES:.r=.qfo)
alignment_dep=$(addprefix ./$(DEPDIR)/,$(alignment_obj:.qfo=.Qo))
alignment.dat$(EXEEXT): $(alignment_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(alignment_obj)
alignment.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(alignment_dep) # am--include-marker
r_depfiles_remade += $(alignment_dep)

anonstruct_dat_SOURCES=anonstruct.r
anonstruct_obj=$(anonstruct_dat_SOURCES:.r=.qfo)
anonstruct_dep=$(addprefix ./$(DEPDIR)/,$(anonstruct_obj:.qfo=.Qo))
anonstruct.dat$(EXEEXT): $(anonstruct_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(anonstruct_obj)
anonstruct.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(anonstruct_dep) # am--include-marker
r_depfiles_remade += $(anonstruct_dep)

assignchain_dat_SOURCES=assignchain.r
assignchain_obj=$(assignchain_dat_SOURCES:.r=.qfo)
assignchain_dep=$(addprefix ./$(DEPDIR)/,$(assignchain_obj:.qfo=.Qo))
assignchain.dat$(EXEEXT): $(assignchain_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(assignchain_obj)
assignchain.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(assignchain_dep) # am--include-marker
r_depfiles_remade += $(assignchain_dep)

chewed_alias_dat_SOURCES=chewed-alias.r
chewed_alias_obj=$(chewed_alias_dat_SOURCES:.r=.qfo)
chewed_alias_dep=$(addprefix ./$(DEPDIR)/,$(chewed_alias_obj:.qfo=.Qo))
chewed-alias.dat$(EXEEXT): $(chewed_alias_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(chewed_alias_obj)
chewed-alias.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(chewed_alias_dep) # am--include-marker
r_depfiles_remade += $(chewed_alias_dep)

chewed_return_dat_SOURCES=chewed-return.r
chewed_return_obj=$(chewed_return_dat_SOURCES:.r=.qfo)
chewed_return_dep=$(addprefix ./$(DEPDIR)/,$(chewed_return_obj:.qfo=.Qo))
chewed-return.dat$(EXEEXT): $(chewed_return_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(chewed_return_obj)
chewed-return.run: Makefile build-run
	@TEST_HARNESS_OPTS=--float $(srcdir)/build-run $@
include $(chewed_return_dep) # am--include-marker
r_depfiles_remade += $(chewed_return_dep)

comma_expr_dat_SOURCES=comma-expr.r
comma_expr_obj=$(comma_expr_dat_SOURCES:.r=.qfo)
comma_expr_dep=$(addprefix ./$(DEPDIR)/,$(comma_expr_obj:.qfo=.Qo))
comma-expr.dat$(EXEEXT): $(comma_expr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(comma_expr_obj)
comma-expr.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(comma_expr_dep) # am--include-marker
r_depfiles_remade += $(comma_expr_dep)

compound_dat_SOURCES=compound.r
compound_obj=$(compound_dat_SOURCES:.r=.qfo)
compound_dep=$(addprefix ./$(DEPDIR)/,$(compound_obj:.qfo=.Qo))
compound.dat$(EXEEXT): $(compound_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(compound_obj)
compound.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(compound_dep) # am--include-marker
r_depfiles_remade += $(compound_dep)

deadbool_dat_SOURCES=deadbool.r
deadbool_obj=$(deadbool_dat_SOURCES:.r=.qfo)
deadbool_dep=$(addprefix ./$(DEPDIR)/,$(deadbool_obj:.qfo=.Qo))
deadbool.dat$(EXEEXT): $(deadbool_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(deadbool_obj)
deadbool.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(deadbool_dep) # am--include-marker
r_depfiles_remade += $(deadbool_dep)

double_dat_SOURCES=double.r
double_obj=$(double_dat_SOURCES:.r=.qfo)
double_dep=$(addprefix ./$(DEPDIR)/,$(double_obj:.qfo=.Qo))
double.dat$(EXEEXT): $(double_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(double_obj)
double.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(double_dep) # am--include-marker
r_depfiles_remade += $(double_dep)

double_alias_dat_SOURCES=double-alias.r
double_alias_obj=$(double_alias_dat_SOURCES:.r=.qfo)
double_alias_dep=$(addprefix ./$(DEPDIR)/,$(double_alias_obj:.qfo=.Qo))
double-alias.dat$(EXEEXT): $(double_alias_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(double_alias_obj)
double-alias.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(double_alias_dep) # am--include-marker
r_depfiles_remade += $(double_alias_dep)

classarray.run$(EXEEXT): classarray.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

double-demote-int.run$(EXEEXT): double-demote-int.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

double-demote-float.run$(EXEEXT): double-demote-float.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

double-demote-int-ainit.run$(EXEEXT): double-demote-int-ainit.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

double-demote-float-ainit.run$(EXEEXT): double-demote-float-ainit.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

double-demote-int-ginit.run$(EXEEXT): double-demote-int-ginit.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

double-demote-float-ginit.run$(EXEEXT): double-demote-float-ginit.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

double-demote-int-linit.run$(EXEEXT): double-demote-int-linit.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

double-demote-float-linit.run$(EXEEXT): double-demote-float-linit.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

double-int-compare.run$(EXEEXT): double-int-compare.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

double-float-compare.run$(EXEEXT): double-float-compare.r Makefile build-compile-fail-run
	@$(srcdir)/build-compile-fail-run $@ $(QFCC) $(QCFLAGS) $<

enum_dat_SOURCES=enum.r
enum_obj=$(enum_dat_SOURCES:.r=.qfo)
enum_dep=$(addprefix ./$(DEPDIR)/,$(enum_obj:.qfo=.Qo))
enum.dat$(EXEEXT): $(enum_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(enum_obj)
enum.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(enum_dep) # am--include-marker
r_depfiles_remade += $(enum_dep)

fordecl_dat_SOURCES=fordecl.r
fordecl_obj=$(fordecl_dat_SOURCES:.r=.qfo)
fordecl_dep=$(addprefix ./$(DEPDIR)/,$(fordecl_obj:.qfo=.Qo))
fordecl.dat$(EXEEXT): $(fordecl_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(fordecl_obj)
fordecl.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(fordecl_dep) # am--include-marker
r_depfiles_remade += $(fordecl_dep)

func_expr_dat_SOURCES=func-expr.r
func_expr_obj=$(func_expr_dat_SOURCES:.r=.qfo)
func_expr_dep=$(addprefix ./$(DEPDIR)/,$(func_expr_obj:.qfo=.Qo))
func-expr.dat$(EXEEXT): $(func_expr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(func_expr_obj)
func-expr.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(func_expr_dep) # am--include-marker
r_depfiles_remade += $(func_expr_dep)

func_expr2_dat_SOURCES=func-expr2.r
func_expr2_obj=$(func_expr2_dat_SOURCES:.r=.qfo)
func_expr2_dep=$(addprefix ./$(DEPDIR)/,$(func_expr2_obj:.qfo=.Qo))
func-expr2.dat$(EXEEXT): $(func_expr2_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(func_expr2_obj)
func-expr2.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(func_expr2_dep) # am--include-marker
r_depfiles_remade += $(func_expr2_dep)

func_static_dat_SOURCES=func-static.r
func_static_obj=$(func_static_dat_SOURCES:.r=.qfo)
func_static_dep=$(addprefix ./$(DEPDIR)/,$(func_static_obj:.qfo=.Qo))
func-static.dat$(EXEEXT): $(func_static_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(func_static_obj)
func-static.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(func_static_dep) # am--include-marker
r_depfiles_remade += $(func_static_dep)

gcd_dat_SOURCES=gcd.pas
gcd_obj=$(gcd_dat_SOURCES:.pas=.qfo)
gcd_dep=$(addprefix ./$(DEPDIR)/,$(gcd_obj:.qfo=.Qo))
gcd.dat$(EXEEXT): $(gcd_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(gcd_obj)
gcd.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(gcd_dep) # am--include-marker
r_depfiles_remade += $(gcd_dep)

infloop_dat_SOURCES=infloop.r
infloop_obj=$(infloop_dat_SOURCES:.r=.qfo)
infloop_dep=$(addprefix ./$(DEPDIR)/,$(infloop_obj:.qfo=.Qo))
infloop.dat$(EXEEXT): $(infloop_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(infloop_obj)
infloop.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(infloop_dep) # am--include-marker
r_depfiles_remade += $(infloop_dep)

ivar_struct_return_dat_SOURCES=ivar-struct-return.r
ivar_struct_return_obj=$(ivar_struct_return_dat_SOURCES:.r=.qfo)
ivar_struct_return_dep=$(addprefix ./$(DEPDIR)/,$(ivar_struct_return_obj:.qfo=.Qo))
ivar-struct-return.dat$(EXEEXT): $(ivar_struct_return_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(ivar_struct_return_obj)
ivar-struct-return.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(ivar_struct_return_dep) # am--include-marker
r_depfiles_remade += $(ivar_struct_return_dep)

methodparams_dat_SOURCES=methodparams.r
methodparams_obj=$(methodparams_dat_SOURCES:.r=.qfo)
methodparams_dep=$(addprefix ./$(DEPDIR)/,$(methodparams_obj:.qfo=.Qo))
methodparams.dat$(EXEEXT): $(methodparams_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(methodparams_obj)
methodparams.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(methodparams_dep) # am--include-marker
r_depfiles_remade += $(methodparams_dep)

modulo_dat_SOURCES=modulo.r
modulo_obj=$(modulo_dat_SOURCES:.r=.qfo)
modulo_dep=$(addprefix ./$(DEPDIR)/,$(modulo_obj:.qfo=.Qo))
modulo.dat$(EXEEXT): $(modulo_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(modulo_obj)
modulo.run: Makefile build-run
	@TEST_HARNESS_OPTS=--float $(srcdir)/build-run $@
include $(modulo_dep) # am--include-marker
r_depfiles_remade += $(modulo_dep)

nilparamret_dat_SOURCES=nilparamret.r
nilparamret_obj=$(nilparamret_dat_SOURCES:.r=.qfo)
nilparamret_dep=$(addprefix ./$(DEPDIR)/,$(nilparamret_obj:.qfo=.Qo))
nilparamret.dat$(EXEEXT): $(nilparamret_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(nilparamret_obj)
nilparamret.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(nilparamret_dep) # am--include-marker
r_depfiles_remade += $(nilparamret_dep)

overload_dat_SOURCES=overload.r
overload_obj=$(overload_dat_SOURCES:.r=.qfo)
overload_dep=$(addprefix ./$(DEPDIR)/,$(overload_obj:.qfo=.Qo))
overload.dat$(EXEEXT): $(overload_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(overload_obj)
overload.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(overload_dep) # am--include-marker
r_depfiles_remade += $(overload_dep)

paramret_dat_SOURCES=paramret.r
paramret_obj=$(paramret_dat_SOURCES:.r=.qfo)
paramret_dep=$(addprefix ./$(DEPDIR)/,$(paramret_obj:.qfo=.Qo))
paramret.dat$(EXEEXT): $(paramret_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(paramret_obj)
paramret.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(paramret_dep) # am--include-marker
r_depfiles_remade += $(paramret_dep)

quaternion_dat_SOURCES=quaternion.r
quaternion_obj=$(quaternion_dat_SOURCES:.r=.qfo)
quaternion_dep=$(addprefix ./$(DEPDIR)/,$(quaternion_obj:.qfo=.Qo))
quaternion.dat$(EXEEXT): $(quaternion_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(quaternion_obj)
quaternion.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(quaternion_dep) # am--include-marker
r_depfiles_remade += $(quaternion_dep)

return_ivar_dat_SOURCES=return-ivar.r
return_ivar_obj=$(return_ivar_dat_SOURCES:.r=.qfo)
return_ivar_dep=$(addprefix ./$(DEPDIR)/,$(return_ivar_obj:.qfo=.Qo))
return-ivar.dat$(EXEEXT): $(return_ivar_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(return_ivar_obj)
return-ivar.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(return_ivar_dep) # am--include-marker
r_depfiles_remade += $(return_ivar_dep)

sendv_dat_SOURCES=sendv.r
sendv_obj=$(sendv_dat_SOURCES:.r=.qfo)
sendv_dep=$(addprefix ./$(DEPDIR)/,$(sendv_obj:.qfo=.Qo))
sendv.dat$(EXEEXT): $(sendv_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(sendv_obj)
sendv.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(sendv_dep) # am--include-marker
r_depfiles_remade += $(sendv_dep)

state_dat_SOURCES=state.r
state_obj=$(state_dat_SOURCES:.r=.qfo)
state_dep=$(addprefix ./$(DEPDIR)/,$(state_obj:.qfo=.Qo))
state.dat$(EXEEXT): $(state_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(state_obj)
state.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(state_dep) # am--include-marker
r_depfiles_remade += $(state_dep)

struct_init_param_dat_SOURCES=struct-init-param.r
struct_init_param_obj=$(struct_init_param_dat_SOURCES:.r=.qfo)
struct_init_param_dep=$(addprefix ./$(DEPDIR)/,$(struct_init_param_obj:.qfo=.Qo))
struct-init-param.dat$(EXEEXT): $(struct_init_param_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(struct_init_param_obj)
struct-init-param.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(struct_init_param_dep) # am--include-marker
r_depfiles_remade += $(struct_init_param_dep)

struct_nil_init_dat_SOURCES=struct-nil-init.r
struct_nil_init_obj=$(struct_nil_init_dat_SOURCES:.r=.qfo)
struct_nil_init_dep=$(addprefix ./$(DEPDIR)/,$(struct_nil_init_obj:.qfo=.Qo))
struct-nil-init.dat$(EXEEXT): $(struct_nil_init_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(struct_nil_init_obj)
struct-nil-init.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(struct_nil_init_dep) # am--include-marker
r_depfiles_remade += $(struct_nil_init_dep)

structarray_dat_SOURCES=structarray.r
structarray_obj=$(structarray_dat_SOURCES:.r=.qfo)
structarray_dep=$(addprefix ./$(DEPDIR)/,$(structarray_obj:.qfo=.Qo))
structarray.dat$(EXEEXT): $(structarray_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structarray_obj)
structarray.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(structarray_dep) # am--include-marker
r_depfiles_remade += $(structarray_dep)

structlive_dat_SOURCES=structlive.r
structlive_obj=$(structlive_dat_SOURCES:.r=.qfo)
structlive_dep=$(addprefix ./$(DEPDIR)/,$(structlive_obj:.qfo=.Qo))
structlive.dat$(EXEEXT): $(structlive_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structlive_obj)
structlive.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(structlive_dep) # am--include-marker
r_depfiles_remade += $(structlive_dep)

structptr_dat_SOURCES=structptr.r
structptr_obj=$(structptr_dat_SOURCES:.r=.qfo)
structptr_dep=$(addprefix ./$(DEPDIR)/,$(structptr_obj:.qfo=.Qo))
structptr.dat$(EXEEXT): $(structptr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structptr_obj)
structptr.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(structptr_dep) # am--include-marker
r_depfiles_remade += $(structptr_dep)

structstruct_dat_SOURCES=structstruct.r
structstruct_obj=$(structstruct_dat_SOURCES:.r=.qfo)
structstruct_dep=$(addprefix ./$(DEPDIR)/,$(structstruct_obj:.qfo=.Qo))
structstruct.dat$(EXEEXT): $(structstruct_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(structstruct_obj)
structstruct.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(structstruct_dep) # am--include-marker
r_depfiles_remade += $(structstruct_dep)

swap_dat_SOURCES=swap.r
swap_obj=$(swap_dat_SOURCES:.r=.qfo)
swap_dep=$(addprefix ./$(DEPDIR)/,$(swap_obj:.qfo=.Qo))
swap.dat$(EXEEXT): $(swap_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(swap_obj)
swap.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(swap_dep) # am--include-marker
r_depfiles_remade += $(swap_dep)

triangle_dat_SOURCES=triangle.r
triangle_obj=$(triangle_dat_SOURCES:.r=.qfo)
triangle_dep=$(addprefix ./$(DEPDIR)/,$(triangle_obj:.qfo=.Qo))
triangle.dat$(EXEEXT): $(triangle_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(triangle_obj)
triangle.run: Makefile build-run
	@$(srcdir)/build-run $@ 100000 100000 1.00005 50002.4961
include $(triangle_dep) # am--include-marker
r_depfiles_remade += $(triangle_dep)

typedef_dat_SOURCES=typedef.r
typedef_obj=$(typedef_dat_SOURCES:.r=.qfo)
typedef_dep=$(addprefix ./$(DEPDIR)/,$(typedef_obj:.qfo=.Qo))
typedef.dat$(EXEEXT): $(typedef_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(typedef_obj)
typedef.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(typedef_dep) # am--include-marker
r_depfiles_remade += $(typedef_dep)

typelinker_dat_SOURCES=typelinker_a.r typelinker_b.r
typelinker_obj=$(typelinker_dat_SOURCES:.r=.qfo)
typelinker_dep=$(addprefix ./$(DEPDIR)/,$(typelinker_obj:.qfo=.Qo))
typelinker.dat$(EXEEXT): $(typelinker_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(typelinker_obj)
typelinker.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(typelinker_dep) # am--include-marker
r_depfiles_remade += $(typelinker_dep)

unaryminus_dat_SOURCES=unaryminus.r
unaryminus_obj=$(unaryminus_dat_SOURCES:.r=.qfo)
unaryminus_dep=$(addprefix ./$(DEPDIR)/,$(unaryminus_obj:.qfo=.Qo))
unaryminus.dat$(EXEEXT): $(unaryminus_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(unaryminus_obj)
unaryminus.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(unaryminus_dep) # am--include-marker
r_depfiles_remade += $(unaryminus_dep)

vecaddr_dat_SOURCES=vecaddr.r
vecaddr_obj=$(vecaddr_dat_SOURCES:.r=.qfo)
vecaddr_dep=$(addprefix ./$(DEPDIR)/,$(vecaddr_obj:.qfo=.Qo))
vecaddr.dat$(EXEEXT): $(vecaddr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(vecaddr_obj)
vecaddr.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(vecaddr_dep) # am--include-marker
r_depfiles_remade += $(vecaddr_dep)

vecexpr_dat_SOURCES=vecexpr.r
vecexpr_obj=$(vecexpr_dat_SOURCES:.r=.qfo)
vecexpr_dep=$(addprefix ./$(DEPDIR)/,$(vecexpr_obj:.qfo=.Qo))
vecexpr.dat$(EXEEXT): $(vecexpr_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(vecexpr_obj)
vecexpr.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(vecexpr_dep) # am--include-marker
r_depfiles_remade += $(vecexpr_dep)

vecinit_dat_SOURCES=vecinit.r
vecinit_obj=$(vecinit_dat_SOURCES:.r=.qfo)
vecinit_dep=$(addprefix ./$(DEPDIR)/,$(vecinit_obj:.qfo=.Qo))
vecinit.dat$(EXEEXT): $(vecinit_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(vecinit_obj)
vecinit.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(vecinit_dep) # am--include-marker
r_depfiles_remade += $(vecinit_dep)

voidfor_dat_SOURCES=voidfor.r
voidfor_obj=$(voidfor_dat_SOURCES:.r=.qfo)
voidfor_dep=$(addprefix ./$(DEPDIR)/,$(voidfor_obj:.qfo=.Qo))
voidfor.dat$(EXEEXT): $(voidfor_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(voidfor_obj)
voidfor.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(voidfor_dep) # am--include-marker
r_depfiles_remade += $(voidfor_dep)

while_dat_SOURCES=while.r
while_obj=$(while_dat_SOURCES:.r=.qfo)
while_dep=$(addprefix ./$(DEPDIR)/,$(while_obj:.qfo=.Qo))
while.dat$(EXEEXT): $(while_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(while_obj)
while.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(while_dep) # am--include-marker
r_depfiles_remade += $(while_dep)

zerolinker_dat_SOURCES=zerolinker.r
zerolinker_obj=$(zerolinker_dat_SOURCES:.r=.qfo)
zerolinker_dep=$(addprefix ./$(DEPDIR)/,$(zerolinker_obj:.qfo=.Qo))
zerolinker.dat$(EXEEXT): $(zerolinker_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -o $@ $(zerolinker_obj)
zerolinker.run: Makefile build-run
	@$(srcdir)/build-run $@
include $(zerolinker_dep) # am--include-marker
r_depfiles_remade += $(zerolinker_dep)

$(r_depfiles_remade):
	@$(MKDIR_P) $(@D)
	@echo '# dummy' >$@-t && $(am__mv) $@-t $@


am--depfiles: $(am__depfiles_remade) $(r_depfiles_remade)

EXTRA_DIST= test-bi.h build-run test-defspace.h
CLEANFILES= *.dat *.sym *.qfo *.run *.frame *.log *.trs
