/*
	expr.h

	expression construction and manipulations

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/06/15

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

#ifndef __expr_h
#define __expr_h

#include "QF/pr_comp.h"

typedef enum {
	ex_error,
	ex_state,
	ex_bool,
	ex_label,
	ex_block,
	ex_expr,	// binary expression
	ex_uexpr,	// unary expression
	ex_def,
	ex_temp,	// temporary variable
	ex_nil,		// umm, nil, null. nuff said
	ex_name,

	ex_string,
	ex_float,
	ex_vector,
	ex_entity,
	ex_field,
	ex_func,
	ex_pointer,
	ex_quaternion,
	ex_integer,
	ex_uinteger,
	ex_short,
} expr_type;

typedef struct ex_label_s {
	struct ex_label_s *next;
	struct reloc_s *refs;
	int         ofs;
	const char *name;
} ex_label_t;

typedef struct {
	struct expr_s *head;
	struct expr_s **tail;
	struct expr_s *result;
	int			is_call;
} ex_block_t;

typedef struct {
	struct expr_s *expr;
	struct def_s *def;
	struct type_s *type;
	int			users;
} ex_temp_t;

typedef struct {
	int			val;
	struct type_s *type;
	struct def_s *def;
} ex_pointer_t;

typedef struct {
	int         size;
	struct expr_s *e[1];
} ex_list_t;

typedef struct {
	ex_list_t  *true_list;
	ex_list_t  *false_list;
	struct expr_s *e;
} ex_bool_t;

typedef struct {
	struct expr_s *frame;
	struct expr_s *think;
	struct expr_s *step;
} ex_state_t;

#define POINTER_VAL(p) (((p).def ? (p).def->ofs : 0) + (p).val)

typedef struct expr_s {
	struct expr_s *next;
	expr_type	type;
	int			line;
	string_t	file;
	unsigned	paren:1;
	unsigned	rvalue:1;
	union {
		ex_label_t label;
		ex_state_t state;
		ex_bool_t  bool;
		ex_block_t block;
		struct {
			int		op;
			struct type_s *type;
			struct expr_s *e1;
			struct expr_s *e2;
		}		expr;
		struct def_s *def;
		ex_temp_t temp;

		const char	*string_val;
		float	float_val;
		float	vector_val[3];
		int		entity_val;
		int		func_val;
		ex_pointer_t	pointer;
		float	quaternion_val[4];
		int		integer_val;
		unsigned int	uinteger_val;
		short	short_val;
	} e;
} expr_t;

extern etype_t qc_types[];
extern struct type_s *ev_types[];
extern expr_type expr_types[];

expr_t *type_mismatch (expr_t *e1, expr_t *e2, int op);

extern expr_t *local_expr;

struct type_s *get_type (expr_t *e);
etype_t extract_type (expr_t *e);

expr_t *new_expr (void);

const char *new_label_name (void);

expr_t *new_label_expr (void);
expr_t *new_state_expr (expr_t *frame, expr_t *think, expr_t *step);
expr_t *new_bool_expr (ex_list_t *true_list, ex_list_t *false_list, expr_t *e);
expr_t *new_block_expr (void);
expr_t *new_binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *new_unary_expr (int op, expr_t *e1);
expr_t *new_def_expr (struct def_s *def);
expr_t *new_temp_def_expr (struct type_s *type);
expr_t *new_nil_expr (void);
expr_t *new_name_expr (const char *name);

expr_t *new_string_expr (const char *string_val);
expr_t *new_float_expr (float float_val);
expr_t *new_vector_expr (float *vector_val);
expr_t *new_entity_expr (int entity_val);
expr_t *new_field_expr (int field_val, struct type_s *type, struct def_s *def);
expr_t *new_func_expr (int func_val);
expr_t *new_pointer_expr (int val, struct type_s *type, struct def_s *def);
expr_t *new_quaternion_expr (float *quaternion_val);
expr_t *new_integer_expr (int integer_val);
expr_t *new_uinteger_expr (unsigned int uinteger_val);
expr_t *new_short_expr (short short_val);

int is_constant (expr_t *e);
int is_compare (int op);
int is_logic (int op);
expr_t *constant_expr (expr_t *var);

expr_t *new_bind_expr (expr_t *e1, expr_t *e2);
expr_t *new_self_expr (void);
expr_t *new_this_expr (void);
expr_t *new_ret_expr (struct type_s *type);
expr_t *new_param_expr (struct type_s *type, int num);
expr_t *new_move_expr (expr_t *e1, expr_t *e2, struct type_s *type);

void inc_users (expr_t *e);
void convert_name (expr_t *e);

expr_t *append_expr (expr_t *block, expr_t *e);

void print_expr (expr_t *e);

void convert_int (expr_t *e);
void convert_uint (expr_t *e);
void convert_short (expr_t *e);
void convert_uint_int (expr_t *e);
void convert_int_uint (expr_t *e);
void convert_short_int (expr_t *e);
void convert_short_uint (expr_t *e);

expr_t *test_expr (expr_t *e, int test);
void backpatch (ex_list_t *list, expr_t *label);
expr_t *convert_bool (expr_t *e, int block);
expr_t *bool_expr (int op, expr_t *label, expr_t *e1, expr_t *e2);
expr_t *binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *asx_expr (int op, expr_t *e1, expr_t *e2);
expr_t *unary_expr (int op, expr_t *e);
expr_t *build_function_call (expr_t *fexpr, struct type_s *ftype,
							 expr_t *params);
expr_t *function_expr (expr_t *e1, expr_t *e2);
struct function_s;
expr_t *return_expr (struct function_s *f, expr_t *e);
expr_t *conditional_expr (expr_t *cond, expr_t *e1, expr_t *e2);
expr_t *incop_expr (int op, expr_t *e, int postop);
expr_t *array_expr (expr_t *array, expr_t *index);
expr_t *address_expr (expr_t *e1, expr_t *e2, struct type_s *t);
expr_t *assign_expr (expr_t *e1, expr_t *e2);
expr_t *cast_expr (struct type_s *t, expr_t *e);

void init_elements (struct def_s *def, expr_t *eles);

expr_t *error (expr_t *e, const char *fmt, ...) __attribute__((format(printf, 2,3)));
expr_t *warning (expr_t *e, const char *fmt, ...) __attribute__((format(printf, 2,3)));
expr_t * notice (expr_t *e, const char *fmt, ...) __attribute__((format(printf, 2,3)));

const char *get_op_string (int op);

extern int lineno_base;

struct keywordarg_s;
struct class_type_s;
expr_t *selector_expr (struct keywordarg_s *selector);
expr_t *protocol_expr (const char *protocol);
expr_t *encode_expr (struct type_s *type);
expr_t *super_expr (struct class_type_s *class_type);
expr_t *message_expr (expr_t *receiver, struct keywordarg_s *message);
expr_t *sizeof_expr (expr_t *expr, struct type_s *type);

expr_t *fold_constants (expr_t *e);

#endif//__expr_h
