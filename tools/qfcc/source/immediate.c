/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
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

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/va.h"

#include "qfcc.h"
#include "def.h"
#include "expr.h"
#include "immediate.h"
#include "type.h"

static hashtab_t *string_imm_defs;
static hashtab_t *float_imm_defs;
static hashtab_t *vector_imm_defs;
static hashtab_t *entity_imm_defs;
static hashtab_t *field_imm_defs;
static hashtab_t *func_imm_defs;
static hashtab_t *pointer_imm_defs;
static hashtab_t *quaternion_imm_defs;
static hashtab_t *integer_imm_defs;
static hashtab_t *strings_tab;

static const char *
string_imm_get_key (void *_def, void *unused)
{
	def_t      *def = (def_t *) _def;

	return G_STRING (def->ofs);
}

static const char *
float_imm_get_key (void *_def, void *unused)
{
	def_t      *def = (def_t *) _def;
	static char rep[20];

	return va ("\001float:%08X\001", G_INT (def->ofs));
	return rep;
}

static const char *
vector_imm_get_key (void *_def, void *unused)
{
	def_t      *def = (def_t *) _def;
	static char rep[60];

	return va ("\001vector:%08X\001%08X\001%08X\001",
			  G_INT (def->ofs), G_INT (def->ofs + 1), G_INT (def->ofs + 2));
	return rep;
}

static const char *
quaternion_imm_get_key (void *_def, void *unused)
{
	def_t      *def = (def_t *) _def;
	static char rep[60];

	return va ("\001quaternion:%08X\001%08X\001%08X\001%08X\001",
			   G_INT (def->ofs), G_INT (def->ofs + 1),
			   G_INT (def->ofs + 2), G_INT (def->ofs + 3));
	return rep;
}

static const char *
int_imm_get_key (void *_def, void *_str)
{
	def_t      *def = (def_t *) _def;
	static char rep[60];
	char       *str = (char *) _str;

	return va ("\001%s:%08X\001", str, G_INT (def->ofs));
	return rep;
}

static const char *
strings_get_key (void *_str, void *unsued)
{
	return pr.strings + (int) _str;
}

int
CopyString (const char *str)
{
	int         old;
	int         len = strlen (str) + 1;

	if (!strings_tab) {
		strings_tab = Hash_NewTable (16381, strings_get_key, 0, 0);
	}
	if (pr.strofs + len >= pr.strings_size) {
		pr.strings_size += (len + 16383) & ~16383;
		pr.strings = realloc (pr.strings, pr.strings_size);
	}
	old = pr.strofs;
	strcpy (pr.strings + pr.strofs, str);
	pr.strofs += len;
	Hash_Add (strings_tab, (void *)old);
	return old;
}

int
ReuseString (const char *str)
{
	int         s;

	if (!*str)
		return 0;
	if (!strings_tab)
		return CopyString (str);
	s = (long) Hash_Find (strings_tab, str);
	if (s)
		return s;
	return CopyString (str);
}

