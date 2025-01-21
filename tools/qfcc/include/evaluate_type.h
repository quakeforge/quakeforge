/*
	evaluate_type.h

	type evaluation

	Copyright (C) 2024 Bill Currie <bill@taniwha.org>

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

#ifndef __evaluate_type_h
#define __evaluate_type_h

/** \defgroup qfcc_evaluate_type Computed type expression evaluation.
	\ingroup qfcc_expr
*/
///@{

enum {
	tf_null,
	tf_eval,
	tf_property,
	tf_function,
	tf_field,
	tf_pointer,
	tf_reference,
	tf_array,
	tf_base,
	tf_width,
	tf_vector,
	tf_rows,
	tf_cols,
	tf_matrix,
	tf_int,
	tf_uint,
	tf_bool,
	tf_float,
	tf_gentype,

	tf_num_functions
};

typedef struct typeeval_s {
	struct typeeval_s *next;
	struct dstatement_s *code;
	struct pr_type_s *data;
	const char *strings;
	int code_size;
	int data_size;
	int string_size;
} typeeval_t;

void setup_type_progs (void);

typedef struct expr_s expr_t;
typedef struct gentype_s gentype_t;
typedef struct rua_ctx_s rua_ctx_t;

typeeval_t *build_type_function (const expr_t *te, int num_types,
								 gentype_t *types, rua_ctx_t *rua_ctx);
const type_t *evaluate_type (const typeeval_t *typeeval, int num_types,
							 const type_t **types, const expr_t *expr,
							 rua_ctx_t *rua_ctx);
///@}

#endif//__evaluate_type_h
