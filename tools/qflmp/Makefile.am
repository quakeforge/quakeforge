AUTOMAKE_OPTIONS= foreign

QFLMP_LIBS=@QFLMP_LIBS@
QFLMP_DEPS=@QFLMP_DEPS@
QFLMP_INCS=@QFLMP_INCS@

INCLUDES= -I$(top_srcdir)/include $(QFLMP_INCS)

bin_PROGRAMS=	$(qflmp)

#mans=qflmp.1
man_MANS=	$(mans)

qflmp_SOURCES=	lmp.c
qflmp_LDADD=	$(QFLMP_LIBS)
qflmp_DEPENDENCIES= $(QFLMP_DEPS)

EXTRA_DIST=		lmp.h #lmp.1
