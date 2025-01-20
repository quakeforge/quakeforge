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

#include <string.h>
#include <stdint.h>

#include "QF/alloc.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/va.h"

#include "QF/simd/types.h"

#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/evaluate.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

//FIXME I don't like this being here (or aligned_alloc being necessary in
//the first place, but not at all sure what to do about that)
#ifdef _WIN32
#define aligned_alloc(al, sz) _aligned_malloc(sz, al)
#define free(x) _aligned_free(x)
#endif

typedef struct {
	def_t      *def;
	union {
		uint8_t     raw_imm;
#define EV_TYPE(type) pr_##type##_t type##_val;
#include "QF/progs/pr_type_names.h"
#define VEC_TYPE(type_name, base_type) pr_##type_name##_t type_name##_val;
#include "tools/qfcc/include/vec_types.h"
		ex_pointer_t pointer;
	};
} immediate_t;
#define imm_size (sizeof(immediate_t) - offsetof(immediate_t, raw_imm))

static hashtab_t *value_table;
ALLOC_STATE (ex_value_t, values);

static uintptr_t
value_get_hash (const void *_val, void *unused)
{
	const ex_value_t *val = (const ex_value_t *) _val;
	return Hash_Buffer (&val->raw_value, value_size) ^ (uintptr_t) val->type;
}

static int
value_compare (const void *_val1, const void *_val2, void *unused)
{
	const ex_value_t *val1 = (const ex_value_t *) _val1;
	const ex_value_t *val2 = (const ex_value_t *) _val2;
	if (val1->type != val2->type)
		return 0;
	return memcmp (&val1->raw_value, &val2->raw_value, value_size) == 0;
}

ex_value_t *
new_value (void)
{
	ex_value_t *value;
#define malloc(x) aligned_alloc (__alignof__(ex_value_t), x)
	ALLOC (256, ex_value_t, values, value);
#undef malloc
	return value;
}

static void
set_val_type (ex_value_t *val, const type_t *type)
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
		val.string_val = save_string (string_val);
	return find_value (&val);
}

ex_value_t *
new_double_val (double double_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_double);
	val.double_val = double_val;
	return find_value (&val);
}

ex_value_t *
new_float_val (float float_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_float);
	val.float_val = float_val;
	return find_value (&val);
}

ex_value_t *
new_vector_val (const float *vector_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_vector);
	VectorCopy (vector_val, val.vector_val);
	return find_value (&val);
}

ex_value_t *
new_entity_val (int entity_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_entity);
	val.entity_val = entity_val;
	return find_value (&val);
}

ex_value_t *
new_field_val (int field_val, const type_t *type, def_t *def)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, field_type (type));
	val.pointer.val = field_val;
	val.pointer.type = type;
	val.pointer.def = def;
	return find_value (&val);
}

ex_value_t *
new_func_val (int func_val, const type_t *type)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, type);
	val.func_val.val = func_val;
	val.func_val.type = type;
	return find_value (&val);
}

ex_value_t *
new_pointer_val (int pointer_val, const type_t *type, def_t *def,
				 struct operand_s *tempop)
{
	ex_value_t  val;
	if (!type) {
		internal_error (0, "pointer value with no type");
	}
	memset (&val, 0, sizeof (val));
	set_val_type (&val, pointer_type (type));
	val.pointer.val = pointer_val;
	val.pointer.type = type;
	val.pointer.def = def;
	val.pointer.tempop = tempop;
	return find_value (&val);
}

ex_value_t *
new_quaternion_val (const float *quaternion_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_quaternion);
	QuatCopy (quaternion_val, val.quaternion_val);
	return find_value (&val);
}

ex_value_t *
new_int_val (int int_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_int);
	val.int_val = int_val;
	return find_value (&val);
}

ex_value_t *
new_uint_val (int uint_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_uint);
	val.uint_val = uint_val;
	return find_value (&val);
}

ex_value_t *
new_long_val (pr_long_t long_val)
{
	ex_value_t  val = { .long_val = long_val };
	set_val_type (&val, &type_long);
	return find_value (&val);
}

ex_value_t *
new_ulong_val (pr_ulong_t ulong_val)
{
	ex_value_t  val = { .ulong_val = ulong_val };
	set_val_type (&val, &type_ulong);
	return find_value (&val);
}

ex_value_t *
new_short_val (short short_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_short);
	val.short_val = short_val;
	return find_value (&val);
}

ex_value_t *
new_ushort_val (unsigned short ushort_val)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, &type_ushort);
	val.ushort_val = ushort_val;
	return find_value (&val);
}

