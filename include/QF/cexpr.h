/*
	cexpr.h

	Config expression parser. Or concurrent.

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

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
#ifndef __QF_expr_h
#define __QF_expr_h

#include <stdlib.h>

struct exprval_s;
struct exprctx_s;

typedef struct binop_s {
	int         op;
	struct exprtype_s *other;
	struct exprtype_s *result;
	void      (*func) (const struct exprval_s *val1,
					   const struct exprval_s *val2,
					   struct exprval_s *result,
					   struct exprctx_s *context);
} binop_t;

typedef struct unop_s {
	int         op;
	struct exprtype_s *result;
	void      (*func) (const struct exprval_s *val, struct exprval_s *result,
					   struct exprctx_s *context);
} unop_t;

typedef struct exprtype_s {
	const char *name;
	size_t      size;
	binop_t    *binops;
	unop_t     *unops;
	void       *data;
} __attribute__((designated_init)) exprtype_t;

typedef struct exprval_s {
	exprtype_t *type;
	void       *value;
} exprval_t;

typedef struct exprlist_s {
	struct exprlist_s *next;
	exprval_t  *value;
} exprlist_t;

typedef struct exprsym_s {
	const char *name;
	exprtype_t *type;
	void       *value;
	struct exprsym_s *next;
} exprsym_t;

typedef struct exprfunc_s {
	exprtype_t *result;
	int         num_params;
	exprtype_t **param_types;
	void      (*func) (const exprval_t **params, exprval_t *result,
					   struct exprctx_s *context);
} exprfunc_t;

typedef struct exprtab_s {
	exprsym_t  *symbols;
	struct hashtab_s *tab;
} exprtab_t;

typedef struct exprarray_s {
	exprtype_t *type;
	unsigned    size;
} exprarray_t;

typedef struct exprctx_s {
	struct exprctx_s *parent;		// for nested symol scopes
	exprval_t  *result;
	exprtab_t  *symtab;				// directly accessible symbols
	exprtab_t  *external_variables;	// accessible via $id
	struct memsuper_s *memsuper;
	const struct plitem_s *item;
	struct plitem_s *messages;
	struct hashlink_s **hashlinks;
	int         errors;
} exprctx_t;

typedef struct exprenum_s {
	exprtype_t *type;
	exprtab_t  *symtab;
} exprenum_t;

int cexpr_parse_enum (exprenum_t *enm, const char *str,
					  const exprctx_t *context, void *data);
binop_t *cexpr_find_cast (exprtype_t *dst_type, exprtype_t *src_type) __attribute__((pure));
exprval_t *cexpr_value (exprtype_t *type, exprctx_t *ctx);
exprval_t *cexpr_value_reference (exprtype_t *type, void *data, exprctx_t *ctx);
int cexpr_eval_string (const char *str, exprctx_t *context);
void cexpr_error(exprctx_t *ctx, const char *fmt, ...) __attribute__((format(PRINTF,2,3)));

void cexpr_array_getelement (const exprval_t *a, const exprval_t *b,
							 exprval_t *c, exprctx_t *ctx);
void cexpr_struct_getfield (const exprval_t *a, const exprval_t *b,
							exprval_t *c, exprctx_t *ctx);
void cexpr_struct_pointer_getfield (const exprval_t *a, const exprval_t *b,
									exprval_t *c, exprctx_t *ctx);
exprval_t *cexpr_cvar (const char *name, exprctx_t *ctx);
exprval_t *cexpr_cvar_struct (exprctx_t *ctx);

void cexpr_cast_plitem (const exprval_t *val1, const exprval_t *src,
						exprval_t *result, exprctx_t *ctx);

void cexpr_init_symtab (exprtab_t *symtab, exprctx_t *ctx);

char *cexpr_yyget_text (void *scanner);

extern exprtype_t cexpr_int;
extern exprtype_t cexpr_uint;
extern exprtype_t cexpr_size_t;
extern exprtype_t cexpr_float;
extern exprtype_t cexpr_double;
extern exprtype_t cexpr_vector;
extern exprtype_t cexpr_quaternion;
extern exprtype_t cexpr_exprval;
extern exprtype_t cexpr_field;
extern exprtype_t cexpr_function;
extern exprtype_t cexpr_plitem;

extern binop_t cexpr_array_binops[];
extern binop_t cexpr_struct_binops[];
extern binop_t cexpr_struct_pointer_binops[];

extern exprsym_t cexpr_lib_symbols[];

#endif//__QF_expr_h
