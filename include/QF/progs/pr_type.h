/*
	pr_type.h

	object file type encoding support

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/02/18

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

#ifndef __QF_pr_type_h
#define __QF_pr_type_h

/** \defgroup qfcc_qfo_type Object file type encoding
	\ingroup progs

	All \c pr_ptr_t \c type fields are pointers within the type qfo_space.
*/
///@{

#include "QF/progs/pr_comp.h"

typedef enum {
	ty_basic,				///< VM type (float, int, pointer, field, etc)
	ty_struct,
	ty_union,
	ty_enum,
	ty_array,
	ty_class,
	ty_alias,
	ty_handle,
	ty_algebra,
	ty_bool,

	ty_meta_count
} ty_meta_e;

typedef struct qfot_alias_s {
	etype_t     type;				///< type at end of alias chain
	pr_ptr_t    aux_type;			///< referenced type: stripped of aliases
	pr_ptr_t    full_type;			///< includes full alias info
	pr_string_t name;				///< alias name, may be null
} qfot_alias_t;

typedef struct qfot_handle_s {
	etype_t     type;
	pr_string_t tag;
} qfot_handle_t;

typedef struct qfot_fldptr_s {
	etype_t     type;				///< ev_field or ev_ptr
	pr_ptr_t    aux_type;			///< referenced type
} qfot_fldptr_t;

typedef struct qfot_basic_s {
	etype_t     type;				///< integral and fp scalar types
	pr_int_t    width;				///< components in vector (1 for vector or
									///< quaternion), rows in matrix
	pr_int_t    columns;			///< columns in matrix (or 1)
} qfot_basic_t;

typedef struct qfot_func_s {
	etype_t     type;				///< always ev_func
	pr_ptr_t    return_type;		///< return type of the function
	pr_int_t    num_params;			///< ones compliment count of the
									///< parameters. -ve values indicate the
									///< number of real parameters before the
									///< ellipsis
	pr_ptr_t    param_types[1];	///< variable length list of parameter
									///< types
} qfot_func_t;

typedef struct qfot_var_s {
	pr_ptr_t    type;				///< type of field or self reference for
									///< enum
	pr_string_t name;				///< name of field/enumerator
	pr_int_t    offset;				///< value for enum, 0 for union
} qfot_var_t;

typedef struct qfot_struct_s {
	pr_string_t tag;				///< struct/union/enum tag
	pr_int_t    num_fields;			///< number of fields/enumerators
	qfot_var_t  fields[1];			///< variable length list of
									///< fields/enumerators
} qfot_struct_t;

typedef struct qfot_array_s {
	pr_ptr_t    type;				///< element type
	pr_int_t    base;				///< start index of array
	pr_int_t    count;				///< number of elements in array
} qfot_array_t;

typedef struct qfot_algebra_s {
	etype_t     type;				///< always ev_float or ev_double
	pr_int_t    width;				///< components in multivector
	pr_ptr_t    algebra;			///< algebra descriptor
	pr_int_t    element;			///< element within algebra
} qfot_algebra_t;

/**	QFO type encoding.

	\note As this holds a union of all type representations, and those
	representations may contain variable arrays, sizeof() will return only
	one, rather useless, value. It is also not suitable for direct use in
	arrays.
*/
typedef struct qfot_type_s {
	ty_meta_e   meta;				///< meta type
	pr_uint_t   size;				///< total word size of this encoding
	pr_string_t encoding;			///< Objective-QC encoding
	union {
		etype_t     type;			///< ty_basic: etype_t
		qfot_basic_t basic;			///< ty_basic: int/float/double/long etc
		qfot_fldptr_t fldptr;		///< ty_basic, ev_ptr/ev_field
		qfot_func_t func;			///< ty_basic, ev_func
		qfot_struct_t strct;		///< ty_struct/ty_union/ty_enum
		qfot_array_t array;			///< ty_array
		pr_string_t class;			///< ty_class
		qfot_alias_t alias;			///< ty_alias
		qfot_handle_t handle;		///< ty_handle
		qfot_algebra_t algebra;		///< ty_algebra
	};
} qfot_type_t;

typedef struct qfot_type_encodings_s {
	pr_ptr_t    types;
	pr_uint_t   size;
} qfot_type_encodings_t;

///@}

#endif//__QF_pr_type_h
