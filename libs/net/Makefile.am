AUTOMAKE_OPTIONS= foreign

SUBDIRS= nc nm
AM_CFLAGS= @PREFER_NON_PIC@
INCLUDES= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	libnet_chan.la libnet_main.la

libnet_chan_la_LIBADD=		nc/libnc.la
libnet_chan_la_LDFLAGS=		@STATIC@
libnet_chan_la_SOURCES= 	net_chan.c
libnet_chan_la_DEPENDENCIES=	nc/libnc.la

libnet_main_la_LIBADD=		nm/libnm.la
libnet_main_la_LDFLAGS=		@STATIC@
libnet_main_la_SOURCES=		net_main.c
libnet_main_la_DEPENDENCIES=	nm/libnm.la
