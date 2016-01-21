AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_NON_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	libnm.la

if SYSTYPE_WIN32
net_sources= net_win.c net_wins.c
else
net_sources= net_bsd.c net_udp.c
endif
libnm_la_SOURCES= net_dgrm.c net_loop.c net_vcr.c $(net_sources)
libnm_la_LDFLAGS= @STATIC@


EXTRA_libnm_la_SOURCES= \
		net_bsd.c net_win.c net_wins.c net_udp.c
