/*
	value.c

	value handling

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/va.h"

#include "qfcc.h"
#include "def.h"
#include "defspace.h"
#include "diagnostic.h"
#include "emit.h"
#include "expr.h"
#include "reloc.h"
#include "strpool.h"
#include "symtab.h"
#include "type.h"
#include "value.h"

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
		double      double_val;
	} i;
} immediate_t;

static hashtab_t *value_table;
static ex_value_t *values_freelist;

static uintptr_t
value_get_hash (const void *_val, void *unused)
{
	const ex_value_t *val = (const ex_value_t *) _val;
	return Hash_Buffer (&val->v, sizeof (val->v)) ^ (uintptr_t) val->type;
}

static int
value_compare (const void *_val1, const void *_val2, void *unused)
{
	const ex_value_t *val1 = (const ex_value_t *) _val1;
	const ex_value_t *val2 = (const ex_value_t *) _val2;
	if (val1->type != val2->type)
		return 0;
	return memcmp (&val1->v, &val2->v, sizeof (val1->v)) == 0;
}

static ex_value_t *
new_value (void)
{
	ex_value_t *value;
	ALLOC (256, ex_value_t, values, value);
	return value;
}

static void
set_val_type (ex_value_t *val, type_t *type)
{
	val->type = type;
	val->lltype = low_level_type (type);
}

static ex_value_t *
find_value (const ex_value_t *val)
{
	ex_value_t *value;

	value = Hash_FindElement (value_table, val);
	if (value)
		return value;
	value = new_value ();
	*value = *val;
	Hash_AddElement (value_table, value);
	return value;
}

ex_value_t *
new_string_val (const char *string_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_string);
	if (string_val)
		val.v.string_val = save_string (string_val);
	return find_value (&val);
}

ex_value_t *
new_double_val (double double_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_double);
	val.v.double_val = double_val;
	return find_value (&val);
}

ex_value_t *
new_float_val (float float_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_float);
	val.v.float_val = float_val;
	return find_value (&val);
}

ex_value_t *
new_vector_val (const float *vector_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_vector);
	VectorCopy (vector_val, val.v.vector_val);
	return find_value (&val);
}

ex_value_t *
new_entity_val (int entity_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_entity);
	val.v.entity_val = entity_val;
	return find_value (&val);
}

ex_value_t *
new_field_val (int field_val, type_t *type, def_t *def)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, field_type (type));
	val.v.pointer.val = field_val;
	val.v.pointer.type = type;
	val.v.pointer.def = def;
	return find_value (&val);
}

ex_value_t *
new_func_val (int func_val, type_t *type)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, type);
	val.v.func_val.val = func_val;
	val.v.func_val.type = type;
	return find_value (&val);
}

ex_value_t *
new_pointer_val (int pointer_val, type_t *type, def_t *def,
				 struct operand_s *tempop)
{
	ex_value_t  val;
	if (!type) {
		internal_error (0, "pointer value with no type");
	}
	memset (&val, 0, sizeof (val));
	set_val_type (&val, pointer_type (type));
	val.v.pointer.val = pointer_val;
	val.v.pointer.type = type;
	val.v.pointer.def = def;
	val.v.pointer.tempop = tempop;
	return find_value (&val);
}

ex_value_t *
new_quaternion_val (const float *quaternion_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_quaternion);
	QuatCopy (quaternion_val, val.v.quaternion_val);
	return find_value (&val);
}

ex_value_t *
new_integer_val (int integer_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_integer);
	val.v.integer_val = integer_val;
	return find_value (&val);
}

ex_value_t *
new_uinteger_val (int uinteger_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_uinteger);
	val.v.uinteger_val = uinteger_val;
	return find_value (&val);
}

ex_value_t *
new_short_val (short short_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_short);
	val.v.short_val = short_val;
	return find_value (&val);
}

ex_value_t *
new_nil_val (type_t *type)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, type);
	if (val.lltype == ev_void) {
		val.lltype = type_nil->type;
	}
	if (val.lltype == ev_pointer || val.lltype == ev_field )
		val.v.pointer.type = type->t.fldptr.type;
	if (val.lltype == ev_func)
		val.v.func_val.type = type;
	return find_value (&val);
}

static hashtab_t *string_imm_defs;
static hashtab_t *float_imm_defs;
static hashtab_t *vector_imm_defs;
static hashtab_t *entity_imm_defs;
static hashtab_t *field_imm_defs;
static hashtab_t *func_imm_defs;
static hashtab_t *pointer_imm_defs;
static hashtab_t *quaternion_imm_defs;
static hashtab_t *integer_imm_defs;
static hashtab_t *double_imm_defs;

static void
imm_free (void *_imm, void *unused)
{
	free (_imm);
}

static __attribute__((pure)) uintptr_t
imm_get_hash (const void *_imm, void *_tab)
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
		return Hash_Buffer (&imm->i.pointer, sizeof (&imm->i.pointer));
	} else if (tab == &func_imm_defs) {
		return imm->i.integer_val;
	} else if (tab == &pointer_imm_defs) {
		return Hash_Buffer (&imm->i.pointer, sizeof (&imm->i.pointer));
	} else if (tab == &quaternion_imm_defs) {
		return Hash_Buffer (&imm->i.quaternion_val,
							sizeof (&imm->i.quaternion_val));
	} else if (tab == &double_imm_defs) {
		return Hash_Buffer (&imm->i.double_val, sizeof (&imm->i.double_val));
	} else if (tab == &integer_imm_defs) {
		return imm->i.integer_val;
	} else {
		internal_error (0, 0);
	}
}

static __attribute__((pure)) int
imm_compare (const void *_imm1, const void *_imm2, void *_tab)
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
		return !memcmp (&imm1->i.pointer, &imm2->i.pointer,
						sizeof (imm1->i.pointer));
	} else if (tab == &func_imm_defs) {
		return imm1->i.func_val == imm2->i.func_val;
	} else if (tab == &pointer_imm_defs) {
		return !memcmp (&imm1->i.pointer, &imm2->i.pointer,
						sizeof (imm1->i.pointer));
	} else if (tab == &quaternion_imm_defs) {
		return QuatCompare (imm1->i.quaternion_val, imm2->i.quaternion_val);
	} else if (tab == &double_imm_defs) {
		return imm1->i.double_val == imm2->i.double_val;
	} else if (tab == &integer_imm_defs) {
		return imm1->i.integer_val == imm2->i.integer_val;
	} else {
		internal_error (0, 0);
	}
}

int
ReuseString (const char *str)
{
	return strpool_addstr (pr.strings, str);
}

static float
value_as_float (ex_value_t *value)
{
	if (value->lltype == ev_uinteger)
		return value->v.uinteger_val;
	if (value->lltype == ev_integer)
		return value->v.integer_val;
	if (value->lltype == ev_short)
		return value->v.short_val;
	if (value->lltype == ev_double)
		return value->v.double_val;
	if (value->lltype == ev_float)
		return value->v.float_val;
	return 0;
}

static double
value_as_double (ex_value_t *value)
{
	if (value->lltype == ev_uinteger)
		return value->v.uinteger_val;
	if (value->lltype == ev_integer)
		return value->v.integer_val;
	if (value->lltype == ev_short)
		return value->v.short_val;
	if (value->lltype == ev_double)
		return value->v.double_val;
	if (value->lltype == ev_float)
		return value->v.float_val;
	return 0;
}

static int
value_as_int (ex_value_t *value)
{
	if (value->lltype == ev_uinteger)
		return value->v.uinteger_val;
	if (value->lltype == ev_integer)
		return value->v.integer_val;
	if (value->lltype == ev_short)
		return value->v.short_val;
	if (value->lltype == ev_double)
		return value->v.double_val;
	if (value->lltype == ev_float)
		return value->v.float_val;
	return 0;
}

static unsigned
value_as_uint (ex_value_t *value)
{
	if (value->lltype == ev_uinteger)
		return value->v.uinteger_val;
	if (value->lltype == ev_integer)
		return value->v.integer_val;
	if (value->lltype == ev_short)
		return value->v.short_val;
	if (value->lltype == ev_double)
		return value->v.double_val;
	if (value->lltype == ev_float)
		return value->v.float_val;
	return 0;
}

ex_value_t *
convert_value (ex_value_t *value, type_t *type)
{
	if (!is_scalar (type) || !is_scalar (ev_types[value->lltype])) {
		error (0, "unable to convert non-scalar value");
		return value;
	}
	if (is_float (type)) {
		float       val = value_as_float (value);
		return new_float_val (val);
	} else if (is_double (type)) {
		double      val = value_as_double (value);
		return new_double_val (val);
	} else if (type->type == ev_short) {
		int         val = value_as_int (value);
		return new_short_val (val);
	} else if (type->type == ev_uinteger) {
		unsigned    val = value_as_uint (value);
		return new_uinteger_val (val);
	} else {
		//FIXME handle enums separately?
		int         val = value_as_int (value);
		return new_integer_val (val);
	}
}

ex_value_t *
alias_value (ex_value_t *value, type_t *type)
{
	ex_value_t  new;

	if (type_size (type) != type_size (ev_types[value->lltype])) {
		error (0, "unable to alias different sized values");
		return value;
	}
	new = *value;
	set_val_type (&new, type);
	return find_value (&new);
}

static immediate_t *
make_def_imm (def_t *def, hashtab_t *tab, ex_value_t *val)
{
	immediate_t *imm;

	imm = calloc (1, sizeof (immediate_t));
	imm->def = def;
	memcpy (&imm->i, &val->v, sizeof (imm->i));

	Hash_AddElement (tab, imm);

	return imm;
}

def_t *
emit_value (ex_value_t *value, def_t *def)
{
	def_t      *cn;
	hashtab_t  *tab = 0;
	type_t     *type;
	ex_value_t  val = *value;
	immediate_t *imm, search;

	if (!string_imm_defs) {
		clear_immediates ();
	}
	cn = 0;
//	if (val.type == ev_void)
//		val.type = type_nil->type;
	switch (val.lltype) {
		case ev_entity:
			tab = entity_imm_defs;
			type = &type_entity;
			break;
		case ev_field:
			tab = field_imm_defs;
			type = &type_field;
			break;
		case ev_func:
			tab = func_imm_defs;
			type = &type_function;
			break;
		case ev_pointer:
			tab = pointer_imm_defs;
			type = &type_pointer;
			break;
		case ev_integer:
		case ev_uinteger:
			if (!def || !is_float(def->type)) {
				tab = integer_imm_defs;
				type = &type_integer;
				break;
			}
			val.v.float_val = val.v.integer_val;
			val.lltype = ev_float;
		case ev_float:
			tab = float_imm_defs;
			type = &type_float;
			break;
		case ev_string:
			val.v.integer_val = ReuseString (val.v.string_val);
			tab = string_imm_defs;
			type = &type_string;
			break;
		case ev_vector:
			tab = vector_imm_defs;
			type = &type_vector;
			break;
		case ev_quat:
			tab = quaternion_imm_defs;
			type = &type_quaternion;
			break;
		case ev_double:
			tab = double_imm_defs;
			type = &type_double;
			break;
		default:
			internal_error (0, 0);
	}
	memcpy (&search.i, &val.v, sizeof (search.i));
	imm = (immediate_t *) Hash_FindElement (tab, &search);
	if (imm && strcmp (imm->def->name, ".zero") == 0) {
		if (def) {
			imm = 0;	//FIXME do full def aliasing
		} else {
			symbol_t   *sym;
			sym = make_symbol (".zero", &type_zero, 0, sc_extern);
			return sym->s.def;
		}
	}
	if (imm) {
		cn = imm->def;
		if (def) {
			defspace_free_loc (def->space, def->offset, type_size (def->type));
			def->offset = cn->offset;
			def->initialized = def->constant = 1;
			def->nosave = 1;
			def->local = 0;
			cn = def;
		} else {
			if (cn->type != type) {
				def = new_def (".imm", type, pr.near_data, sc_static);
				def->offset = cn->offset;
				cn = def;
			}
		}
		return cn;
	}
	// allocate a new one
	// always share immediates
	if (def) {
		if (def->type != type) {
			cn = new_def (".imm", type, pr.near_data, sc_static);
			cn->offset = def->offset;
		} else {
			cn = def;
		}
	} else {
		cn = new_def (".imm", type, pr.near_data, sc_static);
	}
	cn->initialized = cn->constant = 1;
	cn->nosave = 1;
	// copy the immediate to the global area
	switch (val.lltype) {
		case ev_string:
			reloc_def_string (cn);
			break;
		case ev_func:
			if (val.v.func_val.val) {
				reloc_t    *reloc;
				reloc = new_reloc (cn->space, cn->offset, rel_def_func);
				reloc->next = pr.relocs;
				pr.relocs = reloc;
			}
			break;
		case ev_field:
			if (val.v.pointer.def)
				reloc_def_field_ofs (val.v.pointer.def, cn);
			break;
		case ev_pointer:
			if (val.v.pointer.def) {
				EMIT_DEF_OFS (pr.near_data, D_INT (cn),
							  val.v.pointer.def);
			}
			break;
		default:
			break;
	}

	memcpy (D_POINTER (void, cn), &val.v, 4 * type_size (type));

	make_def_imm (cn, tab, &val);

	return cn;
}

void
clear_immediates (void)
{
	def_t      *def;
	ex_value_t  zero_val;

	if (value_table) {
		Hash_FlushTable (value_table);
		Hash_FlushTable (string_imm_defs);
		Hash_FlushTable (float_imm_defs);
		Hash_FlushTable (vector_imm_defs);
		Hash_FlushTable (entity_imm_defs);
		Hash_FlushTable (field_imm_defs);
		Hash_FlushTable (func_imm_defs);
		Hash_FlushTable (pointer_imm_defs);
		Hash_FlushTable (quaternion_imm_defs);
		Hash_FlushTable (integer_imm_defs);
		Hash_FlushTable (double_imm_defs);
	} else {
		value_table = Hash_NewTable (16381, 0, 0, 0, 0);
		Hash_SetHashCompare (value_table, value_get_hash, value_compare);

		string_imm_defs = Hash_NewTable (16381, 0, imm_free,
										 &string_imm_defs, 0);
		Hash_SetHashCompare (string_imm_defs, imm_get_hash, imm_compare);

		float_imm_defs = Hash_NewTable (16381, 0, imm_free,
										&float_imm_defs, 0);
		Hash_SetHashCompare (float_imm_defs, imm_get_hash, imm_compare);

		vector_imm_defs = Hash_NewTable (16381, 0, imm_free,
										 &vector_imm_defs, 0);
		Hash_SetHashCompare (vector_imm_defs, imm_get_hash, imm_compare);

		entity_imm_defs = Hash_NewTable (16381, 0, imm_free,
										 &entity_imm_defs, 0);
		Hash_SetHashCompare (entity_imm_defs, imm_get_hash, imm_compare);

		field_imm_defs = Hash_NewTable (16381, 0, imm_free,
										&field_imm_defs, 0);
		Hash_SetHashCompare (field_imm_defs, imm_get_hash, imm_compare);

		func_imm_defs = Hash_NewTable (16381, 0, imm_free,
									   &func_imm_defs, 0);
		Hash_SetHashCompare (func_imm_defs, imm_get_hash, imm_compare);

		pointer_imm_defs = Hash_NewTable (16381, 0, imm_free,
										  &pointer_imm_defs, 0);
		Hash_SetHashCompare (pointer_imm_defs, imm_get_hash, imm_compare);

		quaternion_imm_defs = Hash_NewTable (16381, 0, imm_free,
											 &quaternion_imm_defs, 0);
		Hash_SetHashCompare (quaternion_imm_defs, imm_get_hash, imm_compare);

		integer_imm_defs = Hash_NewTable (16381, 0, imm_free,
										  &integer_imm_defs, 0);
		Hash_SetHashCompare (integer_imm_defs, imm_get_hash, imm_compare);

		double_imm_defs = Hash_NewTable (16381, 0, imm_free,
										 &double_imm_defs, 0);
		Hash_SetHashCompare (double_imm_defs, imm_get_hash, imm_compare);
	}

	def = make_symbol (".zero", &type_zero, 0, sc_extern)->s.def;

	memset (&zero_val, 0, sizeof (zero_val));
	make_def_imm (def, string_imm_defs, &zero_val);
	make_def_imm (def, float_imm_defs, &zero_val);
	make_def_imm (def, entity_imm_defs, &zero_val);
	make_def_imm (def, pointer_imm_defs, &zero_val);
	make_def_imm (def, integer_imm_defs, &zero_val);
}
