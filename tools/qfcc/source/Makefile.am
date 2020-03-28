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

AM_CPPFLAGS=	-I$(top_srcdir)/include $(QFCC_INCS)
YFLAGS = -v -d -Wno-yacc -Werror

bin_PROGRAMS=	qfcc qfprogs
bin_SCRIPTS=	qfpreqcc

common_src=\
	class.c codespace.c constfold.c cpp.c dags.c debug.c def.c defspace.c \
	diagnostic.c \
	dot.c dot_dag.c dot_expr.c dot_flow.c dot_sblock.c dot_type.c \
	emit.c \
	expr.c expr_assign.c expr_binary.c expr_bool.c expr_compound.c expr_obj.c \
	flow.c function.c grab.c \
	idstuff.c \
	linker.c method.c \
	obj_file.c \
	obj_type.c opcodes.c options.c pragma.c qfcc.c reloc.c shared.c \
	statements.c strpool.c struct.c switch.c symtab.c type.c value.c

qfcc_SOURCES= qc-lex.l qc-parse.y qp-lex.l qp-parse.y $(common_src)
qfcc_LDADD=			$(QFCC_LIBS)
qfcc_DEPENDENCIES=	$(QFCC_DEPS)

qfprogs_SOURCES= \
	disassemble.c dump_globals.c dump_lines.c dump_modules.c dump_strings.c \
	obj_file.c qfprogs.c strpool.c stub.c type.c
qfprogs_LDADD=	$(QFCC_LIBS)
qfprogs_DEPENDENCIES= $(QFCC_DEPS)

BUILT_SOURCES=qc-parse.c qc-parse.h qc-lex.c qp-parse.c qp-parse.h qp-lex.c

qc-parse.c: qc-parse.y
	$(YACC) $(YFLAGS) -Dapi.prefix={qc_yy} $< -o $@
qc-lex.c: qc-lex.l qc-parse.h
	$(LEX) $(LFLAGS) $(AM_LFLAGS) -Pqc_yy -o$@ $<
qp-parse.c: qp-parse.y
	$(YACC) $(YFLAGS) -Dapi.prefix={qp_yy} $< -o $@
qp-lex.c: qp-lex.l qp-parse.h
	$(LEX) $(LFLAGS) $(AM_LFLAGS) -Pqp_yy -o$@ $<

EXTRA_DIST=qc-lex.c qc-parse.c qc-parse.h qfpreqcc \
			qp-parse.c qp-parse.h qp-lex.c
