AUTOMAKE_OPTIONS= foreign

INCLUDES=   -I$(top_srcdir)/include

bin_PROGRAMS=	qfdefs
noinst_PROGRAMS=	gendefs

qfdefs_SOURCES= qfdefs.c defs_nq.c defs_qw.c
qfdefs_LDADD=	-lQFgamecode -lQFutil

gendefs_SOURCES= gendefs.c

defs_nq.c: $(top_srcdir)/include/progdefs.nq gendefs
	./gendefs nq $(top_srcdir)/include/progdefs.nq defs_nq.c

defs_qw.c: $(top_srcdir)/include/progdefs.qw gendefs
	./gendefs qw $(top_srcdir)/include/progdefs.qw defs_qw.c

CLEANFILES=	defs_nq.c defs_qw.c
