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

#include "QF/dstring.h"

#include "codespace.h"
#include "expr.h"
#include "function.h"
#include "qfcc.h"
#include "reloc.h"
#include "symtab.h"
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
	struct function_s *function;
	struct switch_block_s *switch_block;
	struct param_s	*param;
	struct struct_s *strct;
	struct symtab_s *symtab;
	struct symbol_s *symbol;
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

%left	<op>		RELOP
%left	<op>		ADDOP
%left	<op>		MULOP
%right	UNARY

%token	<type>		TYPE TYPE_NAME
%token	<symbol>	ID
%token	<expr>		CONST

%token	PROGRAM VAR ARRAY OF FUNCTION PROCEDURE PBEGIN END IF THEN ELSE
%token	WHILE DO RANGE ASSIGNOP NOT ELLIPSIS

%type	<type>		type standard_type
%type	<symbol>	program_head identifier_list subprogram_head
%type	<param>		arguments parameter_list
%type	<expr>		compound_statement optional_statements statement_list
%type	<expr>		statement procedure_statement
%type	<expr>		expression_list expression unary_expr primary variable name
%type	<op>		sign

%{
symtab_t       *current_symtab;
function_t *current_func;
struct class_type_s *current_class;
expr_t  *local_expr;
struct scope_s *current_scope;
param_t *current_params;
%}

%%

program
	: program_head
	  declarations
	  subprogram_declarations
	  compound_statement '.'
	  	{
			dstring_t  *str = dstring_newstr ();
			symbol_t   *s;

			// move the symbol for the program name to the end of the list
			symtab_removesymbol (current_symtab, $1);
			symtab_addsymbol (current_symtab, $1);

			for  (s = current_symtab->symbols; s; s = s->next) {
				dstring_clearstr (str);
				print_type_str (str, s->type);
				printf ("%s %s\n", s->name, str->str);
			}
			dstring_delete (str);
		}
	;

program_head
	: PROGRAM ID '(' opt_identifier_list ')' ';'
		{

			$$ = $2;
			current_symtab = new_symtab (0, stab_global);
			$$->type = parse_params (&type_void, 0);
			symtab_addsymbol (current_symtab, $2);
		}
	;

opt_identifier_list
	: /* empty */
	| identifier_list
	;

identifier_list
	: ID
	| identifier_list ',' ID
		{
			symbol_t  **s;
			$$ = $1;
			for (s = &$$; *s; s = &(*s)->next)
				;
			*s = $3;
		}
	;

declarations
	: declarations VAR identifier_list ':' type ';'
		{
			while ($3) {
				symbol_t   *next = $3->next;
				if ($3->table == current_symtab) {
					error (0, "%s redefined", $3->name);
				} else {
					if ($3->table)
						$3 = new_symbol ($3->name);
					$3->type = $5;
					symtab_addsymbol (current_symtab, $3);
				}
				$3 = next;
			}
		}
	| /* empty */
	;

type
	: standard_type
	| ARRAY '[' CONST RANGE CONST ']' OF standard_type
		{
			$$ = based_array_type ($8, $3->e.integer_val, $5->e.integer_val);
		}
	;

standard_type
	: TYPE
	| TYPE_NAME
	;

subprogram_declarations
	: subprogram_declarations subprogram_declaration
	| /* emtpy */
	;

subprogram_declaration
	: subprogram_head ';'
		{
			param_t    *p;

			$<symtab>$ = current_symtab;
			current_symtab = new_symtab (current_symtab, stab_local);

			for (p = $1->params; p; p = p->next)
				symtab_addsymbol (current_symtab, copy_symbol (p->symbol));
		}
	  declarations compound_statement ';'
		{
			dstring_t  *str = dstring_newstr ();
			symbol_t   *s;

			for  (s = current_symtab->symbols; s; s = s->next) {
				dstring_clearstr (str);
				print_type_str (str, s->type);
				printf ("    %s %s\n", s->name, str->str);
			}
			current_symtab = current_symtab->parent;
			dstring_delete (str);
		}
	| subprogram_head ASSIGNOP '#' CONST ';'
	;

subprogram_head
	: FUNCTION ID arguments ':' standard_type
		{
			$$ = $2;
			if ($$->table == current_symtab) {
				error (0, "%s redefined", $$->name);
			} else {
				$$->params = $3;
				$$->type = parse_params ($5, $3);
				symtab_addsymbol (current_symtab, $$);
			}
		}
	| PROCEDURE ID arguments
		{
			$$ = $2;
			if ($$->table == current_symtab) {
				error (0, "%s redefined", $$->name);
			} else {
				$$->params = $3;
				$$->type = parse_params (&type_void, $3);
				symtab_addsymbol (current_symtab, $$);
			}
		}
	;

arguments
	: '(' parameter_list ')'				{ $$ = $2; }
	| '(' parameter_list ';' ELLIPSIS ')'
		{
			$$ = param_append_identifiers ($2, 0, 0);
		}
	| '(' ELLIPSIS ')'						{ $$ = new_param (0, 0, 0); }
	| /* emtpy */							{ $$ = 0; }
	;

parameter_list
	: identifier_list ':' type
		{
			$$ = param_append_identifiers (0, $1, $3);
		}
	| parameter_list ';' identifier_list ':' type
		{
			$$ = param_append_identifiers ($1, $3, $5);
		}
	;

compound_statement
	: PBEGIN optional_statements END		{ $$ = $2; }
	;

optional_statements
	: statement_list opt_semi
	| /* emtpy */							{ $$ = 0; }
	;

opt_semi
	:	';'
	| /* empty */
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
			$$ = $1;
			if ($$->type == ex_symbol && extract_type ($$) == ev_func)
				$$ = new_ret_expr ($$->e.symbol->type->t.func.type);
			$$ = assign_expr ($$, $3);
		}
	| procedure_statement
	| compound_statement
	| IF expression THEN statement ELSE statement
		{
			$$ = build_if_statement ($2, $4, $6);
		}
	| IF expression THEN statement %prec IFX
		{
			$$ = build_if_statement ($2, $4, 0);
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
	: name
	| name '[' expression ']'				{ $$ = array_expr ($1, $3); }
	;

procedure_statement
	: name									{ $$ = function_expr ($1, 0); }
	| name '(' expression_list ')'			{ $$ = function_expr ($1, $3); }
	;

expression_list
	: expression
	| expression_list ',' expression
		{
			$$ = $3;
			$$->next = $1;
		}
	;

unary_expr
	: primary
	| sign unary_expr	%prec UNARY			{ $$ = unary_expr ($1, $2); }
	| NOT expression	%prec UNARY			{ $$ = unary_expr ('!', $2); }
	;

primary
	: variable
		{
			$$ = $1;
			if ($$->type == ex_symbol && extract_type ($$) == ev_func)
				$$ = function_expr ($$, 0);
		}
	| CONST
	| name '(' expression_list ')'			{ $$ = function_expr ($1, $3); }
	| '(' expression ')'					{ $$ = $2; }
	;

expression
	: unary_expr
	| expression RELOP expression			{ $$ = binary_expr ($2, $1, $3); }
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
			if ($$ == 'o')
				PARSE_ERROR;
		}
	;

name
	: ID									{ $$ = new_symbol_expr ($1); }
	;

%%
