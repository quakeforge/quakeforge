QFCC=$(top_builddir)/tools/qfcc/source/qfcc
QCFLAGS=-g -Werror
INCLUDES= -I$(top_srcdir)/include

noinst_PROGRAMS=qwaq
noinst_DATA=qwaq.dat

qwaq_libs= \
	$(top_builddir)/libs/gamecode/engine/libQFgamecode.la \
	$(top_builddir)/libs/util/libQFutil.la

qwaq_dat_src= \
	main.qc $(top_srcdir)/cs-code/object.r test.r

qwaq_SOURCES= main.c builtins.c
qwaq_LDADD= $(qwaq_libs)
qwaq_DEPENDENCIES= $(qwaq_libs)

qwaq.dat: progs.src $(qwaq_dat_src)
	$(QFCC) $(QCFLAGS)

EXTRA_DIST=progs.src
CLEANFILES= *.dat *.sym