def_t *
ReuseConstant (expr_t *expr, def_t *def)
{
	def_t      *cn;
	static dstring_t*rep = 0;
	hashtab_t  *tab = 0;
	type_t     *type;
	expr_t      e = *expr;

	if (!rep)
		rep = dstring_newstr ();
	if (!string_imm_defs) {
		string_imm_defs = Hash_NewTable (16381, string_imm_get_key, 0, 0);
		float_imm_defs = Hash_NewTable (16381, float_imm_get_key, 0, 0);
		vector_imm_defs = Hash_NewTable (16381, vector_imm_get_key, 0, 0);
		entity_imm_defs = Hash_NewTable (16381, int_imm_get_key, 0, "entity");
		field_imm_defs = Hash_NewTable (16381, int_imm_get_key, 0, "field");
		func_imm_defs = Hash_NewTable (16381, int_imm_get_key, 0, "func");
		pointer_imm_defs = Hash_NewTable (16381, int_imm_get_key, 0, "pointer");
		quaternion_imm_defs =
			Hash_NewTable (16381, quaternion_imm_get_key, 0, 0);
		integer_imm_defs = Hash_NewTable (16381, int_imm_get_key, 0, "integer");

		Hash_Add (string_imm_defs, cn = new_def (&type_string, ".imm",
												   pr.scope));
		cn->initialized = cn->constant = 1;
		Hash_Add (float_imm_defs, cn = new_def (&type_float, ".imm",
												  pr.scope));
		cn->initialized = cn->constant = 1;
		Hash_Add (entity_imm_defs, cn = new_def (&type_entity, ".imm",
												   pr.scope));
		cn->initialized = cn->constant = 1;
		Hash_Add (pointer_imm_defs, cn = new_def (&type_pointer, ".imm",
													pr.scope));
		cn->initialized = cn->constant = 1;
		Hash_Add (integer_imm_defs, cn = new_def (&type_integer, ".imm",
													pr.scope));
		cn->initialized = cn->constant = 1;
	}
	cn = 0;
	switch (e.type) {
		case ex_entity:
			dsprintf (rep, "\001entity:%08X\001", e.e.integer_val);
			tab = entity_imm_defs;
			type = &type_entity;
			break;
		case ex_field:
			dsprintf (rep, "\001field:%08X\001", e.e.integer_val);
			tab = field_imm_defs;
			type = &type_field;
			break;
		case ex_func:
			dsprintf (rep, "\001func:%08X\001", e.e.integer_val);
			tab = func_imm_defs;
			type = &type_function;
			break;
		case ex_pointer:
			dsprintf (rep, "\001pointer:%08X\001", e.e.pointer.val);
			tab = pointer_imm_defs;
			type = &type_pointer;
			break;
		case ex_integer:
		case ex_uinteger:
			if (!def || def->type != &type_float) {
				dsprintf (rep, "\001integer:%08X\001", e.e.integer_val);
				tab = integer_imm_defs;
				if (e.type == ex_uinteger)
					type = &type_uinteger;
				else
					type = &type_integer;
				break;
			}
			if (e.type == ex_uinteger)
				e.e.float_val = e.e.uinteger_val;
			else
				e.e.float_val = e.e.integer_val;
		case ex_float:
			dsprintf (rep, "\001float:%08X\001", e.e.integer_val);
			tab = float_imm_defs;
			type = &type_float;
			break;
		case ex_string:
			dsprintf (rep, "%s", e.e.string_val ? e.e.string_val : "");
			tab = string_imm_defs;
			type = &type_string;
			break;
		case ex_vector:
			dsprintf (rep, "\001vector:%08X\001%08X\001%08X\001",
					  *(int *) &e.e.vector_val[0],
					  *(int *) &e.e.vector_val[1], *(int *) &e.e.vector_val[2]);
			tab = vector_imm_defs;
			type = &type_vector;
			break;
		case ex_quaternion:
			dsprintf (rep, "\001quaternion:%08X\001%08X\001%08X\001%08X\001",
					  *(int *) &e.e.quaternion_val[0],
					  *(int *) &e.e.quaternion_val[1],
					  *(int *) &e.e.quaternion_val[2],
					  *(int *) &e.e.quaternion_val[3]);
			tab = vector_imm_defs;
			type = &type_quaternion;
			break;
		default:
			abort ();
	}
	cn = (def_t *) Hash_Find (tab, rep->str);
	if (cn) {
		if (def) {
			free_location (def);
			def->ofs = cn->ofs;
			def->initialized = def->constant = 1;
			cn = def;
		} else {
			if (cn->type != type) {
				def = new_def (type, ".imm", pr.scope);
				def->ofs = cn->ofs;
				cn = def;
			}
		}
		return cn;
	}
	// allocate a new one
	// always share immediates
	if (def) {
		if (def->type != type) {
			cn = new_def (type, ".imm", pr.scope);
			cn->ofs = def->ofs;
		} else {
			cn = def;
		}
	} else {
		cn = new_def (type, ".imm", pr.scope);
		cn->ofs = new_location (type, pr.near_data);
		if (type == &type_vector || type == &type_quaternion) {
			int         i;

			for (i = 0; i < 3 + (type == &type_quaternion); i++)
				new_def (&type_float, ".imm", pr.scope);
		}
	}
	cn->initialized = cn->constant = 1;
	// copy the immediate to the global area
	if (e.type == ex_string)
		e.e.integer_val = ReuseString (rep->str);

	memcpy (G_POINTER (void, cn->ofs), &e.e, 4 * type_size (type));

	Hash_Add (tab, cn);

	return cn;
}
