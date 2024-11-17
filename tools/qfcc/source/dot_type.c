/*
	dot_type.c

	"emit" types to dot (graphvis).

	Copyright (C) 2020 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2020/03/28

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

#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/quakeio.h"
#include "QF/set.h"
#include "QF/va.h"

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/strpool.h"

typedef void (*print_f) (dstring_t *dstr, const type_t *, int, set_t *);
static void dot_print_type (dstring_t *dstr, const type_t *t, int level,
							set_t *seen);

static void
print_pointer (dstring_t *dstr, const type_t *t, int level, set_t *seen)
{
	int         indent = level * 2 + 2;
	auto aux = t->fldptr.type;

	dot_print_type (dstr, aux, level, seen);
	dasprintf (dstr, "%*st_%p -> \"t_%p\";\n", indent, "", t, aux);
	dasprintf (dstr, "%*st_%p [label=\"%c\"];\n", indent, "", t,
			   t->type == ev_ptr ? '*' : '.');
}

static void
print_ellipsis (dstring_t *dstr, int level, set_t *seen)
{
	static int  ellipsis_id = 0;	// no type has id 0
	int         indent = level * 2 + 2;

	if (set_is_member (seen, ellipsis_id)) {
		return;
	}
	set_add (seen, ellipsis_id);
	dasprintf (dstr, "%*st_ellipsis [label=\"...\"];\n", indent, "");
}

static void
print_function (dstring_t *dstr, const type_t *t, int level, set_t *seen)
{
	int         indent = level * 2 + 2;
	const ty_func_t *func = &t->func;
	const type_t *ret = func->ret_type;
	const type_t *param;

	dot_print_type (dstr, ret, level + 1, seen);
	if (func->num_params < 0) {
		for (int i = 0; i < ~func->num_params; i++) {
			param = func->param_types[i];
			dot_print_type (dstr, param, level + 1, seen);
		}
		print_ellipsis (dstr, level, seen);
	} else {
		for (int i = 0; i < func->num_params; i++) {
			param = func->param_types[i];
			dot_print_type (dstr, param, level + 1, seen);
		}
	}
	dasprintf (dstr, "%*st_%p -> \"t_%p\" [label=\"r\"];\n", indent, "",
			   t, ret);
	if (func->num_params < 0) {
		for (int i = 0; i < ~func->num_params; i++) {
			param = func->param_types[i];
			dasprintf (dstr, "%*st_%p -> \"t_%p\";\n", indent, "", t, param);
		}
		dasprintf (dstr, "%*st_%p -> \"t_ellipsis\";\n", indent, "", t);
	} else {
		for (int i = 0; i < func->num_params; i++) {
			param = func->param_types[i];
			dasprintf (dstr, "%*st_%p -> \"t_%p\";\n", indent, "", t, param);
		}
	}
	dasprintf (dstr, "%*st_%p [label=\"( )\"];\n", indent, "", t);
}

static void
print_basic (dstring_t *dstr, const type_t *t, int level, set_t *seen)
{
	if (t->type == ev_ptr || t->type == ev_field) {
		print_pointer (dstr, t, level, seen);
	} else if (t->type == ev_func) {
		print_function (dstr, t, level, seen);
	} else {
		int         indent = level * 2 + 2;
		dasprintf (dstr, "%*st_%p [label=\"%s\"];\n", indent, "", t, t->name);
	}
}

static void
print_struct (dstring_t *dstr, const type_t *t, int level, set_t *seen)
{
	int         indent = level * 2 + 2;
	const symtab_t *symtab = t->symtab;
	const symbol_t *sym;
	int         pnum;
	static const char *struct_type_names[3] = {"struct", "union", "enum"};
	const char *struct_type = struct_type_names[t->meta - ty_struct];

	if (!symtab) {
		dasprintf (dstr, "%*st_%p [label=\"%s %s\"];\n", indent, "", t,
				   struct_type, quote_string (t->name));
		return;
	}
	if (t->meta != ty_enum) {
		for (sym = symtab->symbols; sym; sym = sym->next) {
			if (sym->sy_type != sy_offset) {
				continue;
			}
			dot_print_type (dstr, sym->type, level, seen);
		}
		for (pnum = 0, sym = symtab->symbols; sym; sym = sym->next) {
			if (sym->sy_type != sy_offset) {
				continue;
			}
			dasprintf (dstr, "%*st_%p:f%d -> \"t_%p\";\n", indent, "",
					   t, pnum++, sym->type);
		}
	}
	dasprintf (dstr, "%*st_%p [shape=none,label=<\n", indent, "", t);
	dasprintf (dstr, "%*s<table border=\"0\" cellborder=\"1\" "
			   "cellspacing=\"0\">\n",
			   indent + 2, "");
	dasprintf (dstr, "%*s<tr><td colspan=\"2\">%s %s</td></tr>\n",
			   indent + 4, "",
			   struct_type, quote_string (t->name));
	for (pnum = 0, sym = symtab->symbols; sym; sym = sym->next) {
		int         val;
		const char *port = "";
		if (sym->sy_type == sy_const) {
			val = sym->value->int_val;
		} else {
			if (sym->sy_type != sy_offset) {
				continue;
			}
			val = sym->offset;
			port = va (0, " port=\"f%d\"", pnum++);
		}
		dasprintf (dstr, "%*s<tr><td>%s</td><td%s>%d</td></tr>\n",
				   indent + 4, "",
				   quote_string (sym->name), port, val);
	}
	dasprintf (dstr, "%*s</table>\n", indent + 2, "");
	dasprintf (dstr, "%*s>];\n", indent, "");
}

static void
print_array (dstring_t *dstr, const type_t *t, int level, set_t *seen)
{
	int         indent = level * 2 + 2;
	auto type = t->array.type;

	dot_print_type (dstr, type, level, seen);
	dasprintf (dstr, "%*st_%p -> \"t_%p\";\n", indent, "", t, type);
	if (t->array.base) {
		dasprintf (dstr, "%*st_%p [label=\"[%d..%d]\"];\n", indent, "", t,
				   t->array.base, t->array.base + t->array.count - 1);
	} else {
		dasprintf (dstr, "%*st_%p [label=\"[%d]\"];\n", indent, "", t,
				   t->array.count);
	}
}

static void
print_class (dstring_t *dstr, const type_t *t, int level, set_t *seen)
{
	int         indent = level * 2 + 2;
	dasprintf (dstr, "%*st_%p [label=\"class '%s'\"];\n", indent, "", t,
			   t->class->name);
}

static void
print_alias (dstring_t *dstr, const type_t *t, int level, set_t *seen)
{
	int         indent = level * 2 + 2;
	auto aux = t->alias.aux_type;
	auto full = t->alias.full_type;

	dot_print_type (dstr, aux, level, seen);
	dot_print_type (dstr, full, level, seen);
	dasprintf (dstr, "%*st_%p -> \"t_%p\";\n", indent, "", t, aux);
	dasprintf (dstr, "%*st_%p -> \"t_%p\";\n", indent, "", t, full);
	dasprintf (dstr, "%*st_%p [label=\"alias '%s'\"];\n", indent, "", t,
			   t->name);
}

static void
dot_print_type (dstring_t *dstr, const type_t *t, int level, set_t *seen)
{
	static print_f print_funcs[] = {
		print_basic,
		print_struct,
		print_struct,
		print_struct,
		print_array,
		print_class,
		print_alias,
	};
	int         indent = level * 2 + 2;

	if (!t) {
		dasprintf (dstr, "%*s\"e_%p\" [label=\"(null)\"];\n", indent, "", t);
		return;
	}
	if (set_is_member (seen, t->id))		// already printed this type
		return;
	set_add (seen, t->id);

	if ((unsigned) t->meta >= sizeof (print_funcs) / sizeof (print_funcs[0])) {
		dasprintf (dstr, "%*se_%p [label=\"(bad type meta)\\n%d\"];\n",
				   indent, "", t, t->meta);
		return;
	}
	print_funcs [t->meta] (dstr, t, level, seen);
}

void
dump_dot_type (void *_t, const char *filename)
{
	set_t      *seen = set_new ();
	dstring_t  *dstr = dstring_newstr ();
	const type_t *t = _t;

	dasprintf (dstr, "digraph type_%p {\n", t);
	dasprintf (dstr, "  graph [label=\"%s\"];\n",
			   filename ? quote_string (filename) : "");
	dasprintf (dstr, "  layout=dot; rankdir=TB; compound=true;\n");
	dot_print_type (dstr, t, 0, seen);
	dasprintf (dstr, "}\n");

	if (filename) {
		QFile      *file;

		file = Qopen (filename, "wt");
		Qwrite (file, dstr->str, dstr->size - 1);
		Qclose (file);
	} else {
		fputs (dstr->str, stdout);
	}
	dstring_delete (dstr);
	set_delete (seen);
}
