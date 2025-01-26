/*
	type.c

	type system

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/12/11

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
#include <ctype.h>

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/obj_type.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

#define EV_TYPE(t) \
	type_t type_##t = { \
		.type = ev_##t, \
		.name = #t, \
		.alignment = PR_ALIGNOF(t), \
		.width = 1, \
		.columns = 1, \
		.meta = ty_basic, \
		{{ __builtin_choose_expr (ev_##t == ev_field \
							   || ev_##t == ev_func \
							   || ev_##t == ev_ptr, &type_void, 0 ) }}, \
	};
#include "QF/progs/pr_type_names.h"

type_t      type_invalid = {
	.type = ev_invalid,
	.name = "invalid",
};

type_t      type_auto = {
	.type = ev_invalid,
	.name = "auto",
};

#define VEC_TYPE(type_name, base_type) \
	type_t type_##type_name = { \
		.type = ev_##base_type, \
		.name = #type_name, \
		.alignment = PR_ALIGNOF(type_name), \
		.width = PR_SIZEOF(type_name) / PR_SIZEOF (base_type), \
		.columns = 1, \
		.meta = ty_basic, \
	};
#include "tools/qfcc/include/vec_types.h"

#define MAT_TYPE(type_name, base_type, cols, align_as) \
	type_t type_##type_name = { \
		.type = ev_##base_type, \
		.name = #type_name, \
		.alignment = PR_ALIGNOF(align_as), \
		.width = PR_SIZEOF(align_as) / PR_SIZEOF (base_type), \
		.columns = cols, \
		.meta = ty_basic, \
	};
#include "tools/qfcc/include/mat_types.h"

type_t type_bool = {
	.type = ev_int,
	.name = "bool",
	.alignment = PR_ALIGNOF(int),
	.width = PR_SIZEOF(int) / PR_SIZEOF (int),
	.columns = 1,
	.meta = ty_bool,
};
type_t type_bvec2 = {
	.type = ev_int,
	.name = "bvec2",
	.alignment = PR_ALIGNOF(ivec2),
	.width = PR_SIZEOF(ivec2) / PR_SIZEOF (int),
	.columns = 1,
	.meta = ty_bool,
};
type_t type_bvec3 = {
	.type = ev_int,
	.name = "bvec3",
	.alignment = PR_ALIGNOF(ivec3),
	.width = PR_SIZEOF(ivec3) / PR_SIZEOF (int),
	.columns = 1,
	.meta = ty_bool,
};
type_t type_bvec4 = {
	.type = ev_int,
	.name = "bvec4",
	.alignment = PR_ALIGNOF(ivec4),
	.width = PR_SIZEOF(ivec4) / PR_SIZEOF (int),
	.columns = 1,
	.meta = ty_bool,
};
type_t type_lbool = {
	.type = ev_long,
	.name = "lbool",
	.alignment = PR_ALIGNOF(long),
	.width = PR_SIZEOF(long) / PR_SIZEOF (long),
	.columns = 1,
	.meta = ty_bool,
};
type_t type_lbvec2 = {
	.type = ev_long,
	.name = "lbvec2",
	.alignment = PR_ALIGNOF(lvec2),
	.width = PR_SIZEOF(lvec2) / PR_SIZEOF (long),
	.columns = 1,
	.meta = ty_bool,
};
type_t type_lbvec3 = {
	.type = ev_long,
	.name = "lbvec3",
	.alignment = PR_ALIGNOF(lvec3),
	.width = PR_SIZEOF(lvec3) / PR_SIZEOF (long),
	.columns = 1,
	.meta = ty_bool,
};
type_t type_lbvec4 = {
	.type = ev_long,
	.name = "lbvec4",
	.alignment = PR_ALIGNOF(lvec4),
	.width = PR_SIZEOF(lvec4) / PR_SIZEOF (long),
	.columns = 1,
	.meta = ty_bool,
};

#define VEC_TYPE(type_name, base_type) &type_##type_name,
#define MAT_TYPE(type_name, base_type, cols, align_as) &type_##type_name,
static type_t *matrix_types[] = {
#include "tools/qfcc/include/vec_types.h"
	&type_bool,
	&type_bvec2,
	&type_bvec3,
	&type_bvec4,
	&type_lbool,
	&type_lbvec2,
	&type_lbvec3,
	&type_lbvec4,
#include "tools/qfcc/include/mat_types.h"
	0
};

type_t     *type_nil;
type_t     *type_default;
type_t     *type_long_int;
type_t     *type_ulong_uint;

// these will be built up further
type_t      type_va_list = {
	.type = ev_invalid,
	.meta = ty_struct,
};
type_t      type_param = {
	.type = ev_invalid,
	.meta = ty_struct,
};
static type_t type_param_pointer = {
	.type = ev_ptr,
	.alignment = 1,
	.width = 1,
	.columns = 1,
	.meta = ty_basic,
	.fldptr.type = &type_param,
};
type_t      type_zero = {
	.type = ev_invalid,
	.meta = ty_struct,
};
type_t      type_type_encodings = {
	.type = ev_invalid,
	.name = "@type_encodings",
	.meta = ty_struct,
};
type_t      type_xdef = {
	.type = ev_invalid,
	.name = "@xdef",
	.meta = ty_struct,
};
type_t      type_xdef_pointer = {
	.type = ev_ptr,
	.alignment = 1,
	.width = 1,
	.columns = 1,
	.meta = ty_basic,
	.fldptr.type = &type_xdef,
};
type_t      type_xdefs = {
	.type = ev_invalid,
	.name = "@xdefs",
	.meta = ty_struct,
};

type_t      type_floatfield = {
	.type = ev_field,
	.name = ".float",
	.alignment = 1,
	.width = 1,
	.columns = 1,
	.meta = ty_basic,
	{{&type_float}},
};

#define EV_TYPE(type) &type_##type,
const type_t *ev_types[ev_type_count] = {
#include "QF/progs/pr_type_names.h"
	&type_invalid,
};

int type_cast_map[ev_type_count] = {
	[ev_int] = 0,
	[ev_float] = 1,
	[ev_long] = 2,
	[ev_double] = 3,
	[ev_uint] = 4,
	//[ev_bool32] = 5,
	[ev_ulong] = 6,
	//[ev_bool64] = 7,
};

typeset_t type_registry = DARRAY_STATIC_INIT (64);
defset_t type_encodings = DARRAY_STATIC_INIT (64);

ALLOC_STATE (type_t, types);

static hashtab_t *type_tab;

etype_t
low_level_type (const type_t *type)
{
	if (type->type > ev_type_count)
		internal_error (0, "invalid type");
	if (type->type == ev_type_count)
		internal_error (0, "found 'type count' type");
	if (type->type < ev_invalid)
		return type->type;
	if (is_enum (type))
		return type_default->type;
	if (is_structural (type))
		return ev_void;
	internal_error (0, "invalid complex type");
}

const char *
type_get_encoding (const type_t *type)
{
	static dstring_t *encoding;

	if (!encoding)
		encoding = dstring_newstr();
	else
		dstring_clearstr (encoding);
	encode_type (encoding, type);
	return save_string (encoding->str);
}

void
chain_type (type_t *type)
{
	if (type->next) {
		internal_error (0, "type already chained");
	}
	if (type->id) {
		internal_error (0, "type already has id");
	}
	type->id = type_registry.size;
	DARRAY_APPEND (&type_registry, type);
	DARRAY_APPEND (&type_encodings, nullptr);
	type->next = pr.types;
	pr.types = type;
	if (!type->encoding)
		type->encoding = type_get_encoding (type);
	type_encodings.a[type->id] = qfo_encode_type (type, pr.type_data);
	Hash_Add (type_tab, type);
}

type_t *
new_type (void)
{
	type_t     *type;
	ALLOC (1024, type_t, types, type);
	type->freeable = 1;
	type->allocated = 1;
	return type;
}

void
free_type (type_t *type)
{
	if (!type)
		return;
	if (!type->allocated) {		// for statically allocated types
		type->next = 0;
		type->id = 0;
	}
	if (!type->freeable)
		return;
	switch (type->type) {
		case ev_void:
		case ev_string:
		case ev_float:
		case ev_vector:
		case ev_entity:
		case ev_type_count:
		case ev_quaternion:
		case ev_int:
		case ev_uint:
		case ev_long:
		case ev_ulong:
		case ev_short:
		case ev_ushort:
		case ev_double:
			break;
		case ev_field:
		case ev_ptr:
			free_type ((type_t *) type->fldptr.type);
			break;
		case ev_func:
			free_type ((type_t *) type->func.ret_type);
			break;
		case ev_invalid:
			if (type->meta == ty_array)
				free_type ((type_t *) type->array.type);
			break;
	}
	memset (type, 0, sizeof (*type));
	FREE (types, type);
}

static type_t *
copy_chain (type_t *type, type_t *append)
{
	type_t     *new = 0;
	type_t    **n = &new;

	while (type) {
		*n = new_type ();
		**n = *type;
		switch (type->meta) {
			case ty_bool:
			case ty_basic:
				switch (type->type) {
					case ev_void:
					case ev_string:
					case ev_float:
					case ev_vector:
					case ev_entity:
					case ev_type_count:
					case ev_quaternion:
					case ev_int:
					case ev_uint:
					case ev_long:
					case ev_ulong:
					case ev_short:
					case ev_ushort:
					case ev_double:
						internal_error (0, "copy basic type");
					case ev_field:
					case ev_ptr:
						n = (type_t **) &(*n)->fldptr.type;
						type = (type_t *) type->fldptr.type;
						break;
					case ev_func:
						n = (type_t **) &(*n)->func.ret_type;
						type = (type_t *) type->func.ret_type;
						break;
					case ev_invalid:
						internal_error (0, "invalid basic type");
						break;
				}
				break;
			case ty_array:
				n = (type_t **) &(*n)->array.type;
				type = (type_t *) type->array.type;
				break;
			case ty_struct:
			case ty_union:
			case ty_enum:
			case ty_class:
			case ty_alias:	//XXX is this correct?
			case ty_handle:
			case ty_algebra:
			case ty_meta_count:
				internal_error (0, "copy object type %d", type->meta);
		}
	}
	*n = append;
	return new;
}

const type_t *
append_type (const type_t *type, const type_t *new)
{
	const type_t **t = &type;

	while (*t) {
		switch ((*t)->meta) {
			case ty_bool:
			case ty_basic:
				switch ((*t)->type) {
					case ev_void:
					case ev_string:
					case ev_float:
					case ev_vector:
					case ev_entity:
					case ev_type_count:
					case ev_quaternion:
					case ev_int:
					case ev_uint:
					case ev_long:
					case ev_ulong:
					case ev_short:
					case ev_ushort:
					case ev_double:
						internal_error (0, "append to basic type");
					case ev_field:
					case ev_ptr:
						t = (const type_t **) &(*t)->fldptr.type;
						((type_t *) type)->alignment = 1;
						((type_t *) type)->width = 1;
						((type_t *) type)->columns = 1;
						break;
					case ev_func:
						t = (const type_t **) &(*t)->func.ret_type;
						((type_t *) type)->alignment = 1;
						((type_t *) type)->width = 1;
						((type_t *) type)->columns = 1;
						break;
					case ev_invalid:
						internal_error (0, "invalid basic type");
						break;
				}
				break;
			case ty_array:
				t = (const type_t **) &(*t)->array.type;
				((type_t *) type)->alignment = new->alignment;
				((type_t *) type)->width = new->width;
				((type_t *) type)->columns = new->columns;
				break;
			case ty_struct:
			case ty_union:
			case ty_enum:
			case ty_class:
			case ty_alias:	//XXX is this correct?
			case ty_handle:
			case ty_algebra:
			case ty_meta_count:
				internal_error (0, "append to object type");
		}
	}
	if (type && new->meta == ty_alias) {
		auto chain = find_type (copy_chain ((type_t *) type, (type_t *) new));
		*t = new->alias.aux_type;
		type = alias_type (type, chain, 0);
	} else {
		*t = new;
	}
	return type;
}

specifier_t
default_type (specifier_t spec, const symbol_t *sym)
{
	if (spec.type) {
		if (is_float (spec.type) && !spec.multi_type) {
			// no modifieres allowed
			if (spec.is_unsigned) {
				spec.multi_type = true;
				error (0, "both unsigned and float in declaration specifiers");
			} else if (spec.is_signed) {
				spec.multi_type = true;
				error (0, "both signed and float in declaration specifiers");
			} else if (spec.is_short) {
				spec.multi_type = true;
				error (0, "both short and float in declaration specifiers");
			} else if (spec.is_long) {
				spec.multi_type = true;
				error (0, "both long and float in declaration specifiers");
			}
		}
		if (is_double (spec.type)) {
			// long is allowed but ignored
			if (spec.is_unsigned) {
				spec.multi_type = true;
				error (0, "both unsigned and double in declaration specifiers");
			} else if (spec.is_signed) {
				spec.multi_type = true;
				error (0, "both signed and double in declaration specifiers");
			} else if (spec.is_short) {
				spec.multi_type = true;
				error (0, "both short and double in declaration specifiers");
			}
		}
		if (is_int (spec.type)) {
			// signed and short are ignored
			if (spec.is_unsigned && spec.is_long) {
				spec.type = &type_ulong;
			} else if (spec.is_long) {
				spec.type = &type_long;
			} else if (spec.is_unsigned) {
				spec.type = &type_uint;
			}
		}
	} else if (!spec.type_expr) {
		if (spec.is_long) {
			if (spec.is_unsigned) {
				spec.type = type_ulong_uint;
			} else {
				spec.type = type_long_int;
			}
		} else {
			if (spec.is_unsigned) {
				spec.type = &type_uint;
			} else if (spec.is_signed || spec.is_short) {
				spec.type = &type_int;
			}
		}
	}
	if (!spec.type && !spec.type_expr) {
		spec.type = type_default;
		if (sym) {
			warning (0, "type defaults to '%s' in declaration of '%s'",
					 type_default->name, sym->name);
		} else {
			warning (0, "type defaults to '%s' in abstract declaration",
					 type_default->name);
		}
	}
	return spec;
}

static __attribute__((pure)) bool
types_same (const type_t *a, const type_t *b)
{
	int         i, count;

	if (a->type != b->type || a->meta != b->meta)
		return false;
	switch (a->meta) {
		case ty_basic:
			switch (a->type) {
				case ev_field:
				case ev_ptr:
					if (a->fldptr.type != b->fldptr.type)
						return false;
				case ev_func:
					if (a->func.ret_type != b->func.ret_type
						|| a->func.num_params != b->func.num_params
						|| a->func.attribute_bits != b->func.attribute_bits)
						return false;
					count = a->func.num_params;
					if (count < 0)
						count = ~count;	// param count is one's complement
					for (i = 0; i < count; i++)
						if (a->func.param_types[i]
							!= b->func.param_types[i])
							return false;
					return 1;
				default:		// other types don't have aux data
					return a->width == b->width && a->columns == b->columns;
			}
			break;
		case ty_struct:
		case ty_union:
		case ty_enum:
			if (strcmp (a->name, b->name))
				return false;
			if (a->meta == ty_struct)
				return compare_protocols (a->protos, b->protos);
			return 1;
		case ty_array:
			if (a->array.type != b->array.type
				|| a->array.base != b->array.base
				|| a->array.count != b->array.count)
				return false;
			return 1;
		case ty_class:
			if (a->class != b->class)
				return false;
			return compare_protocols (a->protos, b->protos);
		case ty_alias:
			// names have gone through save_string
			return (a->name == b->name
					&& a->alias.aux_type == b->alias.aux_type
					&& a->alias.full_type == b->alias.full_type);
		case ty_handle:
			// names have gone through save_string
			return a->name == b->name;
		case ty_algebra:
			return a->algebra == b->algebra;
		case ty_bool:
			return a->type == b->type;
		case ty_meta_count:
			break;
	}
	internal_error (nullptr, "we be broke");
}

/*
	find_type

	Returns a preexisting complex type that matches the parm, or allocates
	a new one and copies it out.
*/
const type_t *
find_type (const type_t *type)
{
	int         i, count;

	if (!type || type == &type_auto)
		return type;

	if (!type->encoding) {
		//FIXME
		((type_t *) type)->encoding = type_get_encoding (type);
	}

	if (type->freeable) {
		switch (type->meta) {
			case ty_bool:
			case ty_basic:
				switch (type->type) {
					case ev_field:
					case ev_ptr:
						((type_t *) type)->fldptr.type = find_type (type->fldptr.type);
						break;
					case ev_func:
						((type_t *) type)->func.ret_type = find_type (type->func.ret_type);
						count = type->func.num_params;
						if (count < 0)
							count = ~count;	// param count is one's complement
						for (i = 0; i < count; i++)
							((type_t *) type)->func.param_types[i]
								= find_type (type->func.param_types[i]);
						break;
					default:		// other types don't have aux data
						break;
				}
				break;
			case ty_struct:
			case ty_union:
			case ty_enum:
				break;
			case ty_array:
				((type_t *) type)->array.type = find_type (type->array.type);
				break;
			case ty_class:
				break;
			case ty_alias:
				((type_t *) type)->alias.aux_type = find_type (type->alias.aux_type);
				((type_t *) type)->alias.full_type = find_type (type->alias.full_type);
				break;
			case ty_handle:
				break;
			case ty_algebra:
				break;
			case ty_meta_count:
				break;
		}
	}

	const type_t *check;
	if (strchr (type->encoding, '%')) {
		// type chain has attributes so the encoding may be aliased
		auto list = (const type_t **) Hash_FindList (type_tab, type->encoding);
		for (auto c = list; (check = *c); c++) {
			if (types_same (check, type)) {
				break;
			}
		}
		free (list);
	} else {
		check = Hash_Find (type_tab, type->encoding);
	}
	if (check) {
		return check;
	}

	// allocate a new one
	auto new = new_type ();
	*new = *type;
	if (is_func (type)) {
		new->func.param_types = 0;
		const type_t *t = unalias_type (type);
		int         num_params = t->func.num_params;
		if (num_params < 0) {
			num_params = ~num_params;
		}
		if (num_params) {
			new->func.param_types = malloc (sizeof (type_t *) * num_params);
			new->func.param_quals = malloc (sizeof (param_qual_t)*num_params);
			for (int i = 0; i < num_params; i++) {
				new->func.param_types[i] = t->func.param_types[i];
				new->func.param_quals[i] = t->func.param_quals[i];
			}
		}
	}
	new->freeable = false;

	chain_type (new);

	return new;
}

