/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#include <getopt.h>

#include "QF/dstring.h"
#include "QF/vfile.h"

#include "class.h"
#include "def.h"
#include "emit.h"
#include "function.h"
#include "obj_file.h"
#include "options.h"
#include "qfcc.h"
#include "type.h"

int num_linenos;
pr_lineno_t *linenos;
pr_info_t pr;
defspace_t *new_defspace (void) {return 0;}
scope_t *new_scope (scope_type type, defspace_t *space, scope_t *parent) {return 0;}
string_t ReuseString (const char *str) {return 0;}
void encode_type (dstring_t *str, type_t *type) {}
codespace_t *codespace_new (void) {return 0;}
void codespace_addcode (codespace_t *codespace, struct statement_s *code, int size) {}
type_t *parse_type (const char *str) {return 0;}
int function_parms (function_t *f, byte *parm_size) {return 0;}
pr_auxfunction_t *new_auxfunction (void) {return 0;}

static const struct option long_options[] = {
	{NULL, 0, NULL, 0},
};

void
dump_defs (qfo_t *qfo)
{
	qfo_def_t  *def;
	qfo_func_t *func;

	for (def = qfo->defs; def - qfo->defs < qfo->num_defs; def++) {
		printf ("%5d %4d %4x %d %s %s %d %d %s:%d\n",
				def - qfo->defs,
				def->ofs,
				def->flags,
				def->basic_type,
				qfo->types + def->full_type,
				qfo->strings + def->name,
				def->relocs, def->num_relocs,
				qfo->strings + def->file, def->line);
		if (def->flags & (QFOD_LOCAL | QFOD_EXTERNAL))
			continue;
#if 1
		if (def->basic_type == ev_string) {
				printf ("    %4d %s\n", qfo->data[def->ofs].string_var,
						qfo->strings + qfo->data[def->ofs].string_var);
		} else if (def->basic_type == ev_func) {
			if (qfo->data[def->ofs].func_var < 1
				|| qfo->data[def->ofs].func_var - 1 > qfo->num_funcs)
				func = 0;
			else
				func = qfo->funcs + qfo->data[def->ofs].func_var - 1;
			printf ("    %4d %s\n", qfo->data[def->ofs].func_var,
					func ? qfo->strings + func->name : "BORKAGE");
		} else if (def->basic_type == ev_field) {
				printf ("    %4d\n", qfo->data[def->ofs].integer_var);
		} else {
//				printf ("    %4d\n", qfo->data[def->ofs].integer_var);
		}
#endif
	}
}

void
dump_funcs (qfo_t *qfo)
{
	qfo_func_t *func;
	int         i;
	const char *str = qfo->strings;

	for (i = 0; i < qfo->num_funcs; i++) {
		func = qfo->funcs + i;
		printf ("%5d %s %s:%d  %d?%d %d %d,%d\n", i,
				str + func->name, str + func->file, func->line,
				func->builtin, func->code, func->def,
				func->relocs, func->num_relocs);
	}
}

const char *reloc_names[] = {
	"none",
	"op_a_def",
	"op_b_def",
	"op_c_def",
	"op_a_op",
	"op_b_op",
	"op_c_op",
	"def_op",
	"def_def",
	"def_func",
	"def_string",
	"def_field",
};

void
dump_relocs (qfo_t *qfo)
{
	qfo_reloc_t *reloc;
	int         i;

	for (i = 0; i < qfo->num_relocs; i++) {
		reloc = qfo->relocs + i;
		printf ("%5d %5d %-10s %d\n", i, reloc->ofs, reloc_names[reloc->type],
				reloc->def);
	}
}

void
dump_lines (qfo_t *qfo)
{
	pr_lineno_t *line;
	int         i;

	for (i = 0; i < qfo->num_lines; i++) {
		line = qfo->lines + i;
		printf ("%5d %5d %d\n", i, line->fa.addr, line->line);
	}
}

int
main (int argc, char **argv)
{
	int         c;
	qfo_t      *qfo;

	while ((c = getopt_long (argc, argv, "", long_options, 0)) != EOF) {
		switch (c) {
			default:
				return 1;
		}
	}
	while (optind < argc) {
		qfo = qfo_read (argv[optind++]);
		if (!qfo)
			return 1;
		dump_defs (qfo);
		dump_funcs (qfo);
		dump_relocs (qfo);
		dump_lines (qfo);
	}
	return 0;
}
