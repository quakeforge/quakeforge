QWAQ_LIBS=@QWAQ_LIBS@
QWAQ_DEPS=@QWAQ_DEPS@
QWAQ_INCS=@QWAQ_INCS@

INCLUDES=	-I$(top_srcdir)/include $(QWAQ_INCS)

QFCC=$(top_builddir)/tools/qfcc/source/qfcc
QCFLAGS=-qq -g -Werror

noinst_PROGRAMS=qwaq
noinst_DATA=qwaq.dat

qwaq_dat_src= \
	defs.qc main.qc test.r

qwaq_SOURCES= main.c builtins.c
qwaq_LDADD= $(QWAQ_LIBS)
qwaq_DEPENDENCIES= $(QWAQ_DEPS)

qwaq.dat: progs.src $(qwaq_dat_src) $(top_srcdir)/ruamoko/lib/object.r
	$(QFCC) $(QCFLAGS) -I$(top_srcdir)/ruamoko/include

EXTRA_DIST=$(qwaq_dat_src)
CLEANFILES= *.dat *.sym