const type_t *
auto_type (const type_t *type, const expr_t *init)
{
	if (type == &type_auto) {
		if (init) {
			if (!(type = get_type (init))) {
				type = type_default;
			}
		} else {
			error (0, "'auto' requires an initialized data declaration");
			type = type_default;
		}
	}
	return type;
}

const type_t *
field_type (const type_t *aux)
{
	type_t      _new;
	type_t     *new = &_new;

	if (aux)
		memset (&_new, 0, sizeof (_new));
	else
		new = new_type ();
	new->type = ev_field;
	new->alignment = 1;
	new->width = 1;
	new->columns = 1;
	if (aux) {
		return find_type (append_type (new, aux));
	}
	return new;
}

const type_t *
tagged_pointer_type (unsigned tag, const type_t *aux)
{
	type_t      _new;
	type_t     *new = &_new;

	if (aux)
		memset (&_new, 0, sizeof (_new));
	else
		new = new_type ();
	new->type = ev_ptr;
	new->alignment = 1;
	new->width = 1;
	new->columns = 1;
	new->fldptr.tag = tag;
	new->fldptr.deref = false;
	if (aux) {
		return find_type (append_type (new, aux));
	}
	return new;
}

const type_t *
pointer_type (const type_t *aux)
{
	return tagged_pointer_type (0, aux);
}

