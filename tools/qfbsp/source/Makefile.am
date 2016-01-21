AUTOMAKE_OPTIONS= foreign

QFBSP_LIBS=@QFBSP_LIBS@
QFBSP_DEPS=@QFBSP_DEPS@
QFBSP_INCS=@QFBSP_INCS@

AM_CPPFLAGS=	-I$(top_srcdir)/include $(QFBSP_INCS)

bin_PROGRAMS=	qfbsp

qfbsp_SOURCES= \
		brush.c csg4.c map.c merge.c nodraw.c options.c outside.c portals.c \
		qfbsp.c readbsp.c region.c solidbsp.c surfaces.c tjunc.c writebsp.c

qfbsp_LDADD=		$(QFBSP_LIBS)
qfbsp_DEPENDENCIES=	$(QFBSP_DEPS)
