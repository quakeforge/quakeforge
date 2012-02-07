AUTOMAKE_OPTIONS= foreign

SUBDIRS= nc nm
AM_CFLAGS= @PREFER_NON_PIC@
INCLUDES= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	libnet_chan.la libnet_main.la

nc_libs=nc/libnc.la
libnet_chan_la_LDFLAGS=		@STATIC@
libnet_chan_la_LIBADD=		$(nc_libs)
libnet_chan_la_DEPENDENCIES=$(nc_libs)
libnet_chan_la_SOURCES= 	net_chan.c

nm_libs=nm/libnm.la
libnet_main_la_LDFLAGS=		@STATIC@
libnet_main_la_LIBADD=		$(nm_libs)
libnet_main_la_DEPENDENCIES=$(nm_libs)
libnet_main_la_SOURCES=		net_main.c