const type_t *
tagged_reference_type (unsigned tag, const type_t *aux)
{
	type_t      _new;
	type_t     *new = &_new;

	if (aux)
		memset (&_new, 0, sizeof (_new));
	else
		new = new_type ();
	new->type = ev_ptr;
	new->alignment = 1;
	new->width = 1;
	new->columns = 1;
	new->fldptr.tag = tag;
	new->fldptr.deref = true;
	if (aux) {
		return find_type (append_type (new, aux));
	}
	return new;
}

const type_t *
reference_type (const type_t *aux)
{
	return tagged_reference_type (0, aux);
}

const type_t *
matrix_type (const type_t *ele_type, int cols, int rows)
{
	if (!is_boolean (ele_type) && !is_scalar (ele_type)) {
		return nullptr;
	}
	if (rows == 1 && cols == 1) {
		if (is_bool (ele_type)) {
			return &type_bool;
		}
		if (is_lbool (ele_type)) {
			return &type_lbool;
		}
		for (auto t = ev_types; t - ev_types < ev_type_count; t++) {
			if ((*t)->type == ele_type->type && (*t)->width == 1) {
				return *t;
			}
		}
	}
	if (rows == 1) {
		// no horizontal matrices
		return nullptr;
	}
	for (auto mtype = matrix_types; *mtype; mtype++) {
		if ((*mtype)->meta == ele_type->meta
			&& (*mtype)->type == ele_type->type
			&& (*mtype)->width == rows
			&& (*mtype)->columns == cols) {
			if (options.code.progsversion < PROG_VERSION) {
				if (*mtype == &type_vec3) {
					return &type_vector;
				}
				if (*mtype == &type_vec4) {
					return &type_quaternion;
				}
			}
			return *mtype;
		}
	}
	return nullptr;
}

