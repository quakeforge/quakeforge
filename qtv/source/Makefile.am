## Process this file with automake to produce Makefile.in
#
#	Makefile.am
#
#	Automake-using build system for QuakeForge
#
#	Copyright (C) 2000 Jeff Teunissen <deek@quakeforge.net>
#
#	This Makefile is free software; you can redistribute it and/or
#	modify it under the terms of the GNU General Public License
#	as published by the Free Software Foundation; either version 2
#	of the License, or (at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#	See the GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program; if not, write to:
#
#		Free Software Foundation, Inc.
#		59 Temple Place - Suite 330
#		Boston, MA  02111-1307, USA
#
#	$Id$
#

AUTOMAKE_OPTIONS=	foreign

INCLUDES=	-I$(top_srcdir)/include -I$(top_srcdir)/qtv/include

bin_PROGRAMS=	@QTV_TARGETS@

EXTRA_PROGRAMS= qtv

common_ldflags= -export-dynamic

qtv_LIBS= \
		$(SERVER_PLUGIN_STATIC_LIBS) \
		$(top_builddir)/libs/net/libnet_chan.la \
		$(top_builddir)/libs/console/libQFconsole.la \
		$(top_builddir)/libs/util/libQFutil.la

qtv_SOURCES=	connection.c qtv.c server.c
qtv_LDADD=	$(qtv_LIBS) $(NET_LIBS) $(DL_LIBS) $(CURSES_LIBS)
qtv_LDFLAGS=	$(common_ldflags)
qtv_DEPENDENCIES=	$(qtv_LIBS)

# Kill the temp files, hopefully.
CLEANFILES	= *.i *.s
