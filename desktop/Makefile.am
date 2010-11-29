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
# Desktop files
if HAVE_XDG
desktopdir=$(datarootdir)/applications
desktop_DATA= @NQ_DESKTOP_DATA@ @QW_DESKTOP_DATA@
endif

# Stuff that doesn't get linked into an executable NEEDS to be mentioned here,
# or it won't be distributed with 'make dist'

EXTRA_DATA= quakeforge-nq-glx.desktop quakeforge-nq-sdl.desktop \
	quakeforge-nq-sdl32.desktop quakeforge-nq-sgl.desktop \
	quakeforge-nq-x11.desktop quakeforge-qw-glx.desktop \
	quakeforge-qw-sdl.desktop quakeforge-qw-sdl32.desktop \
	quakeforge-qw-sgl.desktop quakeforge-qw-x11.desktop

# Kill the temp files, hopefully.
CLEANFILES	= *.i *.s $(YACCLEX_CLEANFILES)
