AUTOMAKE_OPTIONS= foreign

QFLIGHT_LIBS=@QFLIGHT_LIBS@
QFLIGHT_DEPS=@QFLIGHT_DEPS@
QFLIGHT_INCS=@QFLIGHT_INCS@
PTHREAD_LDFLAGS=@PTHREAD_LDFLAGS@
PTHREAD_CFLAGS=@PTHREAD_CFLAGS@

INCLUDES=	-I$(top_srcdir)/include $(QFLIGHT_INCS) $(PTHREAD_CFLAGS)

if BUILD_QFLIGHT
qflight=qflight
else
qflight=
endif

bin_PROGRAMS=	$(qflight)
EXTRA_PROGRAMS= qflight

qflight_SOURCES= entities.c ltface.c noise.c options.c qflight.c threads.c trace.c vis.c

qflight_LDFLAGS=	$(PTHREAD_LDFLAGS)
qflight_LDADD=		$(QFLIGHT_LIBS)
qflight_DEPENDENCIES=	$(QFLIGHT_DEPS)
