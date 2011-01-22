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

	$Id$
*/

#ifndef __type_h
#define __type_h

#include "QF/pr_comp.h"

typedef struct ty_func_s {
	struct type_s *type;
	int         num_params;
	struct type_s *param_types[MAX_PARMS];
} ty_func_t;

typedef struct ty_fldptr_s {
	struct type_s *type;
} ty_fldptr_t;

typedef struct ty_array_s {
	struct type_s *type;
	int         base;
	int         size;
} ty_array_t;

typedef enum {
	ty_none,				///< func/field/pointer or not used
	ty_struct,
	ty_union,
	ty_enum,
	ty_array,
	ty_class,
} ty_type_e;

typedef struct type_s {
	etype_t     type;		///< ev_invalid means structure/array etc
	const char *name;
	/// function/pointer/array/struct types are more complex
	ty_type_e   ty;
	union {
		ty_func_t   func;
		ty_fldptr_t fldptr;
		ty_array_t  array;
		struct symtab_s *symtab;
		struct class_s *class;
	} t;
	struct type_s *next;
} type_t;

extern	type_t	type_invalid;
extern	type_t	type_void;
extern	type_t	type_string;
extern	type_t	type_float;
extern	type_t	type_vector;
extern	type_t	type_entity;
extern	type_t	type_field;
extern	type_t	type_function;
extern	type_t	type_pointer;
extern	type_t	type_floatfield;
extern	type_t	type_quaternion;
extern	type_t	type_integer;
extern	type_t	type_short;

extern	type_t	*type_nil;		// for passing nil into ...

extern	type_t	type_id;
extern	type_t	type_Class;
extern	type_t	type_Protocol;
extern	type_t	type_SEL;
extern	type_t	type_IMP;
extern  type_t  type_supermsg;
extern	type_t	type_obj_exec_class;
extern	type_t	type_Method;
extern	type_t	type_method_description;
extern	type_t	type_category;
extern	type_t	type_ivar;
extern	type_t	type_module;
extern	type_t	type_Super;
extern	type_t	type_va_list;
extern	type_t	type_param;
extern	type_t	type_zero;

extern struct symtab_s *vector_struct;
extern struct symtab_s *quaternion_struct;

struct dstring_s;

type_t *new_type (void);
type_t *find_type (type_t *new);
void new_typedef (const char *name, type_t *type);
type_t *field_type (type_t *aux);
type_t *pointer_type (type_t *aux);
type_t *array_type (type_t *aux, int size);
type_t *based_array_type (type_t *aux, int base, int top);
void print_type_str (struct dstring_s *str, type_t *type);
void print_type (type_t *type);
const char *encode_params (type_t *type);
void encode_type (struct dstring_s *encoding, type_t *type);
type_t *parse_type (const char *str);
int is_scalar (type_t *type);
int is_struct (type_t *type);
int is_class (type_t *type);
int is_array (type_t *type);
int type_assignable (type_t *dst, type_t *src);
int type_size (type_t *type);

void init_types (void);
void chain_initial_types (void);

void clear_typedefs (void);

#endif//__type_h
