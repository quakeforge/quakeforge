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

#include <QF/dstring.h>
#include <QF/hash.h>
#include <QF/pr_obj.h>
#include <QF/sys.h>
#include <QF/va.h>

#include "qfcc.h"
#include "struct.h"
#include "type.h"

typedef struct {
	const char *name;
	type_t     *type;
} struct_t;

typedef struct {
	const char *name;
	expr_t      value;
} enum_t;

static hashtab_t *structs;
static hashtab_t *enums;

static const char *
structs_get_key (void *s, void *unused)
{
	return ((struct_t *) s)->name;
}

static const char *
struct_field_get_key (void *f, void *unused)
{
	return ((struct_field_t *) f)->name;
}

static const char *
enums_get_key (void *e, void *unused)
{
	return ((enum_t *) e)->name;
}

struct_field_t *
new_struct_field (type_t *strct, type_t *type, const char *name,
				  visibility_t visibility)
{
	struct_field_t *field;

	if (!strct)
		return 0;
	field = malloc (sizeof (struct_field_t));
	field->visibility = visibility;
	field->name = name;
	field->type = type;
	field->offset = strct->num_parms;
	strct->num_parms += type_size (type);
	field->next = 0;
	*strct->struct_tail = field;
	strct->struct_tail = &field->next;
	if (name)
		Hash_Add (strct->struct_fields, field);
	return field;
}

struct_field_t *
struct_find_field (type_t *strct, const char *name)
{
	if (!structs || !strct)
		return 0;
	return Hash_Find (strct->struct_fields, name);
}

type_t *
new_struct (const char *name)
{
	struct_t   *strct;

	if (!structs) {
		structs = Hash_NewTable (16381, structs_get_key, 0, 0);
	}
	if (name) {
		strct = (struct_t *) Hash_Find (structs, name);
		if (strct) {
			error (0, "duplicate struct definition");
			return 0;
		}
	}
	strct = malloc (sizeof (struct_t));
	strct->name = name;
	strct->type = calloc (1, sizeof (type_t));
	strct->type->type = ev_struct;
	strct->type->struct_tail = &strct->type->struct_head;
	strct->type->struct_fields = Hash_NewTable (61, struct_field_get_key, 0, 0);
	if (name)
		Hash_Add (structs, strct);
	return strct->type;
}

type_t *
find_struct (const char *name)
{
	struct_t   *strct;

	if (!structs)
		return 0;
	strct = (struct_t *) Hash_Find (structs, name);
	if (strct)
		return strct->type;
	return 0;
}

void
copy_struct_fields (type_t *dst, type_t *src)
{
	struct_field_t *s;

	if (!src)
		return;
	for (s = src->struct_head; s; s = s->next)
		new_struct_field (dst, s->type, s->name, s->visibility);
}

int
struct_compare_fields (struct type_s *s1, struct type_s *s2)
{
	struct_field_t *f1 = s1->struct_head;
	struct_field_t *f2 = s2->struct_head;

	while (f1 && f2) {
		if (strcmp (f1->name, f2->name)
			|| f1->type != f2->type)
			return 0;
		f1 = f1->next;
		f2 = f2->next;
	}
	return !((!f1) ^ !(f2));
}

int
emit_struct(type_t *strct, const char *name)
{
	struct_field_t *field;
	int         i, count;
	def_t      *ivars_def;
	pr_ivar_list_t *ivars;
	type_t     *ivar_list;
	dstring_t  *encoding = dstring_newstr ();
	dstring_t  *ivars_name = dstring_newstr ();

	if (!strct)
		return 0;
	for (count = 0, field = strct->struct_head; field; field = field->next)
		if (field->name)
			count++;
	ivar_list = new_struct (0);
	new_struct_field (ivar_list, &type_integer, "ivar_count", vis_public);
	for (i = 0; i < count; i++)
		new_struct_field (ivar_list, type_ivar, 0, vis_public);
	dsprintf (ivars_name, "_OBJ_INSTANCE_VARIABLES_%s", name);
	ivars_def = PR_GetDef (ivar_list, ivars_name->str, 0, 0);
	if (ivars_def)
		goto done;
	ivars_def = PR_GetDef (ivar_list, ivars_name->str, 0, &numpr_globals);
	ivars_def->initialized = ivars_def->constant = 1;
	ivars = &G_STRUCT (pr_ivar_list_t, ivars_def->ofs);
	ivars->ivar_count = count;
	for (i = 0, field = strct->struct_head; field; field = field->next) {
		if (!field->name)
			continue;
		encode_type (encoding, field->type);
		ivars->ivar_list[i].ivar_name = ReuseString (field->name);
		ivars->ivar_list[i].ivar_type = ReuseString (encoding->str);
		ivars->ivar_list[i].ivar_offset = field->offset;
		dstring_clearstr (encoding);
		i++;
	}
  done:
	dstring_delete (encoding);
	dstring_delete (ivars_name);
	return ivars_def->ofs;
}

void
process_enum (expr_t *enm)
{
	expr_t     *e = enm;
	expr_t     *c_enum = 0;
	int         enum_val = 0;

	if (!enums) {
		enums = Hash_NewTable (16381, enums_get_key, 0, 0);
	}
	while (e) {
		expr_t     *t = e->next;
		e->next = c_enum;
		c_enum = e;
		e = t;
	}
	for (e = c_enum; e; e = e->next) {
		expr_t     *name = e;
		expr_t     *val = 0;
		enum_t     *new_enum;

		if (name->type == ex_expr) {
			val = name->e.expr.e2;
			name = name->e.expr.e1;
		}
		if ((structs && find_struct (name->e.string_val))
			|| get_enum (name->e.string_val)
			|| PR_GetDef (NULL, name->e.string_val, 0, 0)) {
			error (name, "%s redeclared", name->e.string_val);
			continue;
		}
		if (val)
			enum_val = val->e.integer_val;
		new_enum = malloc (sizeof (enum_t));
		if (!new_enum)
			Sys_Error ("out of memory");
		new_enum->name = name->e.string_val;
		new_enum->value.type = ex_integer;
		new_enum->value.e.integer_val = enum_val++;
		Hash_Add (enums, new_enum);
		//printf ("%s = %d\n", new_enum->name, new_enum->value.e.integer_val);
	}
}

expr_t *
get_enum (const char *name)
{
	enum_t     *e;

	if (!enums)
		return 0;
	e = (enum_t *) Hash_Find (enums, name);
	if (!e)
		return 0;
	return &e->value;
}
