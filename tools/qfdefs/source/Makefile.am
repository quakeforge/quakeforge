AUTOMAKE_OPTIONS= foreign

QFDEFS_LIBS=@QFDEFS_LIBS@
QFDEFS_DEPS=@QFDEFS_DEPS@
QFDEFS_INCS=@QFDEFS_INCS@

INCLUDES=   -I$(top_srcdir)/include $(QFDEFS_INCS)
GENDEFS=./gendefs

bin_PROGRAMS=	qfdefs
noinst_PROGRAMS=	gendefs

qfdefs_SOURCES=		defs.c defs_nq.c defs_qw.c fix_globals.c qfdefs.c
qfdefs_LDADD=		$(QFDEFS_LIBS)
qfdefs_DEPENDENCIES=$(QFDEFS_DEPS)

gendefs_SOURCES= gendefs.c

defs_nq.c: $(srcdir)/../include/progdefs.nq gendefs
	$(GENDEFS) nq $(srcdir)/../include/progdefs.nq defs_nq.c

defs_qw.c: $(srcdir)/../include/progdefs.qw gendefs
	$(GENDEFS) qw $(srcdir)/../include/progdefs.qw defs_qw.c

CLEANFILES=	defs_nq.c defs_qw.c