const type_t *
column_type (const type_t *mat_type)
{
	return vector_type (base_type (mat_type), type_rows (mat_type));
}

const type_t *
vector_type (const type_t *ele_type, int width)
{
	// vectors are single-column matrices
	return matrix_type (ele_type, 1, width);
}

const type_t *
base_type (const type_t *vec_type)
{
	if (is_algebra (vec_type)) {
		return algebra_base_type (vec_type);
	}
	if (!is_math (vec_type)) {
		return vec_type;
	}
	if (is_bool (vec_type)) {
		return &type_bool;
	}
	if (is_lbool (vec_type)) {
		return &type_lbool;
	}
	// vec_type->type for quaternion and vector points back to itself
	if (is_quaternion (vec_type) || is_vector (vec_type)) {
		return &type_float;
	}
	if (is_enum (vec_type)) {//FIXME enum should use valid ev_type
		return type_default;
	}
	return ev_types[vec_type->type];
}

const type_t *
int_type (const type_t *base)
{
	int         width = type_width (base);
	base = base_type (base);
	if (!base) {
		return nullptr;
	}
	if (type_size (base) == 1) {
		base = &type_int;
	} else if (type_size (base) == 2) {
		base = &type_long;
	}
	return vector_type (base, width);
}

const type_t *
uint_type (const type_t *base)
{
	int         width = type_width (base);
	base = base_type (base);
	if (!base) {
		return nullptr;
	}
	if (type_size (base) == 1) {
		base = &type_uint;
	} else if (type_size (base) == 2) {
		base = &type_ulong;
	}
	return vector_type (base, width);
}

const type_t *
bool_type (const type_t *base)
{
	int         width = type_width (base);
	base = base_type (base);
	if (!base) {
		return nullptr;
	}
	if (type_size (base) == 1) {
		base = &type_bool;
	} else if (type_size (base) == 2) {
		base = &type_lbool;
	}
	return vector_type (base, width);
}

const type_t *
float_type (const type_t *base)
{
	int         width = type_width (base);
	base = base_type (base);
	if (!base) {
		return nullptr;
	}
	if (type_size (base) == 1) {
		base = &type_float;
	} else if (type_size (base) == 2) {
		base = &type_double;
	}
	return vector_type (base, width);
}

const type_t *
array_type (const type_t *aux, int size)
{
	type_t      _new;
	type_t     *new = &_new;

	if (aux)
		memset (&_new, 0, sizeof (_new));
	else
		new = new_type ();
	new->meta = ty_array;
	new->type = ev_invalid;
	if (aux) {
		new->alignment = aux->alignment;
		new->width = aux->width;
	}
	new->array.count = size;
	if (aux) {
		return find_type (append_type (new, aux));
	}
	return new;
}

const type_t *
based_array_type (const type_t *aux, int base, int top)
{
	type_t      _new;
	type_t     *new = &_new;

	if (aux)
		memset (&_new, 0, sizeof (_new));
	else
		new = new_type ();
	new->type = ev_invalid;
	if (aux) {
		new->alignment = aux->alignment;
		new->width = aux->width;
	}
	new->meta = ty_array;
	new->array.type = aux;
	new->array.base = base;
	new->array.count = top - base + 1;
	if (aux) {
		return find_type (new);
	}
	return new;
}

const type_t *
alias_type (const type_t *type, const type_t *alias_chain, const char *name)
{
	type_t     *alias = new_type ();
	alias->meta = ty_alias;
	alias->type = type->type;
	alias->alignment = type->alignment;
	alias->width = type->width;
	if (type == alias_chain && type->meta == ty_alias) {
		// typedef of a type that contains a typedef somewhere
		// grab the alias-free branch for type
		type = alias_chain->alias.aux_type;
		if (!alias_chain->name) {
			// the other typedef is further inside, so replace the unnamed
			// alias node with the typedef
			alias_chain = alias_chain->alias.full_type;
		}
	}
	alias->alias.aux_type = type;
	alias->alias.full_type = alias_chain;
	if (name) {
		alias->name = save_string (name);
	}
	alias->property = type->property;
	return alias;
}

const type_t *
unalias_type (const type_t *type)
{
	if (type->meta == ty_alias) {
		type = type->alias.aux_type;
		if (type->meta == ty_alias) {
			internal_error (0, "alias type node in alias-free chain");
		}
	}
	return type;
}

const type_t *
dereference_type (const type_t *type)
{
	// is_ptr is used to catch both pointer and reference
	if (!is_ptr (type) && !is_field (type) && !is_array (type)) {
		internal_error (0, "dereference non pointer/field/array type");
	}
	if (type->meta == ty_alias) {
		type = type->alias.full_type;
	}
	return type->fldptr.type;
}

void
print_type_str (dstring_t *str, const type_t *type)
{
	if (!type) {
		dasprintf (str, " (null)");
		return;
	}
	if (is_auto (type)) {
		dasprintf (str, " auto");
		return;
	}
	switch (type->meta) {
		case ty_meta_count:
			break;
		case ty_algebra:
			algebra_print_type_str (str, type);
			return;
		case ty_handle:
			dasprintf (str, " handle %s", type->name);
			return;
		case ty_alias:
			dasprintf (str, "({%s=", type->name);
			print_type_str (str, type->alias.aux_type);
			dstring_appendstr (str, "})");
			return;
		case ty_class:
			dasprintf (str, " %s", type->class->name);
			if (type->protos)
				print_protocollist (str, type->protos);
			return;
		case ty_enum:
			dasprintf (str, " enum %s", type->name);
			return;
		case ty_struct:
			dasprintf (str, " struct %s", type->name);
			return;
		case ty_union:
			dasprintf (str, " union %s", type->name);
			return;
		case ty_array:
			print_type_str (str, type->array.type);
			if (type->array.base) {
				dasprintf (str, "[%d..%d]", type->array.base,
						type->array.base + type->array.count - 1);
			} else {
				dasprintf (str, "[%d]", type->array.count);
			}
			return;
		case ty_bool:
			dasprintf (str, " %s%s", is_bool (type) ? "bool" : "lbool",
					   type->width > 1 ? va (0, "{%d}", type->width)
									   : "");
			return;
		case ty_basic:
			switch (type->type) {
				case ev_field:
					dasprintf (str, ".(");
					print_type_str (str, type->fldptr.type);
					dasprintf (str, ")");
					return;
				case ev_func:
					print_type_str (str, type->func.ret_type);
					if (type->func.num_params == -1) {
						dasprintf (str, "(...)");
					} else {
						int         c, i;
						dasprintf (str, "(");
						if ((c = type->func.num_params) < 0)
							c = ~c;		// num_params is one's compliment
						for (i = 0; i < c; i++) {
							if (i)
								dasprintf (str, ", ");
							print_type_str (str, type->func.param_types[i]);
						}
						if (type->func.num_params < 0)
								dasprintf (str, ", ...");
						dasprintf (str, ")");
					}
					return;
				case ev_ptr:
					if (type->fldptr.type) {
						if (is_id (type)) {
							__auto_type ptr = type->fldptr.type;
							dasprintf (str, "id");
							if (ptr->protos)
								print_protocollist (str, ptr->protos);
							return;
						}
						if (is_SEL(type)) {
							dasprintf (str, "SEL");
							return;
						}
					}
					dasprintf (str, "(%c", type->fldptr.deref ? '&' : '*');
					if (type->fldptr.tag) {
						dasprintf (str, "%d", type->fldptr.tag);
					}
					print_type_str (str, type->fldptr.type);
					dasprintf (str, ")");
					return;
				case ev_void:
				case ev_string:
				case ev_float:
				case ev_vector:
				case ev_entity:
				case ev_quaternion:
				case ev_int:
				case ev_uint:
				case ev_long:
				case ev_ulong:
				case ev_short:
				case ev_ushort:
				case ev_double:
					{
						const char *name = pr_type_name[type->type];
						int width = type->width;
						int cols = type->columns;
						if (cols > 1) {
							dasprintf (str, " %s{%d,%d}", name, cols, width);
						} else if (type->width > 1) {
							dasprintf (str, " %s{%d}", name, width);
						} else {
							dasprintf (str, " %s", name);
						}
					}
					return;
				case ev_invalid:
				case ev_type_count:
					break;
			}
			break;
	}
	internal_error (0, "bad type meta:type %d:%d", type->meta, type->type);
}

