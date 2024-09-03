/*
	type.h

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

#ifndef __type_h
#define __type_h

#include "QF/progs/pr_type.h"

#include "specifier.h"

typedef enum param_qual_e param_qual_t;
typedef struct ty_func_s {
	const struct type_s *ret_type;
	int         num_params;
	const struct type_s **param_types;
	param_qual_t *param_quals;
	union {
		struct {
			unsigned    no_va_list:1;///< don't inject va_list for ... function
			unsigned    void_return:1;///< special handling for return value
		};
		unsigned    attribute_bits;
	};
} ty_func_t;

typedef struct ty_fldptr_s {
	const struct type_s *type;
} ty_fldptr_t;

typedef struct ty_array_s {
	const struct type_s *type;
	int         base;
	int         size;
} ty_array_t;

typedef struct ty_alias_s {
	const struct type_s *aux_type;	///< other aliases stripped
	const struct type_s *full_type;	///< full alias chain
} ty_alias_t;

typedef struct type_s {
	etype_t     type;		///< ev_invalid means structure/array etc
	const char *name;
	int         alignment;	///< required alignment for instances
	int         width;		///< components in vector types, otherwise 1
							///< vector and quaternion are 1 (separate from
							///< vec3 and vec4)
							///< also, rows in matrix types
	int         columns;	///< for matrix types, otherwise 1
	/// function/pointer/array/struct types are more complex
	ty_meta_e   meta;
	union {
		// no data for ty_basic when not a func, field or pointer
		ty_func_t   func;
		ty_fldptr_t fldptr;
		ty_array_t  array;
		struct symtab_s *symtab;
		struct class_s *class;
		struct algebra_s *algebra;
		struct multivector_s *multivec;
		ty_alias_t  alias;
	};
	struct type_s *next;
	int         freeable;
	int         allocated;
	int         printid;	///< for dot output
	struct protocollist_s *protos;
	const char *encoding;	///< Objective-QC encoding
	struct def_s *type_def;	///< offset of qfo encodoing
} type_t;

#define EV_TYPE(type) extern type_t type_##type;
#include "QF/progs/pr_type_names.h"

#define VEC_TYPE(type_name, base_type) extern type_t type_##type_name;
#include "tools/qfcc/include/vec_types.h"

#define MAT_TYPE(type_name, base_type, cols, align_as) \
	extern type_t type_##type_name;
#include "tools/qfcc/include/mat_types.h"

extern	type_t  type_bool;
extern	type_t  type_bvec2;
extern	type_t  type_bvec3;
extern	type_t  type_bvec4;
extern	type_t	type_auto;
extern	type_t	type_invalid;
extern	type_t	type_floatfield;

extern	type_t	*type_nil;		// for passing nil into ...
extern	type_t	*type_default;	// default type (float or int)
extern	type_t	*type_long_int;	// supported type for long
extern	type_t	*type_ulong_uint;// supported type for ulong

extern	type_t	type_va_list;
extern	type_t	type_param;
extern	type_t	type_zero;
extern	type_t	type_type_encodings;
extern	type_t	type_xdef;
extern	type_t	type_xdef_pointer;
extern	type_t	type_xdefs;

extern struct symtab_s *vector_struct;
extern struct symtab_s *quaternion_struct;

struct dstring_s;

etype_t low_level_type (const type_t *type) __attribute__((pure));
type_t *new_type (void);
void free_type (type_t *type);
void chain_type (type_t *type);

/**	Append a type to the end of a type chain.

	The type chain must be made up of only field, pointer, function, and
	array types, as other types do not have auxiliary type fields.

	\param type		The type chain to which the type will be appended.
	\param new		The type to be appended. May be any type.
	\return			The type chain with the type appended at the deepest
					level.
*/
const type_t *append_type (const type_t *type, const type_t *new);
void set_func_type_attrs (const type_t *func, specifier_t spec);
specifier_t default_type (specifier_t spec, const struct symbol_s *sym);
const type_t *find_type (const type_t *new);
void new_typedef (const char *name, type_t *type);
const type_t *field_type (const type_t *aux);
const type_t *pointer_type (const type_t *aux);
const type_t *vector_type (const type_t *ele_type, int width) __attribute__((pure));
const type_t *matrix_type (const type_t *ele_type, int cols, int rows) __attribute__((pure));
const type_t *base_type (const type_t *vec_type) __attribute__((pure));

