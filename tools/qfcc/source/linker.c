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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/pakfile.h"
#include "QF/va.h"

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
#include "type.h"

#define Xgroup(X)\
typedef struct {\
	qfo_##X##_t  *X##s;\
	int         num_##X##s;\
	int         max_##X##s;\
} X##group_t;

Xgroup(def)		// defgroup_t
Xgroup(reloc)	// relocgroup_t
Xgroup(func)	// funcgroup_t

typedef struct path_s {
	struct path_s *next;
	const char *path;
} path_t;

typedef union defref_s {
	int         def;
	union defref_s *next;
} defref_t;

typedef struct builtin_sym_s {
	const char *name;
	type_t     *type;
	unsigned    flags;
} builtin_sym_t;

static builtin_sym_t builtin_symbols[] = {
	{".zero",		&type_zero,		QFOD_NOSAVE},
	{".return",		&type_param,	QFOD_NOSAVE},
	{".param_0",	&type_param,	QFOD_NOSAVE},
	{".param_1",	&type_param,	QFOD_NOSAVE},
	{".param_2",	&type_param,	QFOD_NOSAVE},
	{".param_3",	&type_param,	QFOD_NOSAVE},
	{".param_4",	&type_param,	QFOD_NOSAVE},
	{".param_5",	&type_param,	QFOD_NOSAVE},
	{".param_6",	&type_param,	QFOD_NOSAVE},
	{".param_7",	&type_param,	QFOD_NOSAVE},
};

static defref_t *free_defrefs;

static hashtab_t *extern_defs;
static hashtab_t *defined_defs;
static hashtab_t *field_defs;

static codespace_t *code;
static defspace_t *data;
static defspace_t *far_data;
static defspace_t *entity;
static strpool_t *strings;
static strpool_t *type_strings;
static relocgroup_t relocs, final_relocs;
static defgroup_t global_defs, local_defs, fields, defs;
static funcgroup_t funcs;
static struct {
	pr_lineno_t *lines;
	int         num_lines;
	int         max_lines;
} lines;
static int      code_base;
static int      data_base;
static int      far_data_base;
static int      entity_base;
static int      reloc_base;
static int      func_base;
static int      line_base;

static path_t  *path_head;
static path_t **path_tail = &path_head;

#define DATA(x) (data->data + (x))
#define STRING(x) (strings->strings + (x))
#define TYPE_STRING(x) (type_strings->strings + (x))