const char *
get_type_string (const type_t *type)
{
	static dstring_t *type_str[8];
	static int  str_index;

	if (!type_str[str_index]) {
		type_str[str_index] = dstring_newstr ();
	}
	dstring_clearstr (type_str[str_index]);
	print_type_str (type_str[str_index], type);
	const char *str = type_str[str_index++]->str;
	str_index %= sizeof (type_str) / sizeof (type_str[0]);
	while (*str == ' ') {
		str++;
	}
	return str;
}

void
print_type (const type_t *type)
{
	dstring_t  *str = dstring_newstr ();
	print_type_str (str, type);
	printf ("%s\n", str->str);
	dstring_delete (str);
}

const char *
encode_params (const type_t *type)
{
	const char *ret;
	dstring_t  *encoding = dstring_newstr ();
	int         i, count;

	if (type->func.num_params < 0)
		count = -type->func.num_params - 1;
	else
		count = type->func.num_params;
	for (i = 0; i < count; i++) {
		// in is the default qualifier
		if (type->func.param_quals[i] != pq_in) {
			dasprintf (encoding, "%c", "c_Oo"[type->func.param_quals[i]]);
		}
		encode_type (encoding, unalias_type (type->func.param_types[i]));
	}
	if (type->func.num_params < 0)
		dasprintf (encoding, ".");

	ret = save_string (encoding->str);
	dstring_delete (encoding);
	return ret;
}

static void
encode_class (dstring_t *encoding, const type_t *type)
{
	class_t    *class = type->class;
	const char *name ="?";

	if (class->name)
		name = class->name;
	dasprintf (encoding, "{%s@}", name);
}

static void
encode_struct (dstring_t *encoding, const type_t *type)
{
	const char *name ="?";
	char        su = ' ';

	if (type->name)
		name = type->name;
	if (type->meta == ty_union)
		su = '-';
	else
		su = '=';
	dasprintf (encoding, "{%s%c}", name, su);
}

static void
encode_enum (dstring_t *encoding, const type_t *type)
{
	const char *name ="?";

	if (type->name)
		name = type->name;
	dasprintf (encoding, "{%s#}", name);
}

void
encode_type (dstring_t *encoding, const type_t *type)
{
	if (!type)
		return;
	if (type->attributes && is_func (type) && type->func.attribute_bits) {
		dstring_appendstr (encoding, "%");
	}
	switch (type->meta) {
		case ty_meta_count:
			break;
		case ty_algebra:
			algebra_encode_type (encoding, type);
			return;
		case ty_handle:
			dasprintf (encoding, "{%s$}", type->name);
			return;
		case ty_alias:
			dasprintf (encoding, "{%s>", type->name ? type->name : "");
			encode_type (encoding, type->alias.full_type);
			dasprintf (encoding, "}");
			return;
		case ty_class:
			encode_class (encoding, type);
			return;
		case ty_enum:
			encode_enum (encoding, type);
			return;
		case ty_struct:
		case ty_union:
			encode_struct (encoding, type);
			return;
		case ty_array:
			dasprintf (encoding, "[");
			dasprintf (encoding, "%d", type->array.count);
			if (type->array.base)
				dasprintf (encoding, ":%d", type->array.base);
			dasprintf (encoding, "=");
			encode_type (encoding, type->array.type);
			dasprintf (encoding, "]");
			return;
		case ty_bool:
			char bool_char = type->type == ev_int ? 'b' : 'B';
			if (type->width > 1) {
				dasprintf (encoding, "%c%d", bool_char, type->width);
			} else {
				dasprintf (encoding, "%c", bool_char);
			}
			return;
		case ty_basic:
			switch (type->type) {
				case ev_void:
					dasprintf (encoding, "v");
					return;
				case ev_string:
					dasprintf (encoding, "*");
					return;
				case ev_double:
					if (type->columns > 1) {
						dasprintf (encoding, "d%d%d",
								   type->columns, type->width);
					} else if (type->width > 1) {
						dasprintf (encoding, "d%d", type->width);
					} else {
						dasprintf (encoding, "d");
					}
					return;
				case ev_float:
					if (type->columns > 1) {
						dasprintf (encoding, "f%d%d",
								   type->columns, type->width);
					} else if (type->width > 1) {
						dasprintf (encoding, "f%d", type->width);
					} else {
						dasprintf (encoding, "f");
					}
					return;
				case ev_vector:
					dasprintf (encoding, "V");
					return;
				case ev_entity:
					dasprintf (encoding, "E");
					return;
				case ev_field:
					dasprintf (encoding, "F");
					encode_type (encoding, type->fldptr.type);
					return;
				case ev_func:
					dasprintf (encoding, "(");
					encode_type (encoding, type->func.ret_type);
					dasprintf (encoding, "%s)", encode_params (type));
					return;
				case ev_ptr:
					if (is_id(type)) {
						dasprintf (encoding, "@");
						return;
					}
					if (is_SEL(type)) {
						dasprintf (encoding, ":");
						return;
					}
					if (is_Class(type)) {
						dasprintf (encoding, "#");
						return;
					}
					dasprintf (encoding, type->fldptr.deref ? "&" : "^");
					if (type->fldptr.tag) {
						dasprintf (encoding, "%d", type->fldptr.tag);
					}
					type = type->fldptr.type;
					encode_type (encoding, type);
					return;
				case ev_quaternion:
					dasprintf (encoding, "Q");
					return;
				case ev_int:
					if (type->width > 1) {
						dasprintf (encoding, "i%d", type->width);
					} else {
						dasprintf (encoding, "i");
					}
					return;
				case ev_uint:
					if (type->width > 1) {
						dasprintf (encoding, "I%d", type->width);
					} else {
						dasprintf (encoding, "I");
					}
					return;
				case ev_long:
					if (type->width > 1) {
						dasprintf (encoding, "l%d", type->width);
					} else {
						dasprintf (encoding, "l");
					}
					return;
				case ev_ulong:
					if (type->width > 1) {
						dasprintf (encoding, "L%d", type->width);
					} else {
						dasprintf (encoding, "L");
					}
					return;
				case ev_short:
					dasprintf (encoding, "s");
					return;
				case ev_ushort:
					dasprintf (encoding, "S");
					return;
				case ev_invalid:
				case ev_type_count:
				break;
			}
			break;
	}
	internal_error (0, "bad type meta:type %d:%d", type->meta, type->type);
}

