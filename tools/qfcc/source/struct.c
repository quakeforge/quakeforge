/*
	struct.c

	structure support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/12/08

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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include <QF/dstring.h>
#include <QF/hash.h>
#include <QF/pr_obj.h>
#include <QF/sys.h>
#include <QF/va.h>

#include "def.h"
#include "emit.h"
#include "expr.h"
#include "immediate.h"
#include "qfcc.h"
#include "reloc.h"
#include "struct.h"
#include "type.h"

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
new_struct_field (struct_t *strct, type_t *type, const char *name,
				  visibility_type visibility)
{
	struct_field_t *field;

	if (!strct)
		return 0;
	field = calloc (sizeof (struct_field_t), 1);
	field->visibility = visibility;
	if (name)
		field->name = save_string (name);
	field->type = type;
	if (strct->stype == str_union) {
		int         size = type_size (type);
		field->offset = 0;
		if (size > strct->size)
			strct->size = size;
	} else {
		field->offset = strct->size;;
		strct->size += type_size (type);
	}
	field->next = 0;
	*strct->struct_tail = field;
	strct->struct_tail = &field->next;
	if (name)
		Hash_Add (strct->struct_fields, field);
	return field;
}

struct_field_t *
struct_find_field (struct_t *strct, const char *name)
{
	if (!structs || !strct)
		return 0;
	return Hash_Find (strct->struct_fields, name);
}

type_t *
init_struct (struct_t *strct, type_t *type, struct_type stype,
			 const char *name)
{
	if (name)
		strct->name = save_string (name);
	strct->type = type;
	strct->type->type = ev_struct;
	strct->struct_tail = &strct->struct_head;
	strct->struct_fields = Hash_NewTable (61, struct_field_get_key, 0, 0);
	strct->type->s.strct = strct;
	strct->stype = stype;
	if (name) {
		strct->type->name = strct->name;
		Hash_Add (structs, strct);
	}
	return strct->type;
}

struct_t *
get_struct (const char *name, int create)
{
	struct_t   *s;

	if (!structs)
		structs = Hash_NewTable (16381, structs_get_key, 0, 0);
	if (name) {
		s = Hash_Find (structs, name);
		if (s || !create)
			return s;
	}
	s = calloc (sizeof (struct_t), 1);
	if (name)
		s->name = save_string (name);
	s->return_addr = __builtin_return_address (0);
	if (name)
		Hash_Add (structs, s);
	return s;
}

int
struct_compare_fields (struct_t *s1, struct_t *s2)
{
	struct_field_t *f1 = s1->struct_head;
	struct_field_t *f2 = s2->struct_head;

	while (f1 && f2) {
		if (f1->name != f2->name)
			if (!f1->name || !f2->name
				|| strcmp (f1->name, f2->name)
				|| f1->type != f2->type)
				return 0;
		f1 = f1->next;
		f2 = f2->next;
	}
	return !((!f1) ^ !(f2));
}

static struct_t *
start_struct (const char *name, struct_type stype)
{
	struct_t   *strct = get_struct (name, 1);
	if (strct->struct_head) {
		error (0, "%s redeclared", name);
		goto err;
	}
	if (strct->stype != str_none && strct->stype != stype) {
		error (0, "%s defined as wrong kind of tag", name);
		goto err;
	}
	if (!strct->type)
		init_struct (strct, new_type (), stype, 0);
	return strct;
err:
	strct = get_struct (0, 0);
	init_struct (strct, new_type (), stype, 0);
	return strct;
}

struct_t *
new_struct (const char *name)
{
	return start_struct (name, str_struct);
}

struct_t *
new_union (const char *name)
{
	return start_struct (name, str_union);
}

static struct_t *
check_struct (const char *name, struct_type stype)
{
	struct_t   *strct = get_struct (name, 0);

	if (!strct)
		return start_struct (name, stype);
	if (strct->stype != stype) {
		error (0, "%s defined as wrong kind of tag", name);
		strct = get_struct (0, 0);
		init_struct (strct, new_type (), stype, 0);
	}
	return strct;
}

struct_t *
decl_struct (const char *name)
{

	return check_struct (name, str_struct);
}

struct_t *
decl_union (const char *name)
{
	return check_struct (name, str_union);
}

def_t *
emit_struct(struct_t *strct, const char *name)
{
	struct_field_t *field;
	int         i, count;
	def_t      *ivars_def;
	pr_ivar_list_t *ivars;
	struct_t   *ivar_list;
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
	ivars_def = get_def (ivar_list->type, ivars_name->str, pr.scope, st_none);
	if (ivars_def)
		goto done;
	ivars_def = get_def (ivar_list->type, ivars_name->str, pr.scope,
						 st_static);
	ivars_def->initialized = ivars_def->constant = 1;
	ivars_def->nosave = 1;
	ivars = &G_STRUCT (pr_ivar_list_t, ivars_def->ofs);
	ivars->ivar_count = count;
	for (i = 0, field = strct->struct_head; field; field = field->next) {
		if (!field->name)
			continue;
		encode_type (encoding, field->type);
		EMIT_STRING (ivars->ivar_list[i].ivar_name, field->name);
		EMIT_STRING (ivars->ivar_list[i].ivar_type, encoding->str);
		ivars->ivar_list[i].ivar_offset = field->offset;
		dstring_clearstr (encoding);
		i++;
	}
  done:
	dstring_delete (encoding);
	dstring_delete (ivars_name);
	return ivars_def;
}

void
clear_structs (void)
{
	if (structs)
		Hash_FlushTable (structs);
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
		if (get_enum (name->e.string_val)
			|| get_def (NULL, name->e.string_val, pr.scope, st_none)) {
			error (name, "%s redeclared", name->e.string_val);
			continue;
		}
		if (val)
			enum_val = val->e.integer_val;
		new_enum = malloc (sizeof (enum_t));
		if (!new_enum)
			Sys_Error ("out of memory");
		new_enum->name = name->e.string_val;
		new_enum->value = enum_val++;
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
	return new_integer_expr (e->value);
}

void
clear_enums (void)
{
	if (enums)
		Hash_FlushTable (enums);
}
