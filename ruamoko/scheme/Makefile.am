AUTOMAKE_OPTIONS=	foreign

pkglibdir=$(libdir)/qfcc/lib

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)
QCFLAGS=-qq -g -Werror -Wall -Wno-integer-divide --no-default-paths
QCPPFLAGS=$(INCLUDES)
PAK=$(top_builddir)/tools/pak/pak$(EXEEXT)
GZIP=if echo $@ | grep -q .gz; then gzip -f `basename $@ .gz`; if test -f `basename $@ .dat.gz`.sym; then gzip -f `basename $@ .dat.gz`.sym; fi; fi
if HAVE_ZLIB
GZ=.gz
else
GZ=
endif
# BSD make can't handle $(shell foo) directives, and GNU make can't handle |=
# so we have to bite the bullet and pass this to the shell every time.
STRIP=`echo -n $(srcdir)/ | sed -e 's/[^/]//g' | wc -c`

RANLIB=touch

INCLUDES=	-I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include

scheme_libs=libscheme.a
if BUILD_RUAMOKO
libs=$(scheme_libs)
data=$(scheme_data)
else
libs=
endif

pkglib_LIBRARIES= $(libs)
EXTRA_LIBRARIES= $(scheme_libs)
#pkgdata_DATA= $(data)
EXTRA_DATA = $(scheme_data)

EXTRA_DIST = \
		BaseContinuation.h Boolean.h CompiledCode.h Compiler.h Cons.h \
		Continuation.h Error.h Frame.h Instruction.h Lambda.h Lexer.h \
		Machine.h Nil.h Number.h Parser.h Primitive.h Procedure.h \
		SchemeObject.h SchemeString.h Scope.h Symbol.h Void.h builtins.h \
		debug.h defs.h state.h \
		\
		main.r defs.r

SUFFIXES=.qc .qfo .r
.r.qfo:
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -p $(STRIP) -c -o $@ $<
.r.o:
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -p $(STRIP) -c -o $@ $<

libscheme_a_SOURCES=\
	SchemeObject.r Cons.r Number.r SchemeString.r Symbol.r Lexer.r Parser.r \
	Nil.r Procedure.r Primitive.r Lambda.r Scope.r Instruction.r builtins.r \
	Frame.r CompiledCode.r Compiler.r Continuation.r Machine.r Void.r \
	Error.r Boolean.r BaseContinuation.r
libscheme_a_AR=$(PAK) -cf

scheme_data=\
	main.dat$(GZ)

scheme_src=\
	main.r defs.r

scheme_obj=$(scheme_src:.qc=.o)

main.dat$(GZ): $(scheme_obj) $(QFCC_DEP) ../lib/libcsqc.a ../lib/libr.a libscheme.a
	$(QFCC) $(QCFLAGS) -p $(STRIP) -o main.dat $(scheme_obj) libscheme.a ../lib/libcsqc.a ../lib/libr.a
	$(GZIP)

CLEANFILES= *.dat *.sym *.gz *.qfo *.o
