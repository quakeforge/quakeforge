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
#include "QF/hash.h"
#include "QF/qfplist.h"
#include "QF/sys.h"

#include "QF/cexpr.h"

static void assign_expr (exprval_t *dst, const exprval_t *src,
						 exprctx_t *context);
static exprval_t *binary_expr (int op, const exprval_t *a, const exprval_t *b,
							   exprctx_t *context);
static exprval_t *field_expr (const exprval_t *a, const exprval_t *b,
							  exprctx_t *context);

static void
yyerror (void *scanner, exprctx_t *context, const char *s)
{
	cexpr_error (context, "%s before %s\n", s, cexpr_yyget_text (scanner));
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
%right			SIZEOF UNARY INCOP
%left			HYPERUNARY
%left			'.' '(' '['

%token <symbol> NAME
%token <value>	VALUE

%type <value>	expr field

%union {
	int        op;
	exprsym_t *symbol;
	exprval_t *value;
	const char *string;
}

%%

start
	: expr				{ assign_expr (context->result, $1, context); }
	;

expr
	: expr SHL expr		{ $$ = binary_expr (SHL, $1, $3, context); }
	| expr SHR expr		{ $$ = binary_expr (SHR, $1, $3, context); }
	| expr '+' expr		{ $$ = binary_expr ('+', $1, $3, context); }
	| expr '-' expr		{ $$ = binary_expr ('-', $1, $3, context); }
	| expr '*' expr		{ $$ = binary_expr ('*', $1, $3, context); }
	| expr '/' expr		{ $$ = binary_expr ('/', $1, $3, context); }
	| expr '&' expr		{ $$ = binary_expr ('&', $1, $3, context); }
	| expr '|' expr		{ $$ = binary_expr ('|', $1, $3, context); }
	| expr '^' expr		{ $$ = binary_expr ('^', $1, $3, context); }
	| expr '%' expr		{ $$ = binary_expr ('%', $1, $3, context); }
	| expr '.' field	{ $$ = field_expr ($1, $3, context); }
	| expr MOD expr		{ $$ = binary_expr (MOD, $1, $3, context); }
	| NAME
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

%%

static void
assign_expr (exprval_t *dst, const exprval_t *src, exprctx_t *context)
{
	binop_t    *binop;
	if (!src) {
		return;
	}
	binop = cexpr_find_cast (dst->type, src->type);
	if (binop && binop->op) {
		binop->func (dst, src, dst, context);
	} else {
		if (dst->type != src->type) {
			cexpr_error (context,
						 "type mismatch in expression result: %s = %s\n",
						 dst->type->name, src->type->name);
			return;
		}
		memcpy (dst->value, src->value, dst->type->size);
	}
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
		cexpr_error (context, "invalid binary expression: %s %c %s\n",
					 a->type->name, op, b->type->name);
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

	for (binop = a->type->binops; binop->op; binop++) {
		if (binop->op == '.' && binop->other == b->type) {
			break;
		}
	}
	if (!binop->op) {
		cexpr_error (context, "invalid binary expression: %s.%s\n",
					 a->type->name, b->type->name);
	} else {
		exprval_t   c = { 0, &result };
		binop->func (a, b, &c, context);
	}
	return result;
}
