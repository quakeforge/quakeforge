/*
	cexpr-parse.y

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
%define api.prefix {cexpr_yy}
%define api.pure full
%define api.push-pull push
%define parse.trace
%parse-param {void *scanner} {exprctx_t *context}

%{
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdio.h>

#include "QF/cmem.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/plist.h"
#include "QF/sys.h"

#include "QF/cexpr.h"

static exprval_t *assign_expr (exprval_t *dst, const exprval_t *src,
							   exprctx_t *context);
static exprval_t *binary_expr (int op, const exprval_t *a, const exprval_t *b,
							   exprctx_t *context);
static exprval_t *field_expr (const exprval_t *a, const exprval_t *b,
							  exprctx_t *context);
static exprval_t *index_expr (const exprval_t *a, const exprval_t *b,
							  exprctx_t *context);
static exprval_t *unary_expr (int op, const exprval_t *val,
							  exprctx_t *context);
static exprval_t *vector_expr (exprlist_t *list, exprctx_t *context);
static exprval_t *function_expr (exprsym_t *fsym, exprlist_t *list,
								 exprctx_t *context);
static exprlist_t *expr_item (exprval_t *val, exprctx_t *context);

static void
yyerror (void *scanner, exprctx_t *context, const char *s)
{
	cexpr_error (context, "%s before %s", s, cexpr_yyget_text (scanner));
}

%}

%left			COMMA
%right	<op>	'=' ASX
%right			'?' ':'
%left			OR
%left			AND
%left			'|'
%left			'^'
%left			'&'
%left			EQ NE
%left			LE GE LT GT

%left			SHL SHR
%left			'+' '-'
%left			'*' '/' '%' MOD
%right	<op>	SIZEOF UNARY INCOP
%left			HYPERUNARY
%left			'.' '(' '['

%token <symbol> NAME
%token <value>	VALUE

%type <value>	expr field uexpr
%type <list>	opt_arg_list arg_list arg_expr

%union {
	int        op;
	exprsym_t *symbol;
	exprval_t *value;
	exprlist_t *list;
	const char *string;
}

%%

start
	: expr				{ assign_expr (context->result, $1, context); }
	;

uexpr
	: NAME
		{
			if ($1) {
				$$ = (exprval_t *) cmemalloc (context->memsuper, sizeof (*$$));
				$$->type = $1->type;
				$$->value = $1->value;
			} else {
				cexpr_error (context, "undefined identifier %s",
							 cexpr_yyget_text (scanner));
			}
		}
	| VALUE
	| '[' arg_list ']'	{ $$ = vector_expr ($2, context); }
	| '(' expr ')'		{ $$ = $2; }
	| NAME '(' opt_arg_list ')'	{ $$ = function_expr ($1, $3, context); }
	| uexpr '.' field	{ $$ = field_expr ($1, $3, context); }
	| uexpr '[' expr ']'	{ $$ = index_expr ($1, $3, context); }
	| '+' uexpr %prec UNARY	{ $$ = $2; }
	| '-' uexpr %prec UNARY	{ $$ = unary_expr ('-', $2, context); }
	| '!' uexpr %prec UNARY	{ $$ = unary_expr ('!', $2, context); }
	| '~' uexpr %prec UNARY	{ $$ = unary_expr ('~', $2, context); }
	;

expr
	: uexpr
	| expr '=' expr		{ $$ = assign_expr ($1, $3, context); }
	| expr SHL expr		{ $$ = binary_expr (SHL, $1, $3, context); }
	| expr SHR expr		{ $$ = binary_expr (SHR, $1, $3, context); }
	| expr '+' expr		{ $$ = binary_expr ('+', $1, $3, context); }
	| expr '-' expr		{ $$ = binary_expr ('-', $1, $3, context); }
	| expr '*' expr		{ $$ = binary_expr ('*', $1, $3, context); }
	| expr '/' expr		{ $$ = binary_expr ('/', $1, $3, context); }
	| expr '&' expr		{ $$ = binary_expr ('&', $1, $3, context); }
	| expr '|' expr		{ $$ = binary_expr ('|', $1, $3, context); }
	| expr '^' expr		{ $$ = binary_expr ('^', $1, $3, context); }
	| expr '%' expr		{ $$ = binary_expr ('%', $1, $3, context); }
	| expr MOD expr		{ $$ = binary_expr (MOD, $1, $3, context); }
	;

field
	: NAME
		{
			exprctx_t  *ctx = context;
			const char *name = cexpr_yyget_text (scanner);
			size_t      size = strlen (name) + 1;
			//FIXME reuse strings
			$$ = (exprval_t *) cmemalloc (ctx->memsuper, sizeof (exprval_t));
			$$->type = &cexpr_field;
			$$->value = cmemalloc (ctx->memsuper, size);
			memcpy ($$->value, name, size);
		}
	;

opt_arg_list
	:					{ $$ = 0; }
	| arg_list          { $$ = $1; }
	;

arg_list
	: arg_expr
	| arg_list ',' arg_expr
		{
			$3-> next = $1;
			$$ = $3;
		}

arg_expr
	: expr				{ $$ = expr_item ($1, context); }
	;

%%

static exprval_t *
assign_expr (exprval_t *dst, const exprval_t *src, exprctx_t *context)
{
	binop_t    *binop;
	if (!dst || !src) {
		return 0;
	}
	if (dst->type == &cexpr_exprval) {
		*(exprval_t **) dst->value = (exprval_t *) src;
		return dst;
	}
	binop = cexpr_find_cast (dst->type, src->type);
	if (binop && binop->op) {
		binop->func (dst, src, dst, context);
	} else {
		if (dst->type != src->type) {
			cexpr_error (context,
						 "type mismatch in expression result: %s = %s",
						 dst->type->name, src->type->name);
			return dst;
		}
		memcpy (dst->value, src->value, dst->type->size);
	}
	return dst;
}

static exprval_t *
binary_expr (int op, const exprval_t *a, const exprval_t *b,
			 exprctx_t *context)
{
	binop_t    *binop;

	for (binop = a->type->binops; binop->op; binop++) {
		exprtype_t *otype = binop->other;
		if (!otype) {
			otype = a->type;
		}
		if (binop->op == op && otype == b->type) {
			break;
		}
	}
	exprtype_t *rtype = binop->result;
	if (!rtype) {
		rtype = a->type;
	}
	exprval_t  *result = cexpr_value (rtype, context);
	if (!binop->op) {
		cexpr_error (context, "invalid binary expression: %s %c %s",
					 a->type->name, op, b->type->name);
		memset (result->value, 0, rtype->size);
	} else {
		binop->func (a, b, result, context);
	}
	return result;
}

static exprval_t *
field_expr (const exprval_t *a, const exprval_t *b, exprctx_t *context)
{
	binop_t    *binop;
	exprval_t  *result = 0;

	if (!a) {
		return 0;
	}

	for (binop = a->type->binops; binop->op; binop++) {
		if (binop->op == '.' && binop->other == b->type) {
			break;
		}
	}
	if (!binop->op) {
		cexpr_error (context, "invalid binary expression: %s.%s",
					 a->type->name, b->type->name);
		result = cexpr_value (&cexpr_int, context);
		*(int *) result->value = 0;
	} else {
		exprval_t   c = { 0, &result };
		binop->func (a, b, &c, context);
	}
	return result;
}

static exprval_t *
index_expr (const exprval_t *a, const exprval_t *b, exprctx_t *context)
{
	binop_t    *binop;
	exprval_t  *result = 0;

	if (!a || !b) {
		return 0;
	}
	for (binop = a->type->binops; binop->op; binop++) {
		if (binop->op == '[' && binop->other == b->type) {
			break;
		}
	}
	if (!binop->op) {
		cexpr_error (context, "invalid index expression: %s.%s",
					 a->type->name, b->type->name);
		result = cexpr_value (&cexpr_int, context);
		*(int *) result->value = 0;
	} else {
		exprval_t   c = { 0, &result };
		binop->func (a, b, &c, context);
	}
	return result;
}

static exprval_t *
unary_expr (int op, const exprval_t *val, exprctx_t *context)
{
	unop_t     *unop;

	for (unop = val->type->unops; unop->op; unop++) {
		if (unop->op == op) {
			break;
		}
	}
	exprtype_t *rtype = unop->result;
	if (!rtype) {
		rtype = val->type;
	}
	exprval_t  *result = cexpr_value (rtype, context);
	if (!unop->op) {
		cexpr_error (context, "invalid unary expression: %c %s",
					 op, val->type->name);
	} else {
		unop->func (val, result, context);
	}
	return result;
}

exprval_t *
vector_expr (exprlist_t *list, exprctx_t *context)
{
	exprlist_t *l;
	exprval_t  *val = cexpr_value (&cexpr_vector, context);
	float      *vector = val->value;
	int         i;
	exprlist_t *rlist = 0;

	// list is built in reverse order, so need to reverse it to make converting
	// to an array easier
	while (list) {
		exprlist_t *t = list->next;
		list->next = rlist;
		rlist = list;
		list = t;
	}
	list = rlist;

	for (i = 0; i < 4 && list; i++, list = l) {
		exprval_t   dst = { &cexpr_float, &vector[i] };
		exprval_t  *src = list->value;
		binop_t    *cast = cexpr_find_cast (&cexpr_float, src->type);
		if (cast) {
			cast->func (&dst, src, &dst, context);
		} else {
			cexpr_error (context, "invalid vector expression type: [%d] %s",
						 i, val->type->name);
		}
		l = list->next;
		cmemfree (context->memsuper, list);
	}
	if (i == 4 && list) {
		cexpr_error (context, "excess elements in vector expression");
	}
	for ( ; i < 4; i++) {
		vector[i] = 0;
	}
	return val;
}

static exprval_t *function_expr (exprsym_t *fsym, exprlist_t *list,
								 exprctx_t *context)
{
	exprlist_t *l;
	int         num_args = 0;
	exprfunc_t *func = 0;
	exprval_t  *result;

	for (l = list; l; l = l->next) {
		num_args++;
	}
	__auto_type args = (const exprval_t **) alloca (num_args * sizeof (exprval_t *));
	__auto_type types = (exprtype_t **) alloca (num_args * sizeof (exprtype_t *));
	for (num_args = 0; list; list = l, num_args++) {
		args[num_args] = list->value;
		types[num_args] = list->value->type;
		l = list->next;
		cmemfree (context->memsuper, list);
	}
	if (fsym->type != &cexpr_function) {
		cexpr_error (context, "invalid function %s", fsym->name);
		result = cexpr_value (&cexpr_int, context);
		*(int *) result->value = 0;
		return result;
	}
	for (exprfunc_t *f = fsym->value; f->result; f++) {
		if (f->num_params == num_args
			&& memcmp (f->param_types, types,
					   num_args * sizeof (exprtype_t *)) == 0) {
			func = f;
			break;
		}
	}
	if (!func) {
		dstring_t  *argstr = dstring_newstr();
		for (int i = 0; i < num_args; i++) {
			dasprintf (argstr, "%s%s", types[i]->name,
					   i + 1 < num_args ? ", ": "");
		}
		cexpr_error (context, "no overload for %s(%s)", fsym->name,
					 argstr->str);
		dstring_delete (argstr);
		result = cexpr_value (&cexpr_int, context);
		*(int *) result->value = 0;
		return result;
	}
	result = cexpr_value (func->result, context);
	func->func (args, result, context);
	return result;
}

static exprlist_t *
expr_item (exprval_t *val, exprctx_t *context)
{
	__auto_type item = (exprlist_t *) cmemalloc (context->memsuper,
												 sizeof (exprlist_t));
	item->next = 0;
	item->value = val;
	return item;
}
