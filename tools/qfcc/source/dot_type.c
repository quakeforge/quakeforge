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

#include <QF/dstring.h>
#include <QF/mathlib.h>
#include <QF/quakeio.h>
#include <QF/va.h>

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/strpool.h"

typedef void (*print_f) (dstring_t *dstr, const type_t *, int, int);
static void dot_print_type (dstring_t *dstr, const type_t *t, int level, int id);

static void
print_pointer (dstring_t *dstr, const type_t *t, int level, int id)
{
	int         indent = level * 2 + 2;
	auto aux = t->fldptr.type;

	dot_print_type (dstr, aux, level, id);
	dasprintf (dstr, "%*st_%p -> \"t_%p\";\n", indent, "", t, aux);
	dasprintf (dstr, "%*st_%p [label=\"%c\"];\n", indent, "", t,
			   t->type == ev_ptr ? '*' : '.');
}

static void
print_ellipsis (dstring_t *dstr, int level, int id)
{
	static int  ellipsis_id;
	int         indent = level * 2 + 2;

	if (ellipsis_id == id) {
		return;
	}
	ellipsis_id = id;
	dasprintf (dstr, "%*st_ellipsis [label=\"...\"];\n", indent, "");
}

static void
print_function (dstring_t *dstr, const type_t *t, int level, int id)
{
	int         indent = level * 2 + 2;
	const ty_func_t *func = &t->func;
	const type_t *ret = func->ret_type;
	const type_t *param;

	dot_print_type (dstr, ret, level + 1, id);
	if (func->num_params < 0) {
		for (int i = 0; i < ~func->num_params; i++) {
			param = func->param_types[i];
			dot_print_type (dstr, param, level + 1, id);
		}
		print_ellipsis (dstr, level, id);
	} else {
		for (int i = 0; i < func->num_params; i++) {
			param = func->param_types[i];
			dot_print_type (dstr, param, level + 1, id);
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
print_basic (dstring_t *dstr, const type_t *t, int level, int id)
{
	if (t->type == ev_ptr || t->type == ev_field) {
		print_pointer (dstr, t, level, id);
	} else if (t->type == ev_func) {
		print_function (dstr, t, level, id);
	} else {
		int         indent = level * 2 + 2;
		dasprintf (dstr, "%*st_%p [label=\"%s\"];\n", indent, "", t, t->name);
	}
}

static void
print_struct (dstring_t *dstr, const type_t *t, int level, int id)
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
			if (sym->sy_type != sy_var) {
				continue;
			}
			dot_print_type (dstr, sym->type, level, id);
		}
		for (pnum = 0, sym = symtab->symbols; sym; sym = sym->next) {
			if (sym->sy_type != sy_var) {
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
			if (sym->sy_type != sy_var) {
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
print_array (dstring_t *dstr, const type_t *t, int level, int id)
{
	int         indent = level * 2 + 2;
	auto type = t->array.type;

	dot_print_type (dstr, type, level, id);
	dasprintf (dstr, "%*st_%p -> \"t_%p\";\n", indent, "", t, type);
	if (t->array.base) {
		dasprintf (dstr, "%*st_%p [label=\"[%d..%d]\"];\n", indent, "", t,
				   t->array.base, t->array.base + t->array.size - 1);
	} else {
		dasprintf (dstr, "%*st_%p [label=\"[%d]\"];\n", indent, "", t,
				   t->array.size);
	}
}

static void
print_class (dstring_t *dstr, const type_t *t, int level, int id)
{
	int         indent = level * 2 + 2;
	dasprintf (dstr, "%*st_%p [label=\"class '%s'\"];\n", indent, "", t,
			   t->class->name);
}

static void
print_alias (dstring_t *dstr, const type_t *t, int level, int id)
{
	int         indent = level * 2 + 2;
	auto aux = t->alias.aux_type;
	auto full = t->alias.full_type;

	dot_print_type (dstr, aux, level, id);
	dot_print_type (dstr, full, level, id);
	dasprintf (dstr, "%*st_%p -> \"t_%p\";\n", indent, "", t, aux);
	dasprintf (dstr, "%*st_%p -> \"t_%p\";\n", indent, "", t, full);
	dasprintf (dstr, "%*st_%p [label=\"alias '%s'\"];\n", indent, "", t,
			   t->name);
}

static void
dot_print_type (dstring_t *dstr, const type_t *t, int level, int id)
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
	if (t->printid == id)		// already printed this type
		return;
	((type_t *) t)->printid = id;

	if ((unsigned) t->meta >= sizeof (print_funcs) / sizeof (print_funcs[0])) {
		dasprintf (dstr, "%*se_%p [label=\"(bad type meta)\\n%d\"];\n",
				   indent, "", t, t->meta);
		return;
	}
	print_funcs [t->meta] (dstr, t, level, id);
}

void
dump_dot_type (void *_t, const char *filename)
{
	static int  id = 0;
	dstring_t  *dstr = dstring_newstr ();
	const type_t *t = _t;

	dasprintf (dstr, "digraph type_%p {\n", t);
	dasprintf (dstr, "  graph [label=\"%s\"];\n", quote_string (filename));
	dasprintf (dstr, "  layout=dot; rankdir=TB; compound=true;\n");
	dot_print_type (dstr, t, 0, ++id);
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
}
