/*
	debug.c

	debug info support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/7/14

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
#include <ctype.h>

#include "QF/alloc.h"
#include "QF/progs/pr_comp.h"

#include "tools/qfcc/include/cpp.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

int         lineno_base;

ALLOC_STATE (srcline_t, srclines);

static void
push_source_file (void)
{
	srcline_t  *srcline;
	ALLOC (16, srcline_t, srclines, srcline);
	srcline->loc = pr.loc;
	srcline->next = pr.srcline_stack;
	pr.srcline_stack = srcline;
}

static void
pop_source_file (void)
{
	srcline_t  *tmp;

	if (!pr.srcline_stack) {
		notice (0, "unbalanced #includes. bug in preprocessor?");
		return;
	}
	tmp = pr.srcline_stack;
	pr.loc = tmp->loc;
	pr.srcline_stack = tmp->next;
	FREE (srclines, tmp);
}

#define Sys_Error(fmt...) internal_error(0, fmt)
void
add_source_file (const char *file)
{
	pr.loc.file = ReuseString (file);
	if (!strpool_findstr (pr.comp_file_set, file)) {
		strpool_addstr (pr.comp_file_set, file);
		DARRAY_APPEND (&pr.comp_files, save_string (file));
	}
}

void
set_line_file (int line, const char *file, int flags)
{
	switch (flags & 3) {
		case 1:
			push_source_file ();
			break;
		case 2:
			pop_source_file ();
			file = GETSTR (pr.loc.file);
			line = pr.loc.line;
			break;
	}
	pr.loc = (rua_loc_t) {
		.line = line,
		.column = 1,
		.last_line = line,
		.last_column = 1,
	};
	if (file) {
		add_source_file (file);
		cpp_set_quote_file (file);
	}
}

pr_lineno_t *
new_lineno (void)
{
	if (pr.num_linenos == pr.linenos_size) {
		pr.linenos_size += 1024;
		pr.linenos = realloc (pr.linenos,
							  pr.linenos_size * sizeof (pr_lineno_t));
	}
	memset (&pr.linenos[pr.num_linenos], 0, sizeof (pr_lineno_t));
	return &pr.linenos[pr.num_linenos++];
}

static void
emit_unit_name (def_t *def, void *data, int index)
{
	if (!is_string (def->type)) {
		internal_error (0, "%s: expected string def", __FUNCTION__);
	}
	EMIT_STRING (def->space, D_STRING (def), pr.unit_name);
}

static void
emit_basedir (def_t *def, void *data, int index)
{
	if (!is_string (def->type)) {
		internal_error (0, "%s: expected string def", __FUNCTION__);
	}
	EMIT_STRING (def->space, D_STRING (def), pr.comp_dir);
}

static void
emit_num_files (def_t *def, void *data, int index)
{
	if (!is_int (def->type)) {
		internal_error (0, "%s: expected int def", __FUNCTION__);
	}
	D_INT (def) = pr.comp_files.size;
}

static void
emit_files_item (def_t *def, void *data, int index)
{
	if (!is_array (def->type) || !is_string (dereference_type (def->type))) {
		internal_error (0, "%s: expected array of string def", __FUNCTION__);
	}
	if ((unsigned) index >= pr.comp_files.size) {
		internal_error (0, "%s: out of bounds index: %d %zd",
						__FUNCTION__, index, pr.comp_files.size);
	}
	EMIT_STRING (def->space, D_STRING (def), pr.comp_files.a[index]);
}

static def_t *
emit_compunit (const char *modname)
{
	static struct_def_t compunit_struct[] = {
		{"unit_name",	&type_string,	emit_unit_name},
		{"basedir",		&type_string,	emit_basedir},
		{"num_files",	&type_int,		emit_num_files},
		{"files",		0,				emit_files_item},
		{0, 0}
	};
	int         count = pr.comp_files.size;

	pr.unit_name = modname;
	compunit_struct[3].type = array_type (&type_string, count);
	return emit_structure (".compile_unit", 's', compunit_struct, 0, &pr,
						   pr.debug_data, sc_static);
}

void
debug_finish_module (const char *modname)
{
	emit_compunit (modname);
}