/** Return an integral type of same size as the provided type.

	Any 32-bit type will produce type_int (or one of ivec2, ivec3 or ivec4).
	Any 64-bit type will produce type_long (lor one of lvec2, lvec3, or lvec4).

	Both type_width() and type_size() of the returned type will match the
	provided type.

	\param base		Type on which the return type will be based.
	\return			Matching integral type (int, long, or a vector form), or
					null if no such match can be made.
*/
const type_t *int_type (const type_t *base) __attribute__((pure));
const type_t *uint_type (const type_t *base) __attribute__((pure));

/** Return a bool type of same size as the provided type.

	Any 32-bit type will produce type_bool (or one of bvec2, bvec3 or bvec4).
	Any 64-bit type will produce type_long (lor one of lvec2, lvec3, or lvec4).

	Both type_width() and type_size() of the returned type will match the
	provided type.

	\param base		Type on which the return type will be based.
	\return			Matching boolean type (bool, long, or a vector form), or
					null if no such match can be made.
	FIXME 64-bit bool types
*/
const type_t *bool_type (const type_t *base) __attribute__((pure));

/** Return a floating point type of same size as the provided type.

	Any 32-bit type will produce type_float (or one of vec2, vec3 or vec4).
	Any 64-bit type will produce type_double (lor one of dvec2, dvec3, or
	dvec4).

	Both type_width() and type_size() of the returned type will match the
	provided type.

	\param base		Type on which the return type will be based.
	\return			Matching floating point type (float, double, or a vector
					form), or null if no such match can be made.
*/
const type_t *float_type (const type_t *base) __attribute__((pure));

const type_t *array_type (const type_t *aux, int size);
const type_t *based_array_type (const type_t *aux, int base, int top);
const type_t *alias_type (const type_t *type, const type_t *alias_chain,
						  const char *name);
const type_t *unalias_type (const type_t *type) __attribute__((pure));
const type_t *dereference_type (const type_t *type) __attribute__((pure));
void print_type_str (struct dstring_s *str, const type_t *type);
const char *get_type_string (const type_t *type);
void print_type (const type_t *type);
void dump_dot_type (void *t, const char *filename);
const char *encode_params (const type_t *type);
void encode_type (struct dstring_s *encoding, const type_t *type);
const char *type_get_encoding (const type_t *type);

#define EV_TYPE(t) int is_##t (const type_t *type) __attribute__((pure));
#include "QF/progs/pr_type_names.h"

int is_enum (const type_t *type) __attribute__((pure));
int is_bool (const type_t *type) __attribute__((pure));
int is_integral (const type_t *type) __attribute__((pure));
int is_real (const type_t *type) __attribute__((pure));
int is_scalar (const type_t *type) __attribute__((pure));
int is_nonscalar (const type_t *type) __attribute__((pure));
int is_matrix (const type_t *type) __attribute__((pure));
int is_math (const type_t *type) __attribute__((pure));
int is_struct (const type_t *type) __attribute__((pure));
int is_union (const type_t *type) __attribute__((pure));
int is_array (const type_t *type) __attribute__((pure));
int is_structural (const type_t *type) __attribute__((pure));
int type_compatible (const type_t *dst, const type_t *src) __attribute__((pure));
int type_assignable (const type_t *dst, const type_t *src);
int type_promotes (const type_t *dst, const type_t *src) __attribute__((pure));
int type_same (const type_t *dst, const type_t *src) __attribute__((pure));
int type_size (const type_t *type) __attribute__((pure));
int type_width (const type_t *type) __attribute__((pure));
int type_rows (const type_t *type) __attribute__((pure));
int type_cols (const type_t *type) __attribute__((pure));
int type_aligned_size (const type_t *type) __attribute__((pure));

void init_types (void);
void chain_initial_types (void);

void clear_typedefs (void);

extern type_t *ev_types[];
extern int type_cast_map[];
#define TYPE_CAST_CODE(from, to, width) (((width) << 6) | ((from) << 3) | (to))

#endif//__type_h