#define Xgroup_add(X)\
static void \
X##group_add_##X##s (X##group_t *X##group, qfo_##X##_t *X##s, int num_##X##s)\
{\
	if (X##group->num_##X##s + num_##X##s > X##group->max_##X##s) {\
		X##group->max_##X##s = RUP (X##group->num_##X##s + num_##X##s, 16384);\
		X##group->X##s = realloc (X##group->X##s,\
								  X##group->max_##X##s * sizeof (qfo_##X##_t));\
	}\
	memcpy (X##group->X##s + X##group->num_##X##s, X##s,\
			num_##X##s * sizeof (qfo_##X##_t));\
	X##group->num_##X##s += num_##X##s;\
}

Xgroup_add(def);	// defgroup_add_defs
Xgroup_add(reloc);	// relocgroup_add_relocs
Xgroup_add(func);	// funcgroup_add_funcs

static void def_error (qfo_def_t *def, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
static void def_warning (qfo_def_t *def, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

static defref_t *
get_defref (qfo_def_t *def, defgroup_t *defgroup)
{
	defref_t   *defref;
	
	ALLOC (16384, defref_t, defrefs, defref);
	defref->def = def - defgroup->defs;
	return defref;
}

static qfo_def_t *
deref_def (defref_t *defref, defgroup_t *defgroup)
{
	return defgroup->defs + defref->def;
}

static const char *
defs_get_key (void *_defref, void *_defgroup)
{
	defref_t   *defref = (defref_t *) _defref;
	defgroup_t *defgroup = (defgroup_t *) _defgroup;
	qfo_def_t  *def = deref_def (defref, defgroup);

	return STRING (def->name);
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

	relocgroup_add_relocs (&relocs, qfo->relocs, qfo->num_relocs);
	for (i = reloc_base; i < relocs.num_relocs; i++) {
		qfo_reloc_t *reloc = relocs.relocs + i;
		switch ((reloc_type)reloc->type) {
			case rel_none:
				break;
			case rel_op_a_def:
			case rel_op_b_def:
			case rel_op_c_def:
			case rel_op_a_def_ofs:
			case rel_op_b_def_ofs:
			case rel_op_c_def_ofs:
				reloc->ofs += code_base;
				break;
			case rel_op_a_op:
			case rel_op_b_op:
			case rel_op_c_op:
				// these are relative and fixed up before the .qfo is written
				break;
			case rel_def_op:
				reloc->ofs += data_base;
				reloc->def += code_base;
				break;
			case rel_def_func:
				reloc->ofs += data_base;
				reloc->def += func_base;
				DATA (reloc->ofs)->func_var = reloc->def + 1;
				break;
			case rel_def_def:
			case rel_def_def_ofs:
				reloc->ofs += data_base;
				break;
			case rel_def_string:
				reloc->ofs += data_base;
				DATA (reloc->ofs)->string_var =
					strpool_addstr (strings,
							qfo->strings + DATA (reloc->ofs)->string_var);
				break;
			case rel_def_field:
			case rel_def_field_ofs:
				//FIXME more?
				reloc->ofs += data_base;
				break;
		}
	}
}

static void
linker_type_mismatch (qfo_def_t *def, qfo_def_t *d)
{
	def_error (def, "type mismatch for `%s' `%s'",
			   STRING (def->name),
			   TYPE_STRING (def->full_type));
	def_error (d, "previous definition `%s'", TYPE_STRING (d->full_type));
}

static void
process_def (qfo_def_t *def)
{
	defref_t   *_d;
	qfo_def_t  *d;

	if (def->flags & QFOD_EXTERNAL) {
		if (!def->num_relocs)
			return;
		if ((_d = Hash_Find (defined_defs, STRING (def->name)))) {
			d = deref_def (_d, &global_defs);
			if (d->full_type != def->full_type) {
				linker_type_mismatch (def, d);
				return;
			}
			def->ofs = d->ofs;
			def->flags = d->flags;
			Hash_Add (defined_defs, get_defref (def, &global_defs));
		} else {
			Hash_Add (extern_defs, get_defref (def, &global_defs));
		}
	} else if (def->flags & QFOD_GLOBAL) {
		if ((_d = Hash_Find (defined_defs, STRING (def->name)))) {
			d = deref_def (_d, &global_defs);
			if (d->flags & QFOD_SYSTEM) {
				int         i, size;

				if (d->full_type != def->full_type) {
					linker_type_mismatch (def, d);
					return;
				}
				d->flags &= ~QFOD_SYSTEM;
				d->flags |= def->flags & (QFOD_INITIALIZED | QFOD_CONSTANT);
				size = type_size (parse_type (TYPE_STRING (d->full_type)));
				memcpy (DATA (d->ofs), DATA (def->ofs),
						size * sizeof (pr_type_t));
				for (i = 0; i < relocs.num_relocs; i++) {
					qfo_reloc_t *reloc = relocs.relocs + i;
					if (reloc->type >= rel_def_def
						&& reloc->type <= rel_def_field)
						if (reloc->ofs == def->ofs)
							reloc->ofs = d->ofs;
				}
				def->ofs = d->ofs;
				def->flags = d->flags;
			} else {
				def_error (def, "%s redefined", STRING (def->name));
				def_error (d, "previous definition");
			}
		}
		while ((_d = Hash_Del (extern_defs, STRING (def->name)))) {
			Hash_Add (defined_defs, _d);
			d = deref_def (_d, &global_defs);
			if (d->full_type != def->full_type) {
				linker_type_mismatch (def, d);
				continue;
			}
			d->ofs = def->ofs;
			d->flags = def->flags;
		}
		Hash_Add (defined_defs, get_defref (def, &global_defs));
	}
}

static void
process_field (qfo_def_t *def)
{
	defref_t   *_d;
	qfo_def_t  *field_def;
	pr_type_t  *var = DATA (def->ofs);

	if (strcmp (STRING (def->name), ".imm")) { //FIXME better test
		if ((_d = Hash_Find (field_defs, STRING (def->name)))) {
			field_def = deref_def (_d, &fields);
			def_error (def, "%s redefined", STRING (def->name));
			def_error (field_def, "previous definition");
		}
	}
	defgroup_add_defs (&fields, def, 1);
	field_def = fields.defs + fields.num_defs - 1;
	field_def->ofs = var->integer_var + entity_base;
	Hash_Add (field_defs, get_defref (field_def, &fields));
}

static void
fixup_def (qfo_t *qfo, qfo_def_t *def, int def_num)
{
	pr_int_t    i;
	qfo_reloc_t *reloc;
	qfo_func_t *func;

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
	if (!(def->flags & QFOD_EXTERNAL)) {
		def->ofs += data_base;
		if (def->flags & QFOD_INITIALIZED) {
			pr_type_t  *var = DATA (def->ofs);
			switch (def->basic_type) {
				case ev_func:
					if (var->func_var) {
						func = funcs.funcs + var->func_var - 1;
						func->def = def_num;
					}
					break;
				case ev_field:
					process_field (def);
					break;
				default:
					break;
			}
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

	funcgroup_add_funcs (&funcs, qfo->funcs, qfo->num_funcs);
	for (i = func_base; i < funcs.num_funcs; i++) {
		qfo_func_t *func = funcs.funcs + i;
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
fixup_relocs (void)
{
	defref_t   *_d;
	qfo_reloc_t *reloc;
	qfo_def_t  *def, *field_def;
	qfo_func_t *func;
	int         i;

	for (i = 0; i < final_relocs.num_relocs; i++) {
		reloc = final_relocs.relocs + i;
		def = 0;
		switch ((reloc_type)reloc->type) {
			case rel_none:
				break;

			case rel_op_a_def:
			case rel_op_b_def:
			case rel_op_c_def:
			case rel_def_def:
			case rel_def_field:
			case rel_op_a_def_ofs:
			case rel_op_b_def_ofs:
			case rel_op_c_def_ofs:
			case rel_def_def_ofs:
			case rel_def_field_ofs:
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
			case rel_op_a_def_ofs:
			case rel_op_b_def_ofs:
			case rel_op_c_def_ofs:
			case rel_def_def_ofs:
			case rel_def_field_ofs:
				break;
			case rel_def_op:
				DATA (reloc->ofs)->integer_var = reloc->def;
				break;
			case rel_def_def:
				DATA (reloc->ofs)->integer_var = def->ofs;
				break;
			case rel_def_func:
				break;
			case rel_def_string:
				break;
			case rel_def_field:
				_d = Hash_Find (field_defs, STRING (def->name));
				if (_d) {		// null if not initialized
					field_def = deref_def (_d, &fields);
					DATA (reloc->ofs)->integer_var = field_def->ofs;
				}
				break;
		}
	}
	for (i = 0; i < defs.num_defs; i++) {
		def = defs.defs + i;
		if (def->basic_type == ev_func
			&& (def->flags & QFOD_INITIALIZED)
			&& !(def->flags & (QFOD_LOCAL | QFOD_EXTERNAL | QFOD_ABSOLUTE))) {
			pr_type_t  *var = DATA (def->ofs);
			if (var->func_var) {
				func = funcs.funcs + var->func_var - 1;
				func->def = i;
			}
		}
	}
}

static void
move_def (hashtab_t *deftab, qfo_def_t *d)
{
	defref_t   *_d;
	qfo_def_t  *def;
	int         def_num = defs.num_defs;
	pr_int_t    j;

	defgroup_add_defs (&defs, d, 1);
	def = defs.defs + def_num;
	def->relocs = final_relocs.num_relocs;
	relocgroup_add_relocs (&final_relocs, relocs.relocs + d->relocs,
						   d->num_relocs);
	for (j = 0; j < d->num_relocs; j++) {
		relocs.relocs[d->relocs + j].type = rel_none;
		final_relocs.relocs[def->relocs + j].def = def_num;
	}
	if ((d->flags & QFOD_CONSTANT) && d->basic_type == ev_func) {
		qfo_func_t *func;
		pr_type_t  *var = DATA (d->ofs);

		if (var->func_var) {
			func = funcs.funcs + var->func_var - 1;
			func->def = def_num;
		}
	}
	if (deftab) {
		while ((_d = Hash_Del (deftab, STRING (def->name)))) {
			int         def_relocs;
			d = deref_def (_d, &global_defs);
			relocgroup_add_relocs (&final_relocs, relocs.relocs + d->relocs,
								   d->num_relocs);
			def_relocs = def->relocs + def->num_relocs;
			def->num_relocs += d->num_relocs;
			for (j = 0; j < d->num_relocs; j++) {
				relocs.relocs[d->relocs + j].type = rel_none;
				final_relocs.relocs[def_relocs + j].def = def_num;
			}
			memset (d, 0, sizeof (*d));
		}
	}
}

static void
merge_defgroups (void)
{
	int         local_base, i;
	pr_int_t    j;
	qfo_def_t  *def;
	defref_t   *d;
	qfo_func_t *func;
	static qfo_def_t null_def;

	for (i = 0; i < global_defs.num_defs; i++) {
		const char *name;
		def = global_defs.defs + i;
		if (memcmp (def, &null_def, sizeof (*def)) == 0)
			continue;
		name = STRING (def->name);
		if ((d = Hash_Del (defined_defs, name)))
			move_def (defined_defs, deref_def (d, &global_defs));
		else if ((d = Hash_Del (extern_defs, name)))
			move_def (extern_defs, deref_def (d, &global_defs));
		else if (!(def->flags & (QFOD_GLOBAL | QFOD_EXTERNAL)))
			move_def (0, def);
	}
	local_base = defs.num_defs;
	for (i = 0; i < local_defs.num_defs; i++) {
		move_def (0, local_defs.defs + i);
	}
	for (i = 0; i < funcs.num_funcs; i++) {
		int         r = final_relocs.num_relocs;
		func = funcs.funcs + i;
		func->local_defs += local_base;
		relocgroup_add_relocs (&final_relocs, relocs.relocs + func->relocs,
							   func->num_relocs);
		for (j = 0; j < func->num_relocs; j++)
			relocs.relocs[func->relocs + j].type = rel_none;
		func->relocs = r;
	}
	for (i = 0; i < relocs.num_relocs; i = j) {
		j = i;
//		while (j < relocs.num_relocs && relocs.relocs[j].type != rel_none)
			j++;
		relocgroup_add_relocs (&final_relocs, relocs.relocs + i, j - i);
//		while (j < relocs.num_relocs && relocs.relocs[j].type == rel_none)
			j++;
	}
}

static void
define_def (const char *name, etype_t basic_type, const char *full_type,
			unsigned flags, int size, int v)
{
	qfo_def_t   d;
	pr_type_t  *val = calloc (size, sizeof (pr_type_t));
	
	val->integer_var = v;

	memset (&d, 0, sizeof (d));
	d.basic_type = basic_type;
	d.full_type = strpool_addstr (type_strings, full_type);
	d.name = strpool_addstr (strings, name);
	d.ofs = data->size;
	d.flags = QFOD_GLOBAL | flags;
	if (basic_type == ev_field) {
		d.relocs = relocs.num_relocs;
		d.num_relocs = 1;
	}
	defspace_adddata (data, val, size);
	defgroup_add_defs (&global_defs, &d, 1);
	process_def (global_defs.defs + global_defs.num_defs - 1);
	if (basic_type == ev_field) {
		int         def_num = global_defs.num_defs - 1;
		qfo_def_t  *def = global_defs.defs + def_num;
		qfo_reloc_t rel = {def->ofs, rel_def_field, def_num};
		relocgroup_add_relocs (&relocs, &rel, 1);
		process_field (def);
	}
	free (val);
}

void
linker_begin (void)
{
	size_t      i;

	extern_defs = Hash_NewTable (16381, defs_get_key, 0, &global_defs);
	defined_defs = Hash_NewTable (16381, defs_get_key, 0, &global_defs);
	field_defs = Hash_NewTable (16381, defs_get_key, 0, &fields);
	code = codespace_new ();
	data = new_defspace ();
	far_data = new_defspace ();
	entity = new_defspace ();
	strings = strpool_new ();
	type_strings = strpool_new ();

	pr.strings = strings;
	if (!options.partial_link) {
		dstring_t  *encoding = dstring_new ();

		for (i = 0;
			 i < sizeof (builtin_symbols) / sizeof (builtin_symbols[0]);
			 i++) {
			etype_t     basic_type = builtin_symbols[i].type->type;
			int         size = type_size (builtin_symbols[i].type);

			dstring_clearstr (encoding);
			encode_type (encoding, builtin_symbols[i].type);
			define_def (builtin_symbols[i].name, basic_type, encoding->str,
						builtin_symbols[i].flags, size, 0);
		}
	}
}

static void
linker_add_qfo (qfo_t *qfo)
{
	code_base = code->size;
	data_base = data->size;
	far_data_base = far_data->size;
	reloc_base = relocs.num_relocs;
	func_base = funcs.num_funcs;
	line_base = lines.num_lines;
	entity_base = entity->size;

	codespace_addcode (code, qfo->code, qfo->code_size);
	defspace_adddata (data, qfo->data, qfo->data_size);
	defspace_adddata (far_data, qfo->far_data, qfo->far_data_size);
	defspace_adddata (entity, 0, qfo->entity_fields);
	add_strings (qfo);
	add_relocs (qfo);
	add_funcs (qfo);
	add_defs (qfo);
	add_lines (qfo);
}

int
linker_add_object_file (const char *filename)
{
	qfo_t      *qfo;

	qfo = qfo_open (filename);
	if (!qfo)
		return 1;

	if (options.verbosity >= 2)
		puts (filename);

	linker_add_qfo (qfo);
	
	qfo_delete (qfo);
	return 0;
}

int
linker_add_lib (const char *libname)
{
	pack_t     *pack = 0;
	path_t      start = {path_head, "."};
	path_t     *path = &start;
	const char *path_name = 0;
	int         i, j;
	int         did_something;

	if (strncmp (libname, "-l", 2) == 0) {
		while (path) {
			path_name = va ("%s/lib%s.a", path->path, libname + 2);
			pack = pack_open (path_name);
			if (pack)
				break;
			if (errno != ENOENT) {
				if (errno)
					perror (libname);
				return 1;
			}
			path = path->next;
		}
	} else {
		path_name = libname;
		pack = pack_open (path_name);
	}

	if (!pack) {
		if (errno)
			perror (libname);
		return 1;
	}

	if (options.verbosity > 1)
		puts (path_name);

	do {
		did_something = 0;
		for (i = 0; i < pack->numfiles; i++) {
			QFile      *f;
			qfo_t      *qfo;

			f = Qsubopen (path_name, pack->files[i].filepos,
						  pack->files[i].filelen, 1);
			qfo = qfo_read (f);
			Qclose (f);

			if (!qfo)
				return 1;

			for (j = 0; j < qfo->num_defs; j++) {
				qfo_def_t  *def = qfo->defs + j;
				if ((def->flags & QFOD_GLOBAL)
					&& !(def->flags & QFOD_EXTERNAL)
					&& Hash_Find (extern_defs, qfo->strings + def->name)) {
					if (options.verbosity >= 2)
						printf ("adding %s because of %s\n",
								pack->files[i].name, qfo->strings + def->name);
					linker_add_qfo (qfo);
					did_something = 1;
					break;
				}
			}

			qfo_delete (qfo);
		}
	} while (did_something);
	pack_del (pack);
	return 0;
}

static void
undefined_def (qfo_def_t *def)
{
	qfo_def_t   line_def;
	pr_int_t    i;
	qfo_reloc_t *reloc = relocs.relocs + def->relocs;

	for (i = 0; i < def->num_relocs; i++, reloc++) {
		if ((reloc->type == rel_op_a_def
			 || reloc->type == rel_op_b_def
			 || reloc->type == rel_op_c_def
			 || reloc->type == rel_op_a_def_ofs
			 || reloc->type == rel_op_b_def_ofs
			 || reloc->type == rel_op_c_def_ofs)
			&& lines.lines) {
			qfo_func_t *func = funcs.funcs;
			qfo_func_t *best = func;
			pr_int_t    best_dist = reloc->ofs - func->code;
			pr_lineno_t *line;

			while (best_dist && func - funcs.funcs < funcs.num_funcs) {
				if (func->code <= reloc->ofs) {
					if (best_dist < 0 || reloc->ofs - func->code < best_dist) {
						best = func;
						best_dist = reloc->ofs - func->code;
					}
				}
				func++;
			}
			line = lines.lines + best->line_info;
			line_def.file = best->file;
			line_def.line = best->line;
			if (!line->line
				&& line->fa.func == (pr_uint_t) (best - funcs.funcs)) {
				while (line - lines.lines < lines.num_lines - 1 && line[1].line
					   && line[1].fa.addr <= (pr_uint_t) reloc->ofs)
					line++;
				line_def.line = line->line + best->line;
			}
			def_error (&line_def, "undefined symbol %s", STRING (def->name));
		} else {
			def_error (def, "undefined symbol %s", STRING (def->name));
		}
	}
}

qfo_t *
linker_finish (void)
{
	defref_t  **undef_defs, **defref;
	qfo_t      *qfo;

	if (!options.partial_link) {
		int         did_self = 0, did_this = 0;

		undef_defs = (defref_t **) Hash_GetList (extern_defs);
		for (defref = undef_defs; *defref; defref++) {
			qfo_def_t  *def = deref_def (*defref, &global_defs);
			const char *name = STRING (def->name);

			if (strcmp (name, ".self") == 0 && !did_self) {
				defref_t   *_d = Hash_Find (defined_defs, "self");
				if (_d) {
					qfo_def_t  *d = deref_def (_d, &global_defs);
					if (d->basic_type == ev_entity)
						def_warning (d, "@self and self used together");
				}
				define_def (".self", ev_entity, "E", 0, 1, 0);
				did_self = 1;
			} else if (strcmp (name, ".this") == 0 && !did_this) {
				entity_base = 0;
				define_def (".this", ev_field, "F@", QFOD_NOSAVE, 1,
							entity->size);
				defspace_adddata (entity, 0, 1);
				did_this = 1;
			}
		}
		free (undef_defs);
		undef_defs = (defref_t **) Hash_GetList (extern_defs);
		for (defref = undef_defs; *defref; defref++) {
			qfo_def_t  *def = deref_def (*defref, &global_defs);
			undefined_def (def);
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
	qfo_add_relocs (qfo, final_relocs.relocs, final_relocs.num_relocs);
	qfo_add_defs (qfo, defs.defs, defs.num_defs);
	qfo_add_funcs (qfo, funcs.funcs, funcs.num_funcs);
	qfo_add_lines (qfo, lines.lines, lines.num_lines);
	qfo_add_types (qfo, type_strings->strings, type_strings->size);
	qfo->entity_fields = entity->size;
	return qfo;
}

void
linker_add_path (const char *path)
{
	path_t     *p = malloc (sizeof (path_t));
	p->next = 0;
	p->path = path;
	*path_tail = p;
	path_tail = &p->next;
}

static void
def_error (qfo_def_t *def, const char *fmt, ...)
{
	va_list     args;
	static dstring_t *string;

	if (!string)
		string = dstring_new ();

	va_start (args, fmt);
	dvsprintf (string, fmt, args);
	va_end (args);

	pr.source_file = def->file;
	pr.source_line = def->line;
	pr.strings = strings;
	error (0, "%s", string->str);
}

static void
def_warning (qfo_def_t *def, const char *fmt, ...)
{
	va_list     args;
	static dstring_t *string;

	if (!string)
		string = dstring_new ();

	va_start (args, fmt);
	dvsprintf (string, fmt, args);
	va_end (args);

	pr.source_file = def->file;
	pr.source_line = def->line;
	pr.strings = strings;
	warning (0, "%s", string->str);
}
