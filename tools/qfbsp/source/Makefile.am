AUTOMAKE_OPTIONS= foreign

QFBSP_LIBS=@QFBSP_LIBS@
QFBSP_DEPS=@QFBSP_DEPS@
QFBSP_INCS=@QFBSP_INCS@
PTHREAD_FLAGS=@PTHREAD_FLAGS@

INCLUDES=	-I$(top_srcdir)/include $(QFBSP_INCS)

if BUILD_QFBSP
qfbsp=qfbsp
else
qfbsp=
endif

bin_PROGRAMS=	$(qfbsp)
EXTRA_PROGRAMS= qfbsp

qfbsp_SOURCES= \
		brush.c csg4.c map.c merge.c nodraw.c outside.c portals.c qfbsp.c \
		region.c solidbsp.c surfaces.c tjunc.c writebsp.c

qfbsp_LDFLAGS=		$(PTHREAD_FLAGS)
qfbsp_LDADD=		$(QFBSP_LIBS)
qfbsp_DEPENDENCIES=	$(QFBSP_DEPS)