#define EV_TYPE(t) \
bool is_##t (const type_t *type) \
{ \
	type = unalias_type (type); \
	if (type->meta != ty_basic && type->meta != ty_algebra) { \
		return false; \
	} \
	return type->type == ev_##t; \
}
#include "QF/progs/pr_type_names.h"

bool
is_auto (const type_t *type)
{
	return type == &type_auto;
}

bool
is_pointer (const type_t *type)
{
	type = unalias_type (type);
	return is_ptr (type) && !type->fldptr.deref;
}

bool
is_reference (const type_t *type)
{
	type = unalias_type (type);
	return is_ptr (type) && type->fldptr.deref;
}

bool
is_enum (const type_t *type)
{
	type = unalias_type (type);
	if (type->type == ev_invalid && type->meta == ty_enum)
		return true;
	return false;
}

bool
is_bool (const type_t *type)
{
	type = unalias_type (type);
	if (type->meta != ty_bool) {
		return false;
	}
	return type->type == ev_int || type->type == ev_float;
}

bool
is_lbool (const type_t *type)
{
	type = unalias_type (type);
	if (type->meta != ty_bool) {
		return false;
	}
	return type->type == ev_long;
}

bool
is_boolean (const type_t *type)
{
	return is_bool (type) || is_lbool (type);
}

bool
is_signed (const type_t *type)
{
	type = unalias_type (type);
	if (is_int (type) || is_long (type) || is_short (type)) {
		return true;
	}
	return false;
}

bool
is_unsigned (const type_t *type)
{
	type = unalias_type (type);
	if (is_uint (type) || is_ulong (type) || is_ushort (type)) {
		return true;
	}
	return false;
}

bool
is_integral (const type_t *type)
{
	if (is_signed (type) || is_unsigned (type))
		return true;
	return is_enum (type);
}

bool
is_real (const type_t *type)
{
	type = unalias_type (type);
	return is_float (type) || is_double (type);
}

bool
is_scalar (const type_t *type)
{
	type = unalias_type (type);
	if (is_short (type) || is_ushort (type)) {
		// shorts have width 0
		return true;
	}
	if (type->width != 1) {
		return false;
	}
	return is_real (type) || is_integral (type) || is_boolean (type);
}

bool
is_matrix (const type_t *type)
{
	if (!type || type->meta != ty_basic) {
		return false;
	}
	return type->columns > 1;
}

bool
is_nonscalar (const type_t *type)
{
	type = unalias_type (type);
	if (is_vector (type) || is_quaternion (type)) {
		return true;
	}
	if (type->width < 2) {
		return false;
	}
	if (type->columns > 1) {
		// while matrices are technically non-scalar, treat them as both
		// niether scalar nor non-scalar for type checking
		return false;
	}
	return is_real (type) || is_integral (type) || is_boolean (type);
}

bool
is_math (const type_t *type)
{
	type = unalias_type (type);

	if (is_vector (type) || is_quaternion (type)) {
		return true;
	}
	if (is_algebra (type)) {
		return true;
	}
	return is_scalar (type) || is_nonscalar (type) || is_matrix (type);
}

bool
is_struct (const type_t *type)
{
	type = unalias_type (type);
	if (type->type == ev_invalid && type->meta == ty_struct)
		return true;
	return false;
}

bool
is_handle (const type_t *type)
{
	type = unalias_type (type);
	if (type->meta == ty_handle)
		return true;
	return false;
}

bool
is_union (const type_t *type)
{
	type = unalias_type (type);
	if (type->type == ev_invalid && type->meta == ty_union)
		return true;
	return false;
}

bool
is_array (const type_t *type)
{
	type = unalias_type (type);
	if (type->type == ev_invalid && type->meta == ty_array)
		return true;
	return false;
}

bool
is_structural (const type_t *type)
{
	type = unalias_type (type);
	return is_struct (type) || is_union (type) || is_array (type);
}

bool
type_compatible (const type_t *dst, const type_t *src)
{
	if (!dst || !src) {
		return false;
	}

	// same type
	if (dst == src) {
		return true;
	}
	if (is_field (dst) && is_field (src)) {
		return true;
	}
	if (is_func (dst) && is_func (src)) {
		return true;
	}
	if (is_pointer (dst) && is_pointer (src)) {
		return true;
	}
	return false;
}

bool
type_assignable (const type_t *dst, const type_t *src)
{
	if (!dst || !src) {
		return false;
	}

	dst = unalias_type (dst);
	src = unalias_type (src);
	// same type
	if (dst == src)
		return true;
	// any field = any field
	if (dst->type == ev_field && src->type == ev_field)
		return true;
	if (is_pointer (dst) && is_func (src)) {
		auto type = dereference_type (dst);
		return type == src;
	}
	// pointer = array
	if (is_pointer (dst) && is_array (src)) {
		if (is_void (dst->fldptr.type)
			|| dst->fldptr.type == src->array.type)
			return true;
		return false;
	}
	if (!is_pointer (dst) || !is_pointer (src)) {
		if (is_algebra (dst) || is_algebra (src)) {
			return algebra_type_assignable (dst, src);
		}
		if (is_scalar (dst) && is_scalar (src)) {
			return true;
		}
		if (is_nonscalar (dst) && is_nonscalar (src)
			&& type_width (dst) == type_width (src)) {
			return true;
		}
		if ((is_int (dst) || is_uint (dst) || is_float (dst))
			&& is_bool (src)) {
			return true;
		}
		if (is_enum (dst) && is_bool (src)) {
			return true;
		}
		if ((is_long (dst) || is_ulong (dst) || is_double (dst))
			&& is_boolean (src)) {
			return true;
		}
		if (is_lbool (dst) && is_bool (src)) {
			return true;
		}
		return false;
	}

	// pointer = pointer
	// give the object system first shot because the pointee types might have
	// protocols attached.
	int ret = obj_types_assignable (dst, src);
	// ret < 0 means obj_types_assignable can't decide
	if (ret >= 0)
		return ret;

	dst = dst->fldptr.type;
	src = src->fldptr.type;
	if (dst == src) {
		return true;
	}
	if (is_void (dst))
		return true;
	if (is_void (src))
		return true;
	return false;
}

#define P(type) (1 << ev_##type)
static unsigned promote_masks[ev_type_count] = {
	[ev_float] = P(int) | P(uint),
	[ev_int] = P(short) | P(ushort),
	[ev_uint] = P(int) | P(short) | P(ushort),
	[ev_double] = P(float) | P(ulong) | P(long)
				| P(int) | P(uint) | P(short) | P(ushort),
	[ev_long] = P(int) | P(uint) | P(short) | P(ushort),
	[ev_ulong] = P(long) | P(int) | P(uint) | P(short) | P(ushort),
	[ev_ushort] = P(short),
};
#undef P

