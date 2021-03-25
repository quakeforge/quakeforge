/*
	dump_lines.c

	Dump line number information.

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/09/06

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/progs.h"
#include "QF/pr_type.h"

#include "tools/qfcc/include/obj_file.h"
#include "tools/qfcc/include/qfprogs.h"

typedef struct {
	const char *source_name;
	const char *source_file;
	pr_uint_t   source_line;
	pr_int_t    first_statement;
	pointer_t   return_type;
	pr_uint_t   local_defs;
	pr_uint_t   num_locals;
	pr_uint_t   line_info;
	pr_uint_t   function;
} func_data_t;

typedef func_data_t *(*get_func_data_t)(unsigned func, void *data);

static func_data_t *
progs_get_func_data (unsigned func_index, void *data)
{
	static func_data_t func_data;
	progs_t    *pr = (progs_t *) data;
	pr_auxfunction_t *aux_func;
	dfunction_t *func;

	memset (&func_data, 0, sizeof (func_data));
	aux_func = PR_Debug_AuxFunction (pr, func_index);
	if (aux_func) {
		func_data.source_line = aux_func->source_line;
		func_data.return_type = aux_func->return_type;
		func_data.num_locals = aux_func->num_locals;
		func_data.local_defs = aux_func->local_defs;
		func_data.line_info = aux_func->line_info;
		func_data.function = aux_func->function;
		if (aux_func->function < (unsigned int) pr->progs->numfunctions) {
			func = pr->pr_functions + aux_func->function;
			func_data.source_file = pr->pr_strings + func->s_file;
			func_data.source_name = pr->pr_strings + func->s_name;
			func_data.first_statement = func->first_statement;
		}
		return &func_data;
	}
	return 0;
}

static void
dump_line_set (pr_lineno_t *lineno, unsigned count,
			   get_func_data_t get_func_data, void *data)
{
	unsigned int line, addr;
	func_data_t *func_data = 0;

	for (; count-- > 0; lineno++) {
		if (!lineno->line) {
			func_data = get_func_data(lineno->fa.func, data);
		}

		printf ("%5u %5u", lineno->fa.addr, lineno->line);
		line = addr = -1;
		if (func_data) {
			line = func_data->source_line + lineno->line;
			if (func_data->source_name) {
				addr = lineno->line ? (pr_int_t) lineno->fa.addr
									: func_data->first_statement;
				printf (" %05x %s:%u %s+%u %d", addr, func_data->source_file,
						line, func_data->source_name,
						addr - func_data->first_statement,
						func_data->return_type);
			} else {
				printf ("%u %u %u %u %u %d", func_data->function, line,
						func_data->line_info, func_data->local_defs,
						func_data->num_locals, func_data->return_type);
			}
		} else if (lineno->line) {
			printf ("%5x", lineno->fa.addr);
		}
		printf ("\n");
	}
}

void
dump_lines (progs_t *pr)
{
	pr_lineno_t *linenos;
	pr_uint_t   num_linenos;
	if (!(linenos = PR_Debug_Linenos (pr, 0, &num_linenos)))
		return;
	dump_line_set (linenos, num_linenos, progs_get_func_data, pr);
}

static func_data_t *
qfo_get_func_data (unsigned func_index, void *data)
{
	return (func_data_t *) data;
}

static void
qfo_set_func_data (qfo_t *qfo, qfo_func_t *func, func_data_t *func_data)
{
	qfot_type_t *type;

	func_data->source_line = func->line;
	//FIXME check type
	type = QFO_POINTER (qfo, qfo_type_space, qfot_type_t, func->type);
	func_data->return_type = type->func.return_type;
	func_data->num_locals = -1;
	if (func->locals_space < qfo->num_spaces) {
		func_data->num_locals = qfo->spaces[func->locals_space].num_defs;
	}
	func_data->local_defs = func->locals_space;
	func_data->line_info = func->line_info;
	func_data->function = func - qfo->funcs;
	func_data->source_file = QFO_GETSTR (qfo, func->file);
	func_data->source_name = QFO_GETSTR (qfo, func->name);
	func_data->first_statement = func->code;
}

void
qfo_lines (qfo_t *qfo)
{
	static func_data_t func_data;
	pr_lineno_t *start_lineno = 0;
	pr_lineno_t *lineno;
	qfo_func_t  *func = 0;

	for (func = qfo->funcs; func - qfo->funcs < qfo->num_funcs; func++) {
		if (!func->line_info) {
			// builtin
			continue;
		}
		if (func->line_info >= qfo->num_lines) {
			printf ("%s: bad line info: %u >= %u\n",
					QFO_GETSTR (qfo, func->name),
					func->line_info, qfo->num_lines);
			continue;
		}
		qfo_set_func_data(qfo, func, &func_data);
		start_lineno = qfo->lines + func->line_info;
		for (lineno = start_lineno + 1;
			 lineno - qfo->lines < qfo->num_lines && lineno->line;
			 lineno++)
		{
		}
		dump_line_set (start_lineno, lineno-start_lineno,
					   qfo_get_func_data, &func_data);
	}
}
