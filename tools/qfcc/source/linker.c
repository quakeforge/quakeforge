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
#include "obj_file.h"
#include "qfcc.h"
#include "strpool.h"

static hashtab_t *extern_defs;
static hashtab_t *defined_defs;
static strpool_t *strings;
static strpool_t *type_strings;

static const char *
defs_get_key (void *_def, void *unused)
{
	qfo_def_t  *def = (qfo_def_t *) _def;

	return G_GETSTR (def->name);
}

void
add_code (qfo_t *qfo)
{
}

void
add_defs (qfo_t *qfo)
{
	qfo_def_t  *def;
	qfo_def_t  *d;

	for (def = qfo->defs; def - qfo->defs < qfo->num_defs; def++) {
		def->full_type = strpool_addstr (type_strings,
										 qfo->strings + def->full_type);
		def->name      = strpool_addstr (strings, qfo->strings + def->name);
		def->file      = strpool_addstr (strings, qfo->strings + def->file);
		if (def->flags & QFOD_EXTERNAL) {
			Hash_Add (extern_defs, def);
		} else {
			if (def->flags & QFOD_GLOBAL) {
				if ((d = Hash_Find (defined_defs, G_GETSTR (def->name)))) {
					pr.source_file = def->file;
					pr.source_line = def->line;
					error (0, "%s redefined", G_GETSTR (def->name));
				}
			}
			if (def->basic_type == ev_string && def->ofs
				&& QFO_var (qfo, string, def->ofs)) {
				string_t    s;
				s = strpool_addstr (strings, QFO_STRING (qfo, def->ofs));
				QFO_var (qfo, string, def->ofs) = s;
			}
			if (def->ofs)
				def->ofs += pr.near_data->size;
			if (def->flags & QFOD_GLOBAL) {
				while ((d = Hash_Find (extern_defs, G_GETSTR (def->name)))) {
					Hash_Del (extern_defs, G_GETSTR (d->name));
					if (d->full_type != def->full_type) {
						pr.source_file = def->file;
						pr.source_line = def->line;
						error (0, "type mismatch %s %s",
							   G_GETSTR (def->full_type),
							   G_GETSTR (d->full_type));
					}
				}
				Hash_Add (defined_defs, def);
			}
		}
	}
}

void
add_functions (qfo_t *qfo)
{
	qfo_function_t *func;

	for (func = qfo->functions; func - qfo->functions < qfo->num_functions;
		 func++) {
		func->name = strpool_addstr (strings, qfo->strings + func->name);
		func->file = strpool_addstr (strings, qfo->strings + func->file);
		if (func->code)
			func->code += pr.code->size;
	}
}

void
linker_begin (void)
{
	extern_defs = Hash_NewTable (16381, defs_get_key, 0, 0);
	defined_defs = Hash_NewTable (16381, defs_get_key, 0, 0);
	strings = strpool_new ();
	type_strings = strpool_new ();
}

void
linker_add_object_file (const char *filename)
{
	qfo_t      *qfo;

	qfo = read_obj_file (filename);
	if (!qfo)
		return;  
	puts(filename);
	add_defs (qfo);
	add_functions (qfo);
	add_code (qfo);
	//add_data (qfo);
	//add_far_data (qfo);
	//add_strings (qfo);
}

void
linker_finish (void)
{
	qfo_def_t **undef_defs, **def;
	undef_defs = (qfo_def_t **) Hash_GetList (extern_defs);
	for (def = undef_defs; *def; def++) {
		pr.source_file = (*def)->file;
		pr.source_line = (*def)->line;
		error (0, "undefined symbol %s", G_GETSTR ((*def)->name));
	}
}