ex_value_t *
new_nil_val (const type_t *type)
{
	ex_value_t  val;
	memset (&val, 0, sizeof (val));
	set_val_type (&val, type);
	if (val.lltype == ev_void) {
		val.lltype = type_nil->type;
	}
	if (val.lltype == ev_ptr || val.lltype == ev_field )
		val.pointer.type = type->fldptr.type;
	if (val.lltype == ev_func)
		val.func_val.type = type;
	return find_value (&val);
}

ex_value_t *
new_type_value (const type_t *type, const pr_type_t *data)
{
	size_t      typeSize = type_size (type) * sizeof (pr_type_t);
	ex_value_t  val = {};
	if (typeSize > sizeof (val) - offsetof (ex_value_t, raw_value)) {
		internal_error (0, "value too large");
	}
	set_val_type (&val, type);
	memcpy (&val.raw_value, data, typeSize);
	return find_value (&val);
}

void
value_store (pr_type_t *dst, const type_t *dstType, const expr_t *src)
{
	size_t      dstSize = type_size (dstType) * sizeof (pr_type_t);

	if (src->type == ex_nil) {
		memset (dst, 0, dstSize);
		return;
	}
	if (src->type == ex_symbol && src->symbol->sy_type == sy_def) {
		// initialized global def treated as a constant
		// from the tests in cast_expr, the def is known to be constant
		def_t      *def = src->symbol->def;
		memcpy (dst, &D_PACKED (pr_type_t, def), dstSize);
		return;
	}
	ex_value_t *val = 0;
	if (src->type == ex_value) {
		val = src->value;
	}
	if (src->type == ex_symbol && src->symbol->sy_type == sy_const) {
		val = src->symbol->value;
	}
	if (!val) {
		internal_error (src, "unexpected constant expression type");
	}
	memcpy (dst, &val->raw_value, dstSize);
}

const char *
get_value_string (const ex_value_t *value)
{
	const type_t *type = value->type;
	const char *str = "";
	switch (type->type) {
		case ev_string:
			return va (0, "\"%s\"", quote_string (value->string_val));
		case ev_vector:
		case ev_quaternion:
		case ev_float:
			switch (type_width (type)) {
				case 1:
					str = va (0, "%.9g", value->float_val);
					break;
				case 2:
					str = va (0, VEC2F_FMT, VEC2_EXP (value->vec2_val));
					break;
				case 3:
					str = va (0, "[%.9g, %.9g, %.9g]",
							  VectorExpand (value->vec3_val));
					break;
				case 4:
					str = va (0, VEC4F_FMT, VEC4_EXP (value->vec4_val));
					break;
			}
			return va (0, "%s %s", type->name, str);
		case ev_entity:
		case ev_func:
			return va (0, "%s %d", type->name, value->int_val);
		case ev_field:
			if (value->pointer.def) {
				int         offset = value->pointer.val;
				offset += value->pointer.def->offset;
				return va (0, "field %d", offset);
			} else {
				return va (0, "field %d", value->pointer.val);
			}
		case ev_ptr:
			if (value->pointer.def) {
				str = va (0, "<%s>", value->pointer.def->name);
			}
			return va (0, "(* %s)[%d]%s",
					   value->pointer.type
						? get_type_string (value->pointer.type) : "???",
					   value->pointer.val, str);
		case ev_int:
			switch (type_width (type)) {
				case 1:
					str = va (0, "%"PRIi32, value->int_val);
					break;
				case 2:
					str = va (0, VEC2I_FMT, VEC2_EXP (value->ivec2_val));
					break;
				case 3:
					str = va (0, "[%"PRIi32", %"PRIi32", %"PRIi32"]",
							  VectorExpand (value->ivec3_val));
					break;
				case 4:
					str = va (0, VEC4I_FMT, VEC4_EXP (value->ivec4_val));
					break;
			}
			return va (0, "%s %s", type->name, str);
		case ev_uint:
			switch (type_width (type)) {
				case 1:
					str = va (0, "%"PRIu32, value->uint_val);
					break;
				case 2:
					str = va (0, "[%"PRIu32", %"PRIi32"]",
							  VEC2_EXP (value->uivec2_val));
					break;
				case 3:
					str = va (0, "[%"PRIu32", %"PRIi32", %"PRIi32"]",
							  VectorExpand (value->uivec3_val));
					break;
				case 4:
					str = va (0, "[%"PRIu32", %"PRIi32", %"PRIi32", %"PRIi32"]",
							  VEC4_EXP (value->uivec4_val));
					break;
			}
			return va (0, "%s %s", type->name, str);
		case ev_short:
			return va (0, "%s %"PRIi16, type->name, value->short_val);
		case ev_ushort:
			return va (0, "%s %"PRIu16, type->name, value->ushort_val);
		case ev_double:
			switch (type_width (type)) {
				case 1:
					str = va (0, "%.17g", value->double_val);
					break;
				case 2:
					str = va (0, VEC2D_FMT, VEC2_EXP (value->dvec2_val));
					break;
				case 3:
					str = va (0, "[%.17g, %.17g, %.17g]",
							  VectorExpand (value->dvec3_val));
					break;
				case 4:
					str = va (0, VEC4D_FMT, VEC4_EXP (value->dvec4_val));
					break;
			}
			return va (0, "%s %s", type->name, str);
		case ev_long:
			switch (type_width (type)) {
				case 1:
					str = va (0, "%"PRIi64, value->long_val);
					break;
				case 2:
					str = va (0, VEC2L_FMT, VEC2_EXP (value->lvec2_val));
					break;
				case 3:
					str = va (0, "[%"PRIi64", %"PRIi64", %"PRIi64"]",
							  VectorExpand (value->lvec3_val));
					break;
				case 4:
					str = va (0, VEC4L_FMT, VEC4_EXP (value->lvec4_val));
					break;
			}
			return va (0, "%s %s", type->name, str);
		case ev_ulong:
			switch (type_width (type)) {
				case 1:
					str = va (0, "%"PRIu64, value->ulong_val);
					break;
				case 2:
					str = va (0, "[%"PRIu64", %"PRIi64"]",
							  VEC2_EXP (value->ulvec2_val));
					break;
				case 3:
					str = va (0, "[%"PRIu64", %"PRIi64", %"PRIi64"]",
							  VectorExpand (value->ulvec3_val));
					break;
				case 4:
					str = va (0, "[%"PRIu64", %"PRIi64", %"PRIi64", %"PRIi64"]",
							  VEC4_EXP (value->ulvec4_val));
					break;
			}
			return va (0, "%s %s", type->name, str);
		case ev_void:
			return "<void>";
		case ev_invalid:
			return "<invalid>";
		case ev_type_count:
			return "<type_count>";
	}
	return "invalid type";
}

