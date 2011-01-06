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

#include "class.h"
#include "expr.h"
#include "function.h"
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

%nonassoc IFX
%nonassoc ELSE

%left	<string_val>	RELOP
%left	<integer_val>	ADDOP
%left	<integer_val>	MULOP
%right	UNARY

%token	<type>				TYPE
%token	<string_val>		ID
%token	<integer_val>		INT_VAL
%token	<string_val>		STRING_VAL
%token	<quaternion_val>	QUATERNION_VAL
%token	<vector_val>		VECTOR_VAL
%token	<float_val>			FLOAT_VAL

%token	PROGRAM VAR ARRAY OF FUNCTION PROCEDURE PBEGIN END IF THEN ELSE
%token	WHILE DO RANGE ASSIGNOP NOT

%type	<type>	standard_type type
%type	<expr>	const num

%{
function_t *current_func;
class_type_t *current_class;
expr_t  *local_expr;
scope_t *current_scope;

%}

%%

program
	: program_header
	  declarations
	  subprogram_declarations
	  compound_statement
	  '.'
	;

program_header
	: PROGRAM ID '(' identifier_list ')' ';'
	;

identifier_list
	: ID
	| identifier_list ',' ID
	;

declarations
	: declarations VAR identifier_list ':' type ';'
	| /* empty */
	;

type
	: standard_type				{ $$ = $1; }
	| ARRAY '[' num RANGE num ']' OF standard_type
		{
			int top = $5->e.integer_val;
			int bot = $3->e.integer_val;
			$$ = array_type ($8, top - bot + 1);
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
	: subprogram_head ';' declarations compound_statement ';'
	| subprogram_head '=' '#' const ';'
	;

subprogram_head
	: FUNCTION ID arguments ':' standard_type
	| PROCEDURE ID arguments
	;

arguments
	: '(' parameter_list ')'
	| /* emtpy */
	;

parameter_list
	: identifier_list ':' type
	| parameter_list ';' identifier_list ':' type
	;

compound_statement
	: PBEGIN optional_statements END
	;

optional_statements
	: statement_list
	| /* emtpy */
	;

statement_list
	: statement
	| statement_list ';' statement
	;

statement
	: variable ASSIGNOP expression
	| procedure_statement
	| compound_statement
	| IF expression THEN statement ELSE statement
	| IF expression THEN statement %prec IFX
	| WHILE expression DO statement
	;

variable
	: ID
	| ID '[' expression ']'
	;

procedure_statement
	: ID
	| ID '(' expression_list ')'
	;

expression_list
	: expression
	| expression_list ',' expression
	;

unary_expr
	: primary
	| sign unary_expr	%prec UNARY
	| NOT expression	%prec UNARY
	;

primary
	: variable
	| const
	| ID '(' expression_list ')'
	| '(' expression ')'
	;

expression
	: unary_expr
	| expression RELOP expression
	| expression ADDOP expression
	| expression MULOP expression
	;

sign
	: ADDOP
		{
			if ($1 == 'o')
				PARSE_ERROR;
		}
	;

const
	: num
	| STRING_VAL				{ $$ = new_string_expr ($1); }
	;

num
	: INT_VAL					{ $$ = new_integer_expr ($1); }
	| FLOAT_VAL					{ $$ = new_float_expr ($1); }
	| VECTOR_VAL				{ $$ = new_vector_expr ($1); }
	| QUATERNION_VAL			{ $$ = new_quaternion_expr ($1); }
	;

%%
