AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include

noinst_LTLIBRARIES= libQFclient.la

libQFclient_la_LDFLAGS=	@STATIC@
libQFclient_la_LIBADD=	$(top_builddir)/libs/gamecode/libQFgamecode.la $(top_builddir)/libs/util/libQFutil.la
libQFclient_la_SOURCES= \
		cl_entities.c
