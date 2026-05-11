/*
	value.h

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

#ifndef __value_h
#define __value_h

/** \defgroup qfcc_value Constant values.
	\ingroup qfcc_expr
*/
///@{

typedef struct def_s def_t;
typedef struct expr_s expr_t;
typedef struct ex_value_s ex_value_t;
typedef struct type_s type_t;
typedef struct pr_type_s pr_type_t;
typedef struct operand_s operand_t;
typedef struct defspace_s defspace_t;

ex_value_t *new_value (void);
ex_value_t *new_string_val (const char *string_val);
ex_value_t *new_buffer_val (const char *buffer, size_t size);
ex_value_t *new_double_val (double double_val);
ex_value_t *new_float_val (float float_val);
ex_value_t *new_vector_val (const float *vector_val);
ex_value_t *new_entity_val (int entity_val);
ex_value_t *new_field_val (int field_val, const type_t *type, def_t *def);
ex_value_t *new_func_val (int func_val, const type_t *type);
ex_value_t *new_pointer_val (int val, const type_t *type, def_t *def,
							 operand_t *tempop);
ex_value_t *new_quaternion_val (const float *quaternion_val);
ex_value_t *new_int_val (int int_val);
ex_value_t *new_uint_val (int uint_val);
ex_value_t *new_long_val (pr_long_t long_val);
ex_value_t *new_ulong_val (pr_ulong_t ulong_val);
ex_value_t *new_short_val (short short_val);
ex_value_t *new_ushort_val (unsigned short ushort_val);
ex_value_t *new_nil_val (const type_t *type);
ex_value_t *new_type_value (const type_t *type, const pr_type_t *data);
void value_store (pr_type_t *dst, const type_t *dstType, const expr_t *src);
const char *get_value_string (const ex_value_t *value);

ex_value_t *offset_alias_value (ex_value_t *value, const type_t *type,
								int offset);
ex_value_t *alias_value (ex_value_t *value, const type_t *type);
def_t *emit_value (ex_value_t *value, def_t *def);
def_t *emit_value_core (ex_value_t *value, def_t *def, defspace_t *defspace);

int	ReuseString (const char *str);

void clear_immediates (void);

///@}

#endif//__value_h