static hashtab_t *string_imm_defs;
static hashtab_t *fldptr_imm_defs;
static hashtab_t *value_imm_defs;

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
		const char *str = pr.strings->strings + imm->string_val;
		return str ? Hash_String (str) : 0;
	} else if (tab == &fldptr_imm_defs) {
		return Hash_Buffer (&imm->pointer, sizeof (&imm->pointer));
	} else if (tab == &value_imm_defs) {
		size_t      size = type_size (imm->def->type) * sizeof (pr_type_t);
		return Hash_Buffer (&imm->raw_imm, size) ^ (uintptr_t) imm->def->type;
	} else {
		internal_error (0, "invalid immediate hash table");
	}
}

static __attribute__((pure)) int
imm_compare (const void *_imm1, const void *_imm2, void *_tab)
{
	immediate_t *imm1 = (immediate_t *) _imm1;
	immediate_t *imm2 = (immediate_t *) _imm2;
	hashtab_t  **tab = (hashtab_t **) _tab;

	if (tab == &string_imm_defs) {
		const char *str1 = pr.strings->strings + imm1->string_val;
		const char *str2 = pr.strings->strings + imm2->string_val;
		return (str1 == str2 || (str1 && str2 && !strcmp (str1, str2)));
	} else if (tab == &fldptr_imm_defs) {
		return !memcmp (&imm1->pointer, &imm2->pointer,
						sizeof (imm1->pointer));
	} else if (tab == &value_imm_defs) {
		size_t      size = type_size (imm1->def->type) * sizeof (pr_type_t);
		if (imm1->def->type->alignment != imm2->def->type->alignment) {
			return 0;
		}
		return !memcmp (&imm1->raw_imm, &imm2->raw_imm, size);
	} else {
		internal_error (0, "invalid immediate hash table");
	}
}

int
ReuseString (const char *str)
{
	return strpool_addstr (pr.strings, str);
}

