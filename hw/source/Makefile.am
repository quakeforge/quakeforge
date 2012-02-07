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

# Stuff that is common to both client and server
INCLUDES=	-I$(top_srcdir)/include -I$(top_srcdir)/hw/include
SDL_LIBS = @SDL_LIBS@

bin_PROGRAMS=	@HW_TARGETS@

EXTRA_PROGRAMS= hw-master

common_ldflags= -export-dynamic
hw_master_libs= \
	$(top_builddir)/libs/net/libnet_chan.la \
	$(SERVER_PLUGIN_STATIC_LIBS) \
	$(top_builddir)/libs/console/libQFconsole.la \
	$(top_builddir)/libs/util/libQFutil.la

hw_master_LDFLAGS=		$(common_ldflags)
hw_master_LDADD=		$(hw_master_libs) $(NET_LIBS)
hw_master_DEPENDENCIES=	$(hw_master_libs)
hw_master_SOURCES=		master.c

# Kill the temp files, hopefully.
CLEANFILES	= *.i *.s
