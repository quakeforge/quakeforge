AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_NON_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	libnc.la

ipv6_src= net_udp6.c
ipv4_src= net_udp.c
if NETTYPE_IPV6
ipvX_src= $(ipv6_src)
else
ipvX_src= $(ipv4_src)
endif
libnc_la_SOURCES= $(ipvX_src)
libnc_la_LDFLAGS= @STATIC@
EXTRA_libnc_la_SOURCES= $(ipv4_src) $(ipv6_src)
