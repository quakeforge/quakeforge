AUTOMAKE_OPTIONS= foreign

QFMODELGEN_LIBS=@QFMODELGEN_LIBS@
QFMODELGEN_DEPS=@QFMODELGEN_DEPS@
QFMODELGEN_INCS=@QFMODELGEN_INCS@

INCLUDES=	-I$(top_srcdir)/include $(QFMODELGEN_INCS)

if BUILD_QFMODELGEN
qfmodelgen=qfmodelgen
else
qfmodelgen=
endif

bin_PROGRAMS=	$(qfmodelgen)
EXTRA_PROGRAMS= qfmodelgen

qfmodelgen_SOURCES= lbmlib.c modelgen.c trilib.c

qfmodelgen_LDADD=		$(QFMODELGEN_LIBS)
qfmodelgen_DEPENDENCIES=	$(QFMODELGEN_DEPS)
