/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

typedef enum {
	ex_error,
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
	int			abs;
} ex_pointer_t;

typedef struct expr_s {
	struct expr_s *next;
	expr_type	type;
	int			line;
	string_t	file;
	unsigned	paren:1;
	unsigned	rvalue:1;
	union {
		ex_label_t label;
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
		int		field_val;
		int		func_val;
		ex_pointer_t	pointer;
		float	quaternion_val[4];
		int		integer_val;
		unsigned int	uinteger_val;
		short	short_val;
	} e;
} expr_t;

extern etype_t qc_types[];
extern struct type_s *types[];
extern expr_type expr_types[];

extern expr_t *local_expr;

struct type_s *get_type (expr_t *e);
etype_t extract_type (expr_t *e);

expr_t *new_expr (void);
const char *new_label_name (void);
expr_t *new_label_expr (void);
expr_t *new_block_expr (void);
expr_t *new_binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *new_unary_expr (int op, expr_t *e1);
expr_t *new_temp_def_expr (struct type_s *type);
expr_t *new_bind_expr (expr_t *e1, expr_t *e2);
expr_t *new_name_expr (const char *name);
expr_t *new_def_expr (struct def_s *def);
expr_t *new_self_expr (void);
expr_t *new_this_expr (void);

void inc_users (expr_t *e);
void convert_name (expr_t *e);

expr_t *append_expr (expr_t *block, expr_t *e);

void print_expr (expr_t *e);

void convert_int (expr_t *e);

expr_t *test_expr (expr_t *e, int test);
expr_t *binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *asx_expr (int op, expr_t *e1, expr_t *e2);
expr_t *unary_expr (int op, expr_t *e);
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
void warning (expr_t *e, const char *fmt, ...) __attribute__((format(printf, 2,3)));
void notice (expr_t *e, const char *fmt, ...) __attribute__((format(printf, 2,3)));

const char *get_op_string (int op);

extern int lineno_base;

struct keywordarg_s;
expr_t *selector_expr (struct keywordarg_s *selector);
expr_t *protocol_expr (const char *protocol);
expr_t *encode_expr (struct type_s *type);
expr_t *message_expr (expr_t *receiver, struct keywordarg_s *message);

#endif//__expr_h