ex_value_t *
offset_alias_value (ex_value_t *value, const type_t *type, int offset)
{
	if (type_size (type) > type_size (value->type)) {
		error (0, "unable to alias to a larger sized value");
		return value;
	}
	if (offset < 0 || offset + type_size (type) > type_size (value->type)) {
		error (0, "invalid offset");
		return value;
	}
	pr_type_t   data[type_size (value->type)];
	size_t      data_size = sizeof (pr_type_t) * type_size (value->type);
	memcpy (data, &value->raw_value, data_size);
	return new_type_value (type, data + offset);
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

	imm = aligned_alloc (__alignof__(immediate_t), sizeof (immediate_t));
	memset (imm, 0, sizeof (immediate_t));
	imm->def = def;
	memcpy (&imm->raw_imm, &val->raw_value, imm_size);

	Hash_AddElement (tab, imm);

	return imm;
}

def_t *
emit_value_core (ex_value_t *val, def_t *def, defspace_t *data)
{
	if (!def) {
		def = new_def (".imm", val->type, data, sc_static);
	}
	def->initialized = def->constant = 1;
	def->nosave = 1;
	// copy the immediate to the global area
	switch (val->lltype) {
		case ev_string:
			reloc_def_string (def);
			break;
		case ev_func:
			if (val->func_val.val) {
				reloc_t    *reloc;
				reloc = new_reloc (def->space, def->offset, rel_def_func);
				reloc->next = pr.relocs;
				pr.relocs = reloc;
			}
			break;
		case ev_field:
			if (val->pointer.def)
				reloc_def_field_ofs (val->pointer.def, def);
			break;
		case ev_ptr:
			if (val->pointer.def) {
				EMIT_DEF_OFS (data, D_INT (def), val->pointer.def);
			}
			break;
		default:
			break;
	}

	size_t      data_size = sizeof (pr_type_t) * type_size (val->type);
	memcpy (D_POINTER (pr_type_t, def), &val->raw_value, data_size);
	return def;
}

def_t *
emit_value (ex_value_t *value, def_t *def)
{
	def_t      *cn;
	hashtab_t  *tab = 0;
	const type_t *type;
	ex_value_t  val = *value;

	if (!string_imm_defs) {
		clear_immediates ();
	}
	cn = 0;
//	if (val.type == ev_void)
//		val.type = type_nil->type;
	switch (val.lltype) {
		case ev_entity:
		case ev_func:
		case ev_int:
		case ev_uint:
		case ev_float:
		case ev_vector:
		case ev_quaternion:
		case ev_double:
		case ev_long:
		case ev_ulong:
			tab = value_imm_defs;
			type = val.type;
			break;
		case ev_field:
		case ev_ptr:
			tab = fldptr_imm_defs;
			type = ev_types[val.lltype];
			break;
		case ev_string:
			val.int_val = ReuseString (val.string_val);
			tab = string_imm_defs;
			type = &type_string;
			break;
		default:
			internal_error (0, "unexpected value type: %s",
							val.type->type < ev_type_count
								? pr_type_name[val.lltype]
								: va (0, "%d", val.lltype));
	}
	def_t       search_def = { .type = type };
	immediate_t search = { .def = &search_def };
	memcpy (&search.raw_imm, &val.raw_value, imm_size);
	immediate_t *imm = Hash_FindElement (tab, &search);
	if (imm && strcmp (imm->def->name, ".zero") == 0) {
		if (def) {
			imm = 0;	//FIXME do full def aliasing
		} else {
			symbol_t   *sym;
			sym = make_symbol (".zero", &type_zero, 0, sc_extern);
			return sym->def;
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
	val.type = type;
	cn = emit_value_core (&val, cn, pr.near_data);

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
		Hash_FlushTable (fldptr_imm_defs);
		Hash_FlushTable (value_imm_defs);
	} else {
		setup_value_progs ();

		value_table = Hash_NewTable (16381, 0, 0, 0, 0);
		Hash_SetHashCompare (value_table, value_get_hash, value_compare);

		string_imm_defs = Hash_NewTable (16381, 0, imm_free,
										 &string_imm_defs, 0);
		Hash_SetHashCompare (string_imm_defs, imm_get_hash, imm_compare);

		fldptr_imm_defs = Hash_NewTable (16381, 0, imm_free,
										&fldptr_imm_defs, 0);
		Hash_SetHashCompare (fldptr_imm_defs, imm_get_hash, imm_compare);

		value_imm_defs = Hash_NewTable (16381, 0, imm_free,
										&value_imm_defs, 0);
		Hash_SetHashCompare (value_imm_defs, imm_get_hash, imm_compare);
	}

	def = make_symbol (".zero", &type_zero, 0, sc_extern)->def;

	memset (&zero_val, 0, sizeof (zero_val));
	make_def_imm (def, string_imm_defs, &zero_val);
	make_def_imm (def, fldptr_imm_defs, &zero_val);
	make_def_imm (def, value_imm_defs, &zero_val);
}
