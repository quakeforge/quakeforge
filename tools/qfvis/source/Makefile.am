AUTOMAKE_OPTIONS= foreign

QFVIS_LIBS=@QFVIS_LIBS@
QFVIS_DEPS=@QFVIS_DEPS@
QFVIS_INCS=@QFVIS_INCS@

INCLUDES=	-I$(top_srcdir)/include $(QFVIS_INCS)

bin_PROGRAMS=	qfvis

qfvis_SOURCES=	flow.c options.c qfvis.c soundphs.c

qfvis_LDADD=		$(QFVIS_LIBS)
qfvis_DEPENDENCIES=	$(QFVIS_DEPS)
