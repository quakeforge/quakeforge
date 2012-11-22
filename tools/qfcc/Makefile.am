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
AUTOMAKE_OPTIONS= foreign

SUBDIRS=	include source doc test

dist-zip: distdir
	-chmod -R a+r $(distdir)
	ZIP="-r9q" zip $(distdir).zip $(NOCONV_DIST)
	ZIP="-r9ql" zip $(distdir).zip $(distdir) -x $(NOCONV_DIST)
	-rm -rf $(distdir)

dist-bz2: distdir
	-chmod -R a+r $(distdir)
	BZIP2="-9" $(TAR) Ichof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

dist-all-local: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) chozf $(distdir).tar.gz $(distdir)
	BZIP2="-9" $(TAR) Ichof $(distdir).tar.bz2 $(distdir)
	ZIP="-r9q" zip $(distdir).zip $(NOCONV_DIST)
	ZIP="-r9ql" zip $(distdir).zip $(distdir) -x $(NOCONV_DIST)
	-rm -rf $(distdir)