bool
type_promotes (const type_t *dst, const type_t *src)
{
	if (!dst || !src) {
		return false;
	}

	dst = unalias_type (dst);
	src = unalias_type (src);
	if (!is_math (dst) || !is_math (src)) {
		return false;
	}
	if (type_cols (dst) != type_cols (src)
		|| type_rows (dst) != type_rows (src)) {
		return false;
	}

	if (is_bool (src)) {
		return !is_short (dst) && !is_ushort (dst);
	}
	if (is_lbool (src)) {
		return is_long (dst) || is_ulong (dst) || is_double (dst);
	}
	if (is_enum (src)) {
		return is_integral (dst) && !is_enum (dst);
	}

	dst = base_type (dst);
	src = base_type (src);
	int dst_mask = promote_masks[dst->type];
	return dst_mask & (1 << src->type);
}

#define D(type) (1 << ev_##type)
static unsigned demote_masks[ev_type_count] = {
	// FIXME should this be a complete matrix, or just from implicit constants?
	// currently implicit constants are assumed
	[ev_float] = D(double) | D(int) | D(long),
	[ev_int] = D(long),
	[ev_uint] = D(long) | D(int),
	[ev_double] = D(int),
};
#undef D

bool
type_demotes (const type_t *dst, const type_t *src)
{
	if (!dst || !src) {
		return false;
	}

	dst = unalias_type (dst);
	src = unalias_type (src);
	if (!is_math (dst) || !is_math (src)) {
		return false;
	}

	if (is_boolean (dst)) {
		return is_lbool (src) || is_long (src);
	}

	dst = base_type (dst);
	src = base_type (src);
	int dst_mask = demote_masks[dst->type];
	return dst_mask & (1 << src->type);
}

#define C(type) (1 << ev_##type)
static unsigned compare_masks[ev_type_count] = {
	[ev_float] = C(float),
	[ev_int] = C(int) | C(long),
	[ev_uint] = C(uint) | C(ulong),
	[ev_double] = C(double),
	[ev_long] = C(long) | C(int),
	[ev_ulong] = C(ulong) | C(uint),
};
#undef C

bool
type_compares (const type_t *dst, const type_t *src)
{
	if (!dst || !src) {
		return false;
	}

	dst = unalias_type (dst);
	src = unalias_type (src);
	if (dst == src) {
		return true;
	}
	if (type_rows (dst) != type_rows (src)
		|| type_cols (dst) != type_cols (src)) {
		return false;
	}
	if (!is_math (dst) || !is_math (src)) {
		return false;
	}

	if (is_boolean (dst) || is_boolean (src)) {
		// comparison between bool and lbool are ok, but not between either
		// bool and any other type
		return is_boolean (dst) && is_boolean (src);
	}
	if (is_enum (src)) {
		return is_integral (dst) && !is_enum (dst);
	}
	if (is_enum (dst)) {
		return is_integral (src) && !is_enum (src);
	}

	dst = base_type (dst);
	src = base_type (src);
	int dst_mask = compare_masks[dst->type];
	return dst_mask & (1 << src->type);
}

bool
type_same (const type_t *dst, const type_t *src)
{
	dst = unalias_type (dst);
	src = unalias_type (src);

	return dst == src;
}

int
type_size (const type_t *type)
{
	switch (type->meta) {
		case ty_handle:
		case ty_bool:
		case ty_basic:
			if (type->type != ev_short && (!type->columns || !type->width)) {
				internal_error (0, "%s:%d:%d", pr_type_name[type->type],
								type->columns, type->width);
			}
			return pr_type_size[type->type] * type->width * type->columns;
		case ty_struct:
		case ty_union:
			if (!type->symtab)
				return 0;
			return type->symtab->size;
		case ty_enum:
			if (!type->symtab)
				return 0;
			return type_size (&type_int);
		case ty_array:
			return type->array.count * type_size (type->array.type);
		case ty_class:
			{
				class_t    *class = type->class;
				int         size;
				if (!class->ivars)
					return 0;
				size = class->ivars->size;
				if (class->super_class)
					size += type_size (class->super_class->type);
				return size;
			}
		case ty_alias:
			return type_size (type->alias.aux_type);
		case ty_algebra:
			return algebra_type_size (type);
		case ty_meta_count:
			break;
	}
	internal_error (0, "invalid type meta: %d", type->meta);
}

int
type_align (const type_t *type)
{
	type = unalias_type (type);
	return type->alignment;
}

int
type_count (const type_t *type)
{
	switch (type->meta) {
		case ty_handle:
		case ty_bool:
		case ty_basic:
		case ty_enum:
			if (type->type != ev_short && (!type->columns || !type->width)) {
				internal_error (0, "%s:%d:%d", pr_type_name[type->type],
								type->columns, type->width);
			}
			return type->width * type->columns;
		case ty_struct:
		case ty_union:
			if (!type->symtab)
				return 0;
			return type->symtab->count;
		case ty_array:
			return type->array.count;
		case ty_class:
			return 1;
		case ty_alias:
			return type_size (type->alias.aux_type);
		case ty_algebra:
			internal_error (0, "count of algebra type");
		case ty_meta_count:
			break;
	}
	internal_error (0, "invalid type meta: %d", type->meta);
}

int
type_width (const type_t *type)
{
	switch (type->meta) {
		case ty_bool:
		case ty_basic:
			if (type->type == ev_vector) {
				return 3;
			}
			if (type->type == ev_quaternion) {
				return 4;
			}
			return type->width;
		case ty_handle:
		case ty_struct:
		case ty_union:
			return 1;
		case ty_enum:
			if (!type->symtab)
				return 0;
			return type_width (&type_int);
		case ty_array:
			return type_width (type->array.type);
		case ty_class:
			return 1;
		case ty_alias:
			return type_width (type->alias.aux_type);
		case ty_algebra:
			return algebra_type_width (type);
		case ty_meta_count:
			break;
	}
	internal_error (0, "invalid type meta: %d", type->meta);
}

int
type_cols (const type_t *type)
{
	switch (type->meta) {
		case ty_bool:
		case ty_basic:
			return type->columns;
		case ty_handle:
		case ty_struct:
		case ty_union:
			return 1;
		case ty_enum:
			if (!type->symtab)
				return 0;
			return type_cols (&type_int);
		case ty_array:
			return type_cols (type->array.type);
		case ty_class:
			return 1;
		case ty_alias:
			return type_cols (type->alias.aux_type);
		case ty_algebra:
			return 1;
		case ty_meta_count:
			break;
	}
	internal_error (0, "invalid type meta: %d", type->meta);
}

int
type_rows (const type_t *type)
{
	// vectors are vertical (includes vector and quaternion), other types
	// have width of 1, thus rows == width
	return type_width (type);
}

int
type_aligned_size (const type_t *type)
{
	int         size = type_size (type);
	return RUP (size, type->alignment);
}

symtab_t *
type_symtab (const type_t *type)
{
	type = unalias_type (type);
	if (is_struct (type) || is_union (type)
		|| is_enum (type) || is_entity (type)) {
		return type->symtab;
	}
	return nullptr;
}

