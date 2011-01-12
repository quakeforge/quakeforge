%{
/*
	qc-parse.y

	parser for quakec

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/06/12

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "codespace.h"
#include "expr.h"
#include "function.h"
#include "qfcc.h"
#include "reloc.h"
#include "type.h"

#define YYDEBUG 1
#define YYERROR_VERBOSE 1
#undef YYERROR_VERBOSE

extern char *yytext;

static void
yyerror (const char *s)
{
#ifdef YYERROR_VERBOSE
	error (0, "%s %s\n", yytext, s);
#else
	error (0, "%s before %s", s, yytext);
#endif
}

static void
parse_error (void)
{
	error (0, "parse error before %s", yytext);
}

#define PARSE_ERROR do { parse_error (); YYERROR; } while (0)

int yylex (void);

%}

%union {
	int			op;
	struct def_s *def;
	struct hashtab_s *def_list;
	struct type_s	*type;
	struct typedef_s *typename;
	struct expr_s	*expr;
	int			integer_val;
	unsigned	uinteger_val;
	float		float_val;
	const char *string_val;
	float		vector_val[3];
	float		quaternion_val[4];
	struct function_s *function;
	struct switch_block_s *switch_block;
	struct param_s	*param;
	struct method_s	*method;
	struct class_s	*class;
	struct category_s *category;
	struct class_type_s	*class_type;
	struct protocol_s *protocol;
	struct protocollist_s *protocol_list;
	struct keywordarg_s *keywordarg;
	struct methodlist_s *methodlist;
	struct struct_s *strct;
}

// these tokens are common with qc
%nonassoc IFX
%nonassoc ELSE
%nonassoc BREAK_PRIMARY
%nonassoc ';'
%nonassoc CLASS_NOT_CATEGORY
%nonassoc STORAGEX

%right  <op> '=' ASX PAS /* pointer assign */
%right  '?' ':'
%left   OR
%left   AND
%left   '|'
%left   '^'
%left   '&'
%left   EQ NE
%left   LE GE LT GT
// end of tokens common with qc

%left	<string_val>	RELOP
%left	<op>			ADDOP
%left	<op>			MULOP
%right	UNARY

%token	<type>				TYPE
%token	<string_val>		ID
%token	<integer_val>		INT_VAL
%token	<string_val>		STRING_VAL
%token	<quaternion_val>	QUATERNION_VAL
%token	<vector_val>		VECTOR_VAL
%token	<float_val>			FLOAT_VAL

%token	PROGRAM VAR ARRAY OF FUNCTION PROCEDURE PBEGIN END IF THEN ELSE
%token	WHILE DO RANGE ASSIGNOP NOT ELLIPSIS

%type	<type>	standard_type type
%type	<expr>	const num identifier_list statement_list statement
%type	<expr>	optional_statements compound_statement procedure_statement
%type	<expr>	expression_list expression unary_expr primary variable
%type	<param>	parameter_list arguments
%type	<def>	subprogram_head program_head
%type	<function> subprogram_declaration
%type	<op>	sign
%type	<expr>	name

%{
function_t *current_func;
struct class_type_s *current_class;
expr_t  *local_expr;
struct scope_s *current_scope;
param_t *current_params;

static int convert_relop (const char *relop);
%}

%%

program
	: program_head
	  declarations
	  subprogram_declarations
		{
			$1 = get_function_def ($1->name, $1->type, current_scope,
								   st_global, 0, 1);
			current_func = begin_function ($1, 0, 0);
		}
	  compound_statement '.'
	  	{
			def_t      *main_def;
			function_t *main_func;
			expr_t     *main_expr;

			current_scope = current_scope->parent;
			//current_storage = st_global;
			build_code_function (current_func, 0, $5);
			current_func = 0;

			main_def = get_def (&type_function, ".main", pr.scope, st_static);
			current_func = main_func = new_function (main_def, 0);
			add_function (main_func);
			reloc_def_func (main_func, main_def->ofs);
			main_func->code = pr.code->size;
			build_scope (main_func, main_def, 0);
			build_function (main_func);
			main_expr = new_block_expr ();
			append_expr (main_expr,
					build_function_call (new_def_expr ($1), $1->type, 0));
			emit_function (main_func, main_expr);
			finish_function (main_func);
			current_func = 0;
		}
	;

program_head
	: PROGRAM ID '(' identifier_list ')' ';'
		{
			type_t     *type = parse_params (&type_void, 0);
			current_params = 0;
			$$ = get_function_def ($2, type, current_scope, st_extern, 0, 1);
		}
	;

identifier_list
	: ID
		{
			$$ = new_block_expr ();
			append_expr ($$, new_name_expr ($1));
		}
	| identifier_list ',' ID
		{
			append_expr ($1, new_name_expr ($3));
		}
	;

declarations
	: declarations VAR identifier_list ':' type ';'
		{
			type_t     *type = $5;
			expr_t     *id_list = $3;
			expr_t     *e;

			for (e = id_list->e.block.head; e; e = e->next)
				get_def (type, e->e.string_val, current_scope, st_global);
		}
	| /* empty */
	;

type
	: standard_type				{ $$ = $1; }
	| ARRAY '[' num RANGE num ']' OF standard_type
		{
			if ($3->type != ex_integer || $5->type != ex_integer)
				error (0, "array bounds must be integers");
			$$ = based_array_type ($8, $3->e.integer_val, $5->e.integer_val);
		}
	;

standard_type
	: TYPE						{ $$ = $1; }
	;

subprogram_declarations
	: subprogram_declarations subprogram_declaration
	| /* emtpy */
	;

subprogram_declaration
	: subprogram_head ';'
		{
			current_func = begin_function ($1, 0, current_params);
			current_scope = current_func->scope;
			//current_storage = st_local;
		}
	  declarations compound_statement ';'
		{
			type_t     *ret_type = $1->type->t.func.type;
			current_scope = current_scope->parent;
			//current_storage = st_global;
			//FIXME want a true void return
			if (ret_type)
				append_expr ($5, return_expr (current_func,
											  new_ret_expr (ret_type)));
			build_code_function (current_func, 0, $5);
			current_func = 0;
		}
	| subprogram_head ASSIGNOP '#' const ';'
		{
			$$ = build_builtin_function ($1, $4);
			if ($$) {
				build_scope ($$, $$->def, current_params);
				flush_scope ($$->scope, 1);
			}
		}
	;

subprogram_head
	: FUNCTION ID arguments ':' standard_type
		{
			type_t     *type = parse_params ($5, $3);
			current_params = $3;
			$$ = get_function_def ($2, type, current_scope, st_global, 0, 1);
		}
	| PROCEDURE ID arguments
		{
			type_t     *type = parse_params (&type_void, $3);
			$$ = get_function_def ($2, type, current_scope, st_global, 0, 1);
		}
	;

arguments
	: '(' parameter_list ')'	{ $$ = $2; }
	| '(' parameter_list ';' ELLIPSIS ')'
		{
			param_t   **p;

			$$ = $2;
			p = &$$;
			for ( ; *p; p = &(*p)->next)
				;
			*p = new_param (0, 0, 0);
		}
	| '(' ELLIPSIS ')'			{ $$ = new_param (0, 0, 0); }
	| /* emtpy */				{ $$ = 0; }
	;

parameter_list
	: identifier_list ':' type
		{
			type_t     *type = $3;
			expr_t     *id_list = $1;
			expr_t     *e;
			param_t   **p;

			$$ = 0;
			p = &$$;
			for (e = id_list->e.block.head; e; e = e->next) {
				*p = new_param (0, type, e->e.string_val);
				p = &(*p)->next;
			}
		}
	| parameter_list ';' identifier_list ':' type
		{
			type_t     *type = $5;
			expr_t     *id_list = $3;
			expr_t     *e;
			param_t   **p;

			$$ = $1;
			p = &$$;
			for ( ; *p; p = &(*p)->next)
				;
			for (e = id_list->e.block.head; e; e = e->next) {
				*p = new_param (0, type, e->e.string_val);
				p = &(*p)->next;
			}
		}
	;

compound_statement
	: PBEGIN optional_statements END	{ $$ = $2; }
	;

optional_statements
	: statement_list					{ $$ = $1; }
	| /* emtpy */						{ $$ = 0; }
	;

statement_list
	: statement
		{
			$$ = new_block_expr ();
			append_expr ($$, $1);
		}
	| statement_list ';' statement
		{
			$$ = $1;
			append_expr ($$, $3);
		}
	;

statement
	: variable ASSIGNOP expression
		{
			convert_name ($1);
			if ($1->type == ex_def && extract_type ($1) == ev_func)
				$1 = new_ret_expr ($1->e.def->type->t.func.type);
			$$ = assign_expr ($1, $3);
		}
	| procedure_statement							{ $$ = $1; }
	| compound_statement							{ $$ = $1; }
	| IF expression THEN statement ELSE statement
		{
			int         line = pr.source_line;
			string_t    file = pr.source_file;
			expr_t     *tl = new_label_expr ();
			expr_t     *fl = new_label_expr ();
			expr_t     *nl = new_label_expr ();

			pr.source_line = $2->line;
			pr.source_file = $2->file;

			$$ = new_block_expr ();

			$2 = convert_bool ($2, 1);
			if ($2->type != ex_error) {
				backpatch ($2->e.bool.true_list, tl);
				backpatch ($2->e.bool.false_list, fl);
				append_expr ($2->e.bool.e, tl);
				append_expr ($$, $2);
			}
			append_expr ($$, $4);
			append_expr ($$, new_unary_expr ('g', nl));

			append_expr ($$, fl);
			append_expr ($$, $6);
			append_expr ($$, nl);

			pr.source_line = line;
			pr.source_file = file;
		}
	| IF expression THEN statement %prec IFX
		{
			int         line = pr.source_line;
			string_t    file = pr.source_file;
			expr_t     *tl = new_label_expr ();
			expr_t     *fl = new_label_expr ();

			pr.source_line = $2->line;
			pr.source_file = $2->file;

			$$ = new_block_expr ();

			$2 = convert_bool ($2, 1);
			if ($2->type != ex_error) {
				backpatch ($2->e.bool.true_list, tl);
				backpatch ($2->e.bool.false_list, fl);
				append_expr ($2->e.bool.e, tl);
				append_expr ($$, $2);
			}
			append_expr ($$, $4);
			append_expr ($$, fl);

			pr.source_line = line;
			pr.source_file = file;
		}
	| WHILE expression DO statement
		{
			int         line = pr.source_line;
			string_t    file = pr.source_file;
			expr_t     *l1 = new_label_expr ();
			expr_t     *l2 = new_label_expr ();
			expr_t     *cont = new_label_expr ();

			pr.source_line = $2->line;
			pr.source_file = $2->file;

			$$ = new_block_expr ();

			append_expr ($$, new_unary_expr ('g', cont));
			append_expr ($$, l1);
			append_expr ($$, $4);
			append_expr ($$, cont);

			$2 = convert_bool ($2, 1);
			if ($2->type != ex_error) {
				backpatch ($2->e.bool.true_list, l1);
				backpatch ($2->e.bool.false_list, l2);
				append_expr ($2->e.bool.e, l2);
				append_expr ($$, $2);
			}

			pr.source_line = line;
			pr.source_file = file;
		}
	;

variable
	: name							{ $$ = $1; }
	| name '[' expression ']'		{ $$ = array_expr ($1, $3); }
	;

procedure_statement
	: name							{ $$ = function_expr ($1, 0); }
	| name '(' expression_list ')'	{ $$ = function_expr ($1, $3); }
	;

expression_list
	: expression
	| expression_list ',' expression
		{
			$3->next = $1;
			$$ = $3;
		}
	;

unary_expr
	: primary
	| sign unary_expr	%prec UNARY
		{
			if ($1 == '-')
				$$ = unary_expr ('-', $2);
			else
				$$ = $2;
		}
	| NOT expression	%prec UNARY
		{
			$$ = unary_expr ('!', $2);
		}
	;

primary
	: variable
		{
			convert_name ($1);
			if ($1->type == ex_def && extract_type ($1) == ev_func)
				$1 = function_expr ($1, 0);
			$$ = $1;
		}
	| const							{ $$ = $1; }
	| name '(' expression_list ')'	{ $$ = function_expr ($1, $3); }
	| '(' expression ')'			{ $$ = $2; }
	;

expression
	: unary_expr
	| expression RELOP expression
		{
			int         op = convert_relop ($2);
			$$ = binary_expr (op, $1, $3);
		}
	| expression ADDOP expression
		{
			if ($2 == 'o')
				$$ = bool_expr (OR, new_label_expr (), $1, $3);
			else
				$$ = binary_expr ($2, $1, $3);
		}
	| expression MULOP expression
		{
			if ($2 == 'd')
				$2 = '/';
			else if ($2 == 'm')
				$2 = '%';
			if ($2 == 'a')
				$$ = bool_expr (AND, new_label_expr (), $1, $3);
			else
				$$ = binary_expr ($2, $1, $3);
		}
	;

sign
	: ADDOP
		{
			if ($1 == 'o')
				PARSE_ERROR;
		}
	;

name
	: ID						{ $$ = new_name_expr ($1); }
	;

const
	: num						{ $$ = $1; }
	| STRING_VAL				{ $$ = new_string_expr ($1); }
	;

num
	: INT_VAL					{ $$ = new_integer_expr ($1); }
	| FLOAT_VAL					{ $$ = new_float_expr ($1); }
	| VECTOR_VAL				{ $$ = new_vector_expr ($1); }
	| QUATERNION_VAL			{ $$ = new_quaternion_expr ($1); }
	;

%%

static int
convert_relop (const char *relop)
{
	switch (relop[0]) {
		case '=':
			return EQ;
		case '<':
			switch (relop[1]) {
				case 0:
					return LT;
				case '>':
					return NE;
				case '=':
					return LE;
			}
			break;
		case '>':
			switch (relop[1]) {
				case 0:
					return GT;
				case '=':
					return GE;
			}
			break;
	}
	error (0, "internal: bad relop %s", relop);
	return EQ;
}
