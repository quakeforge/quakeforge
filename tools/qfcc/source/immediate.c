/*
	immediate.c

	shared immediate value handling

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/06/04

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
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/va.h"

#include "qfcc.h"
#include "def.h"
#include "emit.h"
#include "expr.h"
#include "immediate.h"
#include "reloc.h"
#include "strpool.h"
#include "type.h"

typedef struct {
	def_t      *def;
	union {
		string_t    string_val;
		float       float_val;
		float       vector_val[3];
		int         entity_val;
		int         field_val;
		int         func_val;
		ex_pointer_t pointer;
		float       quaternion_val[4];
		int         integer_val;
	} i;
} immediate_t;

static hashtab_t *string_imm_defs;
static hashtab_t *float_imm_defs;
static hashtab_t *vector_imm_defs;
static hashtab_t *entity_imm_defs;
static hashtab_t *field_imm_defs;
static hashtab_t *func_imm_defs;
static hashtab_t *pointer_imm_defs;
static hashtab_t *quaternion_imm_defs;
static hashtab_t *integer_imm_defs;

static uintptr_t
imm_get_hash (void *_imm, void *_tab)
{
	immediate_t *imm = (immediate_t *) _imm;
	hashtab_t  **tab = (hashtab_t **) _tab;

	if (tab == &string_imm_defs) {
		const char *str = pr.strings->strings + imm->i.string_val;
		return str ? Hash_String (str) : 0;
	} else if (tab == &float_imm_defs) {
		return imm->i.integer_val;
	} else if (tab == &vector_imm_defs) {
		return Hash_Buffer (&imm->i.vector_val, sizeof (&imm->i.vector_val));
	} else if (tab == &entity_imm_defs) {
		return imm->i.integer_val;
	} else if (tab == &field_imm_defs) {
		return imm->i.integer_val;
	} else if (tab == &func_imm_defs) {
		return imm->i.integer_val;
	} else if (tab == &pointer_imm_defs) {
		return Hash_Buffer (&imm->i.pointer, sizeof (&imm->i.pointer));
	} else if (tab == &quaternion_imm_defs) {
		return Hash_Buffer (&imm->i.quaternion_val,
							sizeof (&imm->i.quaternion_val));
	} else if (tab == &integer_imm_defs) {
		return imm->i.integer_val;
	} else {
		abort ();
	}
}

static int
imm_compare (void *_imm1, void *_imm2, void *_tab)
{
	immediate_t *imm1 = (immediate_t *) _imm1;
	immediate_t *imm2 = (immediate_t *) _imm2;
	hashtab_t  **tab = (hashtab_t **) _tab;

	if (tab == &string_imm_defs) {
		const char *str1 = pr.strings->strings + imm1->i.string_val;
		const char *str2 = pr.strings->strings + imm2->i.string_val;
		return (str1 == str2 || (str1 && str2 && !strcmp (str1, str2)));
	} else if (tab == &float_imm_defs) {
		return imm1->i.float_val == imm2->i.float_val;
	} else if (tab == &vector_imm_defs) {
		return VectorCompare (imm1->i.vector_val, imm2->i.vector_val);
	} else if (tab == &entity_imm_defs) {
		return imm1->i.entity_val == imm2->i.entity_val;
	} else if (tab == &field_imm_defs) {
		return imm1->i.field_val == imm2->i.field_val;
	} else if (tab == &func_imm_defs) {
		return imm1->i.func_val == imm2->i.func_val;
	} else if (tab == &pointer_imm_defs) {
		return !memcmp (&imm1->i.pointer, &imm2->i.pointer,
						sizeof (imm1->i.pointer));
	} else if (tab == &quaternion_imm_defs) {
		return (VectorCompare (imm1->i.quaternion_val,
							   imm2->i.quaternion_val)
				&& imm1->i.quaternion_val[3] == imm2->i.quaternion_val[3]);
	} else if (tab == &integer_imm_defs) {
		return imm1->i.integer_val == imm2->i.integer_val;
	} else {
		abort ();
	}
}

int
ReuseString (const char *str)
{
	return strpool_addstr (pr.strings, str);
}

def_t *
ReuseConstant (expr_t *expr, def_t *def)
{
	def_t      *cn;
	hashtab_t  *tab = 0;
	type_t     *type;
	expr_t      e = *expr;
	reloc_t    *reloc = 0;
	immediate_t *imm, search;

	if (!string_imm_defs) {
		clear_immediates ();
	}
	cn = 0;
	switch (e.type) {
		case ex_entity:
			tab = entity_imm_defs;
			type = &type_entity;
			break;
		case ex_field:
			tab = field_imm_defs;
			type = &type_field;
			break;
		case ex_func:
			tab = func_imm_defs;
			type = &type_function;
			break;
		case ex_pointer:
			tab = pointer_imm_defs;
			type = &type_pointer;
			break;
		case ex_integer:
		case ex_uinteger:
			if (!def || def->type != &type_float) {
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
			e.type = ex_float;
		case ex_float:
			tab = float_imm_defs;
			type = &type_float;
			break;
		case ex_string:
			e.e.integer_val = ReuseString (e.e.string_val);
			tab = string_imm_defs;
			type = &type_string;
			break;
		case ex_vector:
			tab = vector_imm_defs;
			type = &type_vector;
			break;
		case ex_quaternion:
			tab = quaternion_imm_defs;
			type = &type_quaternion;
			break;
		default:
			abort ();
	}
	memcpy (&search.i, &e.e, sizeof (search.i));
	imm = (immediate_t *) Hash_FindElement (tab, &search);
	if (imm && strcmp (imm->def->name, ".zero") == 0) {
		if (def) {
			imm = 0;	//FIXME do full def aliasing
		} else {
			expr_t     *e = new_def_expr (imm->def);
			e = address_expr (e, 0, type);
			e = unary_expr ('.', e);
			return emit_sub_expr (e, 0);
		}
	}
	if (imm) {
		cn = imm->def;
		if (def) {
			free_location (def);
			def->ofs = cn->ofs;
			def->initialized = def->constant = 1;
			def->nosave = 1;
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
	}
	cn->initialized = cn->constant = 1;
	cn->nosave = 1;
	// copy the immediate to the global area
	switch (e.type) {
		case ex_string:
			reloc = new_reloc (cn->ofs, rel_def_string);
			break;
		case ex_func:
			if (e.e.func_val)
				reloc = new_reloc (cn->ofs, rel_def_func);
			break;
		case ex_field:
			if (e.e.pointer.def)
				reloc_def_field_ofs (e.e.pointer.def, cn->ofs);
			break;
		case ex_pointer:
			if (e.e.pointer.def) {
				EMIT_DEF_OFS (G_INT (cn->ofs), e.e.pointer.def);
				e.e.pointer.def->users--;
			}
			break;
		default:
			break;
	}
	if (reloc) {
		reloc->next = pr.relocs;
		pr.relocs = reloc;
	}

	memcpy (G_POINTER (void, cn->ofs), &e.e, 4 * type_size (type));

	imm = malloc (sizeof (immediate_t));
	imm->def = cn;
	memcpy (&imm->i, &e.e, sizeof (imm->i));

	Hash_AddElement (tab, imm);

	return cn;
}

void
clear_immediates (void)
{
	immediate_t *imm;

	if (string_imm_defs) {
		Hash_FlushTable (string_imm_defs);
		Hash_FlushTable (float_imm_defs);
		Hash_FlushTable (vector_imm_defs);
		Hash_FlushTable (entity_imm_defs);
		Hash_FlushTable (field_imm_defs);
		Hash_FlushTable (func_imm_defs);
		Hash_FlushTable (pointer_imm_defs);
		Hash_FlushTable (quaternion_imm_defs);
		Hash_FlushTable (integer_imm_defs);
	} else {
		string_imm_defs = Hash_NewTable (16381, 0, 0, &string_imm_defs);
		Hash_SetHashCompare (string_imm_defs, imm_get_hash, imm_compare);

		float_imm_defs = Hash_NewTable (16381, 0, 0, &float_imm_defs);
		Hash_SetHashCompare (float_imm_defs, imm_get_hash, imm_compare);

		vector_imm_defs = Hash_NewTable (16381, 0, 0, &vector_imm_defs);
		Hash_SetHashCompare (vector_imm_defs, imm_get_hash, imm_compare);

		entity_imm_defs = Hash_NewTable (16381, 0, 0, &entity_imm_defs);
		Hash_SetHashCompare (entity_imm_defs, imm_get_hash, imm_compare);

		field_imm_defs = Hash_NewTable (16381, 0, 0, &field_imm_defs);
		Hash_SetHashCompare (field_imm_defs, imm_get_hash, imm_compare);

		func_imm_defs = Hash_NewTable (16381, 0, 0, &func_imm_defs);
		Hash_SetHashCompare (func_imm_defs, imm_get_hash, imm_compare);

		pointer_imm_defs = Hash_NewTable (16381, 0, 0, &pointer_imm_defs);
		Hash_SetHashCompare (pointer_imm_defs, imm_get_hash, imm_compare);

		quaternion_imm_defs =
			Hash_NewTable (16381, 0, 0, &quaternion_imm_defs);
		Hash_SetHashCompare (quaternion_imm_defs, imm_get_hash, imm_compare);

		integer_imm_defs = Hash_NewTable (16381, 0, 0, &integer_imm_defs);
		Hash_SetHashCompare (integer_imm_defs, imm_get_hash, imm_compare);
	}

	imm = calloc (1, sizeof (immediate_t));
	imm->def = get_def (&type_zero, ".zero", pr.scope, st_extern);
	imm->def->nosave = 1;

	Hash_AddElement (string_imm_defs, imm);
	Hash_AddElement (float_imm_defs, imm);
	Hash_AddElement (entity_imm_defs, imm);
	Hash_AddElement (pointer_imm_defs, imm);
	Hash_AddElement (integer_imm_defs, imm);
}
