/*
	link.c

	qc object file linking

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/7/3

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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/hash.h"

#include "def.h"
#include "emit.h"
#include "expr.h"
#include "immediate.h"
#include "linker.h"
#include "obj_file.h"
#include "options.h"
#include "qfcc.h"
#include "reloc.h"
#include "strpool.h"

typedef struct {
	qfo_def_t  *defs;
	int         num_defs;
	int         max_defs;
} defgroup_t;

static hashtab_t *extern_defs;
static hashtab_t *defined_defs;

static codespace_t *code;
static defspace_t *data;
static defspace_t *far_data;
static strpool_t *strings;
static strpool_t *type_strings;
static struct {
	qfo_reloc_t *relocs;
	int         num_relocs;
	int         max_relocs;
} relocs;
static defgroup_t global_defs, local_defs, defs;
static struct {
	qfo_function_t *funcs;
	int         num_funcs;
	int         max_funcs;
} funcs;
static struct {
	pr_lineno_t *lines;
	int         num_lines;
	int         max_lines;
} lines;
static int      code_base;
static int      data_base;
static int      far_data_base;
static int      reloc_base;
static int      func_base;
static int      line_base;

static void
defgroup_add_defs (defgroup_t *defgroup, qfo_def_t *defs, int num_defs)
{
	if (defgroup->num_defs + num_defs > defgroup->max_defs) {
		defgroup->max_defs = RUP (defgroup->num_defs + num_defs, 16384);
		defgroup->defs = realloc (defgroup->defs,
								  defgroup->max_defs * sizeof (qfo_def_t));
	}
	memcpy (defgroup->defs + defgroup->num_defs, defs,
			num_defs * sizeof (qfo_def_t));
	defgroup->num_defs += num_defs;
}

static const char *
defs_get_key (void *_def, void *unused)
{
	qfo_def_t  *def = (qfo_def_t *) _def;

	return strings->strings + def->name;
}

static void
add_strings (qfo_t *qfo)
{
	int         i;

	for (i = 0; i < qfo->strings_size; i += strlen (qfo->strings + i) + 1)
		strpool_addstr (strings, qfo->strings + i);
}

static void
add_relocs (qfo_t *qfo)
{
	int         i;

	if (relocs.num_relocs + qfo->num_relocs > relocs.max_relocs) {
		relocs.max_relocs = RUP (relocs.num_relocs + qfo->num_relocs, 16384);
		relocs.relocs = realloc (relocs.relocs,
								 relocs.max_relocs * sizeof (qfo_reloc_t));
	}
	relocs.num_relocs += qfo->num_relocs;
	memcpy (relocs.relocs + reloc_base, qfo->relocs,
			qfo->num_relocs * sizeof (qfo_reloc_t));
	for (i = reloc_base; i < relocs.num_relocs; i++) {
		qfo_reloc_t *reloc = relocs.relocs + i;
		switch ((reloc_type)reloc->type) {
			case rel_none:
				break;
			case rel_op_a_def:
			case rel_op_b_def:
			case rel_op_c_def:
			case rel_op_a_op:
			case rel_op_b_op:
			case rel_op_c_op:
				reloc->ofs += code_base;
				break;
			case rel_def_op:
				reloc->ofs += data_base;
				reloc->def += code_base;
				break;
			case rel_def_func:
				reloc->ofs += data_base;
				reloc->def += func_base;
				break;
			case rel_def_def:
			case rel_def_string:
				reloc->ofs += data_base;
				data->data[reloc->ofs].string_var =
					strpool_addstr (strings,
							qfo->strings + data->data[reloc->ofs].string_var);
				break;
		}
	}
}

static void
process_def (qfo_def_t *def)
{
	qfo_def_t  *d;

	if (def->flags & QFOD_EXTERNAL) {
		if ((d = Hash_Find (defined_defs, strings->strings + def->name))) {
			def->ofs = d->ofs;
			def->flags = d->flags;
		} else {
			Hash_Add (extern_defs, def);
		}
	} else {
		if (def->flags & QFOD_GLOBAL) {
			if ((d = Hash_Find (defined_defs,
								strings->strings + def->name))) {
				pr.source_file = def->file;
				pr.source_line = def->line;
				error (0, "%s redefined", strings->strings + def->name);
			}
		}
		if (def->flags & QFOD_GLOBAL) {
			while ((d = Hash_Find (extern_defs,
								   strings->strings + def->name))) {
				Hash_Del (extern_defs, strings->strings + d->name);
				if (d->full_type != def->full_type) {
					pr.source_file = def->file;
					pr.source_line = def->line;
					error (0, "type mismatch `%s' `%s'",
						   type_strings->strings + def->full_type,
						   type_strings->strings + d->full_type);
					continue;
				}
				d->ofs = def->ofs;
				d->flags = def->flags;
			}
			Hash_Add (defined_defs, def);
		}
	}
}

static void
fixup_def (qfo_t *qfo, qfo_def_t *def, int def_num)
{
	int         i;
	qfo_reloc_t *reloc;
	qfo_function_t *func;

	def->full_type = strpool_addstr (type_strings,
									 qfo->types + def->full_type);
	def->name = strpool_addstr (strings, qfo->strings + def->name);
	def->relocs += reloc_base;
	for (i = 0, reloc = relocs.relocs + def->relocs;
		 i < def->num_relocs;
		 i++, reloc++)
		reloc->def = def_num;
	def->file = strpool_addstr (strings, qfo->strings + def->file);

	if ((def->flags & (QFOD_LOCAL | QFOD_ABSOLUTE)))
		return;
	if (!def->flags & QFOD_EXTERNAL) {
		def->ofs += data_base;
		if (def->basic_type == ev_func && (def->flags & QFOD_INITIALIZED)) {
			func = funcs.funcs + data->data[def->ofs].func_var - 1 + func_base;
			func->def = def_num;
		}
	}
	process_def (def);
}

static void
add_defs (qfo_t *qfo)
{
	qfo_def_t  *s, *e;

	for (s = e = qfo->defs; s - qfo->defs < qfo->num_defs; s = e) {
		int         def_num = global_defs.num_defs;
		qfo_def_t  *d;
		while (e - qfo->defs < qfo->num_defs && !(e->flags & QFOD_LOCAL))
			e++;
		defgroup_add_defs (&global_defs, s, e - s);
		for (d = global_defs.defs + def_num; def_num < global_defs.num_defs;
			 d++, def_num++)
			fixup_def (qfo, d, def_num);
		while (e - qfo->defs < qfo->num_defs && (e->flags & QFOD_LOCAL))
			e++;
	}
}

static void
add_funcs (qfo_t *qfo)
{
	int         i;

	if (funcs.num_funcs + qfo->num_functions > funcs.max_funcs) {
		funcs.max_funcs = RUP (funcs.num_funcs + qfo->num_functions, 16384);
		funcs.funcs = realloc (funcs.funcs,
								 funcs.max_funcs * sizeof (qfo_function_t));
	}
	funcs.num_funcs += qfo->num_functions;
	memcpy (funcs.funcs + func_base, qfo->functions,
			qfo->num_functions * sizeof (qfo_function_t));
	for (i = func_base; i < funcs.num_funcs; i++) {
		qfo_function_t *func = funcs.funcs + i;
		func->name = strpool_addstr (strings, qfo->strings + func->name);
		func->file = strpool_addstr (strings, qfo->strings + func->file);
		if (func->code)
			func->code += code_base;
		if (func->num_local_defs) {
			int         def_num = local_defs.num_defs;
			qfo_def_t  *d;
			defgroup_add_defs (&local_defs, qfo->defs + func->local_defs,
							   func->num_local_defs);
			func->local_defs = def_num;
			for (d = local_defs.defs + def_num; def_num < local_defs.num_defs;
				 d++, def_num++)
				fixup_def (qfo, d, def_num);
		}
		if (func->line_info)
			func->line_info += line_base;
		func->relocs += reloc_base;
	}
}

static void
add_lines (qfo_t *qfo)
{
	int         i;

	if (lines.num_lines + qfo->num_lines > lines.max_lines) {
		lines.max_lines = RUP (lines.num_lines + qfo->num_lines, 16384);
		lines.lines = realloc (lines.lines,
							   lines.max_lines * sizeof (pr_lineno_t));
	}
	lines.num_lines += qfo->num_lines;
	memcpy (lines.lines + line_base, qfo->lines,
			qfo->num_lines * sizeof (pr_lineno_t));
	for (i = line_base; i < lines.num_lines; i++) {
		pr_lineno_t *line = lines.lines + i;
		if (line->line)
			line->fa.addr += code_base;
		else
			line->fa.func += func_base;
	}
}

static void
fixup_relocs ()
{
	qfo_reloc_t *reloc;
	qfo_def_t  *def;

	for (reloc = relocs.relocs;
		 reloc - relocs.relocs < relocs.num_relocs;
		 reloc++) {
		def = 0;
		switch ((reloc_type)reloc->type) {
			case rel_none:
				break;

			case rel_op_a_def:
			case rel_op_b_def:
			case rel_op_c_def:
			case rel_def_def:
				def = defs.defs + reloc->def;
				if (def->flags & (QFOD_EXTERNAL | QFOD_LOCAL | QFOD_ABSOLUTE))
					continue;
				break;

			case rel_def_op:
			case rel_op_a_op:
			case rel_op_b_op:
			case rel_op_c_op:
			case rel_def_func:
			case rel_def_string:
				break;
		}
		switch ((reloc_type)reloc->type) {
			case rel_none:
				break;
			case rel_op_a_def:
				code->code[reloc->ofs].a = def->ofs;
				break;
			case rel_op_b_def:
				code->code[reloc->ofs].b = def->ofs;
				break;
			case rel_op_c_def:
				code->code[reloc->ofs].c = def->ofs;
				break;
			case rel_op_a_op:
			case rel_op_b_op:
			case rel_op_c_op:
				break;
			case rel_def_op:
				data->data[reloc->ofs].integer_var = reloc->def;
				break;
			case rel_def_def:
				data->data[reloc->ofs].integer_var = def->ofs;
				break;
			case rel_def_func:
				data->data[reloc->ofs].integer_var = reloc->def + 1;
				break;
			case rel_def_string:
				break;
		}
	}
}

static void
merge_defgroups (void)
{
	int         local_base, i, j;
	qfo_def_t  *def;
	qfo_function_t *func;

	defgroup_add_defs (&defs, global_defs.defs, global_defs.num_defs);
	local_base = defs.num_defs;
	defgroup_add_defs (&defs, local_defs.defs, local_defs.num_defs);
	for (i = 0; i < local_defs.num_defs; i++) {
		def = local_defs.defs + i;
		for (j = def->relocs; j < def->relocs + def->num_relocs; j++)
			relocs.relocs[j].def += local_base;
	}
	for (i = 0; i < funcs.num_funcs; i++) {
		func = funcs.funcs + i;
		func->local_defs += local_base;
	}
}

static void
define_def (const char *name, etype_t basic_type, const char *full_type)
{
	qfo_def_t   d;
	pr_type_t   val = {0};

	memset (&d, 0, sizeof (d));
	d.basic_type = basic_type;
	d.full_type = strpool_addstr (type_strings, full_type);
	d.name = strpool_addstr (strings, name);
	d.ofs = data->size;
	d.flags = QFOD_GLOBAL;
	defspace_adddata (data, &val, 1);
	defgroup_add_defs (&global_defs, &d, 1);
	process_def (&d);
}

void
linker_begin (void)
{
	extern_defs = Hash_NewTable (16381, defs_get_key, 0, 0);
	defined_defs = Hash_NewTable (16381, defs_get_key, 0, 0);
	code = codespace_new ();
	data = new_defspace ();
	far_data = new_defspace ();
	strings = strpool_new ();
	type_strings = strpool_new ();

	pr.strings = strings;
}

void
linker_add_object_file (const char *filename)
{
	qfo_t      *qfo;

	qfo = qfo_read (filename);
	if (!qfo)
		return;  

	puts(filename);

	code_base = code->size;
	data_base = data->size;
	far_data_base = far_data->size;
	reloc_base = relocs.num_relocs;
	func_base = funcs.num_funcs;
	line_base = lines.num_lines;

	codespace_addcode (code, qfo->code, qfo->code_size);
	defspace_adddata (data, qfo->data, qfo->data_size);
	defspace_adddata (far_data, qfo->far_data, qfo->far_data_size);
	add_strings (qfo);
	add_relocs (qfo);
	add_funcs (qfo);
	add_defs (qfo);
	add_lines (qfo);
	
	qfo_delete (qfo);
}

qfo_t *
linker_finish (void)
{
	qfo_def_t **undef_defs, **def;
	qfo_t      *qfo;

	if (!options.partial_link) {
		int         did_self = 0, did_this = 0;

		undef_defs = (qfo_def_t **) Hash_GetList (extern_defs);
		for (def = undef_defs; *def; def++) {
			const char *name = strings->strings + (*def)->name;

			if (strcmp (name, ".self") == 0 && !did_self) {
				define_def (".self", ev_entity, "E");
				did_self = 1;
			} else if (strcmp (name, ".this") == 0 && !did_this) {
				define_def (".this", ev_field, "F@");
				did_this = 1;
			}
		}
		free (undef_defs);
		undef_defs = (qfo_def_t **) Hash_GetList (extern_defs);
		for (def = undef_defs; *def; def++) {
			const char *name = strings->strings + (*def)->name;
			pr.source_file = (*def)->file;
			pr.source_line = (*def)->line;
			pr.strings = strings;
			error (0, "undefined symbol %s", name);
		}
		free (undef_defs);
		if (pr.error_count)
			return 0;
	}

	merge_defgroups ();

	fixup_relocs ();

	qfo = qfo_new ();
	qfo_add_code (qfo, code->code, code->size);
	qfo_add_data (qfo, data->data, data->size);
	qfo_add_far_data (qfo, far_data->data, far_data->size);
	qfo_add_strings (qfo, strings->strings, strings->size);
	qfo_add_relocs (qfo, relocs.relocs, relocs.num_relocs);
	qfo_add_defs (qfo, defs.defs, defs.num_defs);
	qfo_add_functions (qfo, funcs.funcs, funcs.num_funcs);
	qfo_add_lines (qfo, lines.lines, lines.num_lines);
	qfo_add_types (qfo, type_strings->strings, type_strings->size);
	return qfo;
}
