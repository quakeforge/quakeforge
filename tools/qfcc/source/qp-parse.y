/*
	qp-parse.y

	parser for quakepascal

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/06

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
%define api.prefix {qp_yy}
%define api.pure full
%define api.push-pull push
%locations
%parse-param {void *scanner}

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

#include "QF/dstring.h"

#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

#define YYDEBUG 1
#define YYERROR_VERBOSE 1
#undef YYERROR_VERBOSE

#include "tools/qfcc/source/qp-parse.h"

#define qp_yytext qp_yyget_text (scanner)
char *qp_yyget_text (void *scanner);

static void
yyerror (YYLTYPE *yylloc, void *scanner, const char *s)
{
#ifdef YYERROR_VERBOSE
	error (0, "%s %s\n", qp_yytext, s);
#else
	error (0, "%s before %s", s, qp_yytext);
#endif
}

static void
parse_error (void *scanner)
{
	error (0, "parse error before %s", qp_yytext);
}

#define PARSE_ERROR do { parse_error (scanner); YYERROR; } while (0)

#define first_line line
#define first_column column

int yylex (void);

%}

%define api.location.type {struct rua_loc_s}

%union {
	int			op;
	struct def_s *def;
	struct hashtab_s *def_list;
	const struct type_s	*type;
	struct typedef_s *typename;
	const struct expr_s	*expr;
	struct expr_s	*mut_expr;
	struct function_s *function;
	struct switch_block_s *switch_block;
	struct param_s	*param;
	struct struct_s *strct;
	struct symtab_s *symtab;
	struct symbol_s *symbol;
	int              storage;
}

// these tokens are common between qc and qp
%left LOW
%nonassoc IFX
%nonassoc ELSE
%nonassoc BREAK_PRIMARY
%nonassoc ';'
%nonassoc CLASS_NOT_CATEGORY
%nonassoc STORAGEX

%left	COMMA
%right  <op> '=' ASX
%right  '?' ':'
%left   OR
%left   XOR
%left   AND
%left   '|'
%left   '^'
%left   '&'
%left   EQ NE
%left	LT GT GE LE
%token	NAND NOR XNOR
// end of tokens common between qc and qp

%left	<op>		RELOP
%left	<op>		ADDOP
%left	<op>		MULOP
%right	UNARY

%token	<type>		TYPE TYPE_NAME
%token	<symbol>	ID
%token	<expr>		VALUE

%token	PROGRAM VAR ARRAY OF FUNCTION PROCEDURE PBEGIN END IF THEN ELSE
%token	WHILE DO RANGE ASSIGNOP NOT ELLIPSIS
%token	RETURN

%type	<type>		type standard_type
%type	<symbol>	program_head identifier_list subprogram_head
%type	<symtab>	param_scope
%type	<param>		arguments parameter_list
%type	<mut_expr>	compound_statement optional_statements statement_list
%type	<expr>		else
%type	<expr>		statement procedure_statement
%type	<mut_expr>	expression_list
%type	<expr>		expression unary_expr primary variable name
%type	<op>		sign

%{

static void
build_dotmain (symbol_t *program)
{
	symbol_t   *dotmain = new_symbol (".main");
	expr_t     *code;
	expr_t     *exitcode;

	dotmain->params = 0;
	dotmain->type = parse_params (&type_int, 0);
	dotmain->type = find_type (dotmain->type);
	dotmain = function_symbol ((specifier_t) { .sym = dotmain });

	exitcode = new_symbol_expr (symtab_lookup (current_symtab, "ExitCode"));

	current_func = begin_function (dotmain, 0, current_symtab, 0,
								   current_storage);
	current_symtab = current_func->locals;
	code = new_block_expr (0);
	append_expr (code, function_expr (new_symbol_expr (program), 0));
	append_expr (code, return_expr (current_func, exitcode));
	build_code_function (dotmain, 0, code);
}

static symbol_t *
function_value (function_t *func)
{
	symbol_t   *ret = 0;
	if (func->type->func.ret_type) {
		ret = symtab_lookup (func->locals, ".ret");
		if (!ret || ret->table != func->locals) {
			ret = new_symbol_type (".ret", func->type->func.ret_type);
			initialize_def (ret, 0, func->locals->space, sc_local,
							func->locals);
		}
	}
	return ret;
}

static expr_t *
function_return (function_t *func)
{
	symbol_t   *ret = function_value (func);
	expr_t     *ret_val = 0;
	if (ret) {
		ret_val = new_symbol_expr (ret);
	}
	return ret_val;
}

%}

%%

program
	: program_head
	  declarations
	  subprogram_declarations
	  compound_statement '.'
	  	{
			symtab_t   *st = current_symtab;
			// move the symbol for the program name to the end of the list
			symtab_removesymbol (current_symtab, $1);
			symtab_addsymbol (current_symtab, $1);

			current_func = begin_function ($1, 0, current_symtab, 0,
										   current_storage);
			current_symtab = current_func->locals;
			build_code_function ($1, 0, $4);
			current_symtab = st;

			build_dotmain ($1);
			current_symtab = st;
		}
	;

program_head
	: PROGRAM								{ current_symtab = pr.symtab; }
	  ID '(' opt_identifier_list ')' ';'
		{
			$$ = $3;

			// FIXME need units and standard units
			{
				symbol_t   *sym = new_symbol ("ExitCode");
				sym->type = &type_int;
				initialize_def (sym, 0, current_symtab->space, sc_global,
								current_symtab);
				if (sym->def) {
					sym->def->nosave = 1;
				}
			}
			$$->type = parse_params (&type_void, 0);
			$$->type = find_type ($$->type);
			$$ = function_symbol ((specifier_t) { .sym = $$ });
		}
	;

opt_identifier_list
	: /* empty */
	| identifier_list
	;

