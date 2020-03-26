AUTOMAKE_OPTIONS= foreign no-exeext

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)

GZ=@progs_gz@
scheme_libs=libscheme.a
scheme_libexec=main.dat

pkglibdir=$(datarootdir)/qfcc/lib
#FIXME where to put pkglibexec?
#pkglibexecdir=$(datarootdir)/qfcc/bin

pkglib_LIBRARIES= $(scheme_libs)
EXTRA_LIBRARIES= $(scheme_libs)

noinst_PROGRAMS= $(scheme_libexec)
EXTRA_PROGRAMS = $(scheme_libexec)

QCFLAGS=-qq -O -g -Werror -Wall -Wno-integer-divide
QCPPFLAGS=--no-default-paths -I$(top_srcdir)/ruamoko/include
QCLINKFLAGS=--no-default-paths -L$(top_builddir)/ruamoko/lib
QCOMPILE=$(QFCC) $(QCFLAGS) $(QCPPFLAGS)
QLINK=$(QFCC) $(QCFLAGS) $(QCLINKFLAGS)

MKDIR_P = @MKDIR_P@
am__mv = mv -f

PAK=$(top_builddir)/tools/pak/pak$(EXEEXT)
GZIP=if echo $@ | grep -q .gz; then gzip -f `basename $@ .gz`; if test -f `basename $@ .dat.gz`.sym; then gzip -f `basename $@ .dat.gz`.sym; fi; fi
# BSD make can't handle $(shell foo) directives, and GNU make can't handle |=
# so we have to bite the bullet and pass this to the shell every time.
STRIP=`echo -n $(srcdir)/ | sed -e 's/[^/]//g' | wc -c`
RANLIB=touch

SUFFIXES=.o .r
.r.o:
	$(QCOMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tqo -p $(STRIP) -c -o $@ $<
	@sed -i -e '1s@:@: $(QFCC_DEP)@' $(DEPDIR)/$*.Tqo
	@$(am__mv) $(DEPDIR)/$*.Tqo $(DEPDIR)/$*.Qo

r_depfiles_remade=

libscheme_a_SOURCES=\
	SchemeObject.r Cons.r Number.r SchemeString.r Symbol.r Lexer.r Parser.r \
	Nil.r Procedure.r Primitive.r Lambda.r Scope.r Instruction.r builtins.r \
	Frame.r CompiledCode.r Compiler.r Continuation.r Machine.r Void.r \
	Error.r Boolean.r BaseContinuation.r
libscheme_a_obj=$(libscheme_a_SOURCES:.r=.o)
libscheme_a_dep=$(addprefix ./$(DEPDIR)/,$(libscheme_a_obj:.o=.Qo))
libscheme_a_AR=$(PAK) -cf
include $(libscheme_a_dep) # am--include-marker
r_depfiles_remade += $(libscheme_a_dep)

scheme_src=\
	main.r defs.r

main_dat_SOURCES=$(scheme_src)
main_obj=$(scheme_src:.r=.o)
main_dep=$(addprefix ./$(DEPDIR)/,$(main_obj:.o=.Qo))
main.dat: $(main_obj) $(QFCC_DEP) libscheme.a ../lib/libcsqc.a ../lib/libr.a
	$(QLINK) -p $(STRIP) -o main.dat $(main_obj) libscheme.a ../lib/libcsqc.a ../lib/libr.a
include $(main_dep) # am--include-marker
r_depfiles_remade += $(main_dep)

main.sym: main.dat

$(r_depfiles_remade):
	$(MKDIR_P) $(@D)
	echo '# dummy' >$@-t && $(am__mv) $@-t $@

am--depfiles: $(am__depfiles_remade) $(r_depfiles_remade)

EXTRA_DIST = \
		BaseContinuation.h Boolean.h CompiledCode.h Compiler.h Cons.h \
		Continuation.h Error.h Frame.h Instruction.h Lambda.h Lexer.h \
		Machine.h Nil.h Number.h Parser.h Primitive.h Procedure.h \
		SchemeObject.h SchemeString.h Scope.h Symbol.h Void.h builtins.h \
		debug.h defs.h state.h \
		\
		main.r defs.r
CLEANFILES= *.dat *.sym *.gz *.qfo *.o
