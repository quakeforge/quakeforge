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
AUTOMAKE_OPTIONS= foreign

QFCC_LIBS=@QFCC_LIBS@
QFCC_DEPS=@QFCC_DEPS@
QFCC_INCS=@QFCC_INCS@

INCLUDES=	-I$(top_srcdir)/include $(QFCC_INCS)
YFLAGS = -d

if BUILD_QFCC
qfcc=qfcc
qfodump=qfodump
else
qfcc=
qfodump=
endif

bin_PROGRAMS=	$(qfcc)
noinst_PROGRAMS= $(qfodump)
EXTRA_PROGRAMS= qfcc qfodump

qfcc_SOURCES= \
	class.c constfold.c cpp.c debug.c def.c emit.c expr.c function.c \
	idstuff.c immediate.c linker.c method.c obj_file.c opcodes.c options.c \
	qc-lex.l qc-parse.y qfcc.c reloc.c strpool.c struct.c switch.c type.c

qfcc_LDADD=			$(QFCC_LIBS)
qfcc_DEPENDENCIES=	$(QFCC_DEPS)

qfodump_SOURCES=		obj_file.c qfodump.c strpool.c
qfodump_LDADD=			$(QFCC_LIBS)
qfodump_DEPENDENCIES=	$(QFCC_DEPS)

constfold.$(OBJEXT) emit.$(OBJEXT) expr.$(OBJEXT) switch.$(OBJEXT) qc-lex.$(OBJEXT): qc-parse.h

EXTRA_DIST=qc-lex.c qc-parse.c qc-parse.h
