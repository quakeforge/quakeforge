AUTOMAKE_OPTIONS= foreign

QFLIGHT_LIBS=@QFLIGHT_LIBS@
QFLIGHT_DEPS=@QFLIGHT_DEPS@
QFLIGHT_INCS=@QFLIGHT_INCS@

INCLUDES=	-I$(top_srcdir)/include $(QFLIGHT_INCS)

if BUILD_QFLIGHT
qflight=qflight
else
qflight=
endif

bin_PROGRAMS=	$(qflight)
EXTRA_PROGRAMS= qflight

qflight_SOURCES= entities.c ltface.c options.c qflight.c threads.c trace.c

qflight_LDADD=		$(QFLIGHT_LIBS)
qflight_DEPENDENCIES=	$(QFLIGHT_DEPS)