static void
chain_basic_types (void)
{
	type_registry.size = 0;
	type_encodings.size = 0;
	DARRAY_APPEND (&type_registry, nullptr);
	DARRAY_APPEND (&type_encodings, nullptr);

	type_entity.symtab = pr.entity_fields;
	if (options.code.progsversion == PROG_VERSION) {
		type_quaternion.alignment = 4;
	}
	if (options.code.progsversion == PROG_ID_VERSION) {
		type_bool.type = ev_float;
	} else {
		type_bool.type = ev_int;
	}

	chain_type (&type_void);
	chain_type (&type_string);
	chain_type (&type_float);
	chain_type (&type_vector);
	chain_type (&type_entity);
	chain_type (&type_field);
	chain_type (&type_func);
	chain_type (&type_ptr);
	chain_type (&type_floatfield);
	chain_type (&type_bool);
	if (!options.traditional) {
		chain_type (&type_quaternion);
		chain_type (&type_int);
		chain_type (&type_uint);
		chain_type (&type_short);
		chain_type (&type_double);

		if (options.code.progsversion == PROG_VERSION) {
			chain_type (&type_long);
			chain_type (&type_ulong);
			chain_type (&type_ushort);
#define VEC_TYPE(name, type) chain_type (&type_##name);
#include "tools/qfcc/include/vec_types.h"
#define MAT_TYPE(name, type, cols, align_as) chain_type (&type_##name);
#include "tools/qfcc/include/mat_types.h"
			chain_type (&type_bvec2);
			chain_type (&type_bvec3);
			chain_type (&type_bvec4);
			chain_type (&type_lbool);
			chain_type (&type_lbvec2);
			chain_type (&type_lbvec3);
			chain_type (&type_lbvec4);
		}
	}
}

static void
chain_structural_types (void)
{
	chain_type (&type_param);
	chain_type (&type_param_pointer);
	chain_type (&type_zero);
	chain_type (&type_type_encodings);
	chain_type (&type_xdef);
	chain_type (&type_xdef_pointer);
	chain_type (&type_xdefs);
	chain_type (&type_va_list);
}

void
chain_initial_types (void)
{
	Hash_FlushTable (type_tab);
	chain_basic_types ();
	chain_structural_types ();
}

static const char *vector_field_names[] = { "x", "y", "z", "w" };
static const char *color_field_names[] = { "r", "g", "b", "a" };
static const char *texture_field_names[] = { "s", "t", "p", "q" };

static void
build_vector_struct (type_t *type, bool extra_names)
{
	ty_meta_e   meta = type->meta;
	etype_t     etype = type->type;
	auto ele_type = base_type (type);
	int         width = type_width (type);

	if (!ele_type || width < 2 || width > 4) {
		internal_error (0, "%s not a vector type: %p %d", type->name, ele_type, width);
	}

	struct_def_t fields[3 * (width + 1)] = {};
	for (int i = 0; i < width; i++) {
		auto v = &fields[i + 0 * (width + 1)];
		auto c = &fields[i + 1 * (width + 1)];
		auto t = &fields[i + 2 * (width + 1)];
		*v = (struct_def_t) { vector_field_names[i], ele_type };
		*c = (struct_def_t) { color_field_names[i], ele_type };
		*t = (struct_def_t) { texture_field_names[i], ele_type };
	}
	if (extra_names) {
		// these slots were zero-initialized so filling them in with a
		// reset field enables the additional component names
		fields[1 * (width + 1) - 1] = (struct_def_t) { ".reset" };
		fields[2 * (width + 1) - 1] = (struct_def_t) { ".reset" };
	}

	make_structure (va (0, "@%s", type->name), 's', fields, type);
	type->type = etype;
	type->meta = meta;
}

static const char *
type_get_key (const void *_t, void *data)
{
	const type_t *t = _t;
	return t->encoding;
}

void
init_types (void)
{
	static struct_def_t zero_struct[] = {
		{"string_val",       &type_string},
		{"float_val",        &type_float},
		{"entity_val",       &type_entity},
		{"field_val",        &type_field},
		{"func_val",         &type_func},
		{"pointer_val",      &type_ptr},
		{"vector_val",       &type_vector},
		{"int_val",          &type_int},
		{"uint_val",         &type_uint},
		{"int_val",          &type_int},
		{"uint_val",         &type_uint},
		{"quaternion_val",   &type_quaternion},
		{"double_val",       &type_double},
		{0, 0}
	};
	static struct_def_t param_struct[] = {
		{"string_val",       &type_string},
		{"float_val",        &type_float},
		{"vector_val",       &type_vector},
		{"entity_val",       &type_entity},
		{"field_val",        &type_field},
		{"func_val",         &type_func},
		{"pointer_val",      &type_ptr},
		{"int_val",          &type_int},
		{"uint_val",         &type_uint},
		{"quaternion_val",   &type_quaternion},
		{"double_val",       &type_double},
		{0, 0}
	};
	static struct_def_t type_encoding_struct[] = {
		{"types",	&type_ptr},
		{"size",	&type_uint},
		{0, 0}
	};
	static struct_def_t xdef_struct[] = {
		{"types",	&type_ptr},
		{"offset",	&type_ptr},
		{0, 0}
	};
	static struct_def_t xdefs_struct[] = {
		{"xdefs",	&type_xdef_pointer},
		{"num_xdefs", &type_uint},
		{0, 0}
	};
	static struct_def_t va_list_struct[] = {
		{"count", &type_int},
		{"list",  0},				// type will be filled in at runtime
		{0, 0}
	};

	type_tab = Hash_NewTable (1021, type_get_key, 0, 0, 0);

	chain_basic_types ();

	type_nil = &type_quaternion;
	type_default = &type_int;
	type_long_int = &type_long;
	type_ulong_uint = &type_ulong;
	if (options.code.progsversion < PROG_VERSION) {
		// even v6p doesn't support long, and I don't feel like adding it
		// use ruamoko :P
		type_long_int = &type_int;
		type_ulong_uint = &type_uint;
	}
	if (options.code.progsversion == PROG_ID_VERSION) {
		// vector can't be part of .zero for v6 progs because for v6 progs,
		// .zero is only one word wide.
		zero_struct[6].name = 0;
		// v6 progs don't have integers or quaternions
		param_struct[7].name = 0;
		type_nil = &type_vector;
		type_default = &type_float;
	}

	make_structure ("@zero", 'u', zero_struct, &type_zero);
	make_structure ("@param", 'u', param_struct, &type_param);

	build_vector_struct (&type_vector, false);

	make_structure ("@type_encodings", 's', type_encoding_struct,
					&type_type_encodings);
	make_structure ("@xdef", 's', xdef_struct, &type_xdef);
	make_structure ("@xdefs", 's', xdefs_struct, &type_xdefs);

	va_list_struct[1].type = &type_param_pointer;
	make_structure ("@va_list", 's', va_list_struct, &type_va_list);

	build_vector_struct (&type_quaternion, false);
	{
		symbol_t   *sym;

		sym = new_symbol_type ("v", &type_vector);
		sym->offset = 0;
		symtab_addsymbol (type_quaternion.symtab, sym);

		sym = new_symbol_type ("s", &type_float);
		sym->offset = 3;
		symtab_addsymbol (type_quaternion.symtab, sym);
	}
#define VEC_TYPE(type_name, base_type) \
	build_vector_struct (&type_##type_name, true);
#include "tools/qfcc/include/vec_types.h"

	chain_structural_types ();
}