identifier_list
	: ID									{ $$ = check_redefined ($1); }
	| identifier_list ',' ID
		{
			symbol_t  **s;
			$$ = $1;
			$3 = check_redefined ($3);
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
				$3->type = $5;
				initialize_def ($3, 0, current_symtab->space, current_storage,
								current_symtab);
				$3 = next;
			}
		}
	| /* empty */
	;

type
	: standard_type
	| ARRAY '[' VALUE RANGE VALUE ']' OF standard_type
		{
			$$ = based_array_type ($8, expr_int ($3), expr_int ($5));
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
			$<storage>$ = current_storage;
			current_func = begin_function ($1, 0, current_symtab, 0,
										   current_storage);
			current_symtab = current_func->locals;
			current_storage = sc_local;
			function_value (current_func);
		}
	  declarations compound_statement ';'
		{
			append_expr ($5, new_return_expr (function_return (current_func)));
			build_code_function ($1, 0, $5);
			current_symtab = current_func->parameters->parent;
			current_storage = $<storage>3;
		}
	| subprogram_head ASSIGNOP '#' VALUE ';'
		{
			build_builtin_function ($1, $4, 0, current_storage);
		}
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
				$$->type = find_type ($$->type);
				$$ = function_symbol ((specifier_t) { .sym = $$ });
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
				$$->type = find_type ($$->type);
				$$ = function_symbol ((specifier_t) { .sym = $$ });
			}
		}
	;

arguments
	: '(' param_scope parameter_list ')'
		{
			$$ = $3;
			current_symtab = $2;
		}
	| '(' param_scope parameter_list ';' ELLIPSIS ')'
		{
			$$ = param_append_identifiers ($3, 0, 0);
			current_symtab = $2;
		}
	| '(' ELLIPSIS ')'						{ $$ = new_param (0, 0, 0); }
	| /* emtpy */							{ $$ = 0; }
	;

param_scope
	: /* empty */
		{
			$$ = current_symtab;
			current_symtab = new_symtab (current_symtab, stab_local);
		}
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
			$$ = new_block_expr (0);
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
			if ($$->type == ex_symbol && $$->symbol->sy_type == sy_func) {
				if ($$->symbol->func != current_func) {
					$$ = error ($$, "cannot assign to other function");
				} else {
					symbol_t   *ret = function_value (current_func);
					if (!ret) {
						$$ = error ($$, "cannot assign to procedure");
					} else {
						$$ = new_symbol_expr (ret);
					}
				}
			}
			$$ = assign_expr ($$, $3);
		}
	| procedure_statement
	| compound_statement
		{
			$$ = $1;
		}
	| IF expression THEN statement else statement
		{
			$$ = build_if_statement (0, $2, $4, $5, $6);
		}
	| IF expression THEN statement %prec IFX
		{
			$$ = build_if_statement (0, $2, $4, 0, 0);
		}
	| WHILE expression DO statement
		{
			$$ = build_while_statement (0, $2, $4,
										new_label_expr (),
										new_label_expr ());
		}
	| RETURN
		{
			$$ = return_expr (current_func, function_return (current_func));
		}
	;

else
	: ELSE
		{
			// this is only to get the the file and line number info
			$$ = new_nil_expr ();
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
	: expression							{ $$ = new_list_expr ($1); }
	| expression_list ',' expression		{ $$ = expr_prepend_expr ($1, $3); }
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
	| VALUE
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
	: ID
		{
			if (!$1->table) {
				error (0, "%s undefined", $1->name);
				$1->type = &type_int;
				symtab_addsymbol (current_symtab, $1);
			}
			$$ = new_symbol_expr ($1);
		}
	;

%%
