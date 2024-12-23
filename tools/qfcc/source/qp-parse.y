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
%parse-param {struct rua_ctx_s *ctx}
%define api.value.type {rua_val_t}
%define api.location.type {rua_loc_t}

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
#include "QF/va.h"

#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

#define YYDEBUG 1
#define YYERROR_VERBOSE 1
#undef YYERROR_VERBOSE

#include "tools/qfcc/source/qp-parse.h"

#define qp_yytext qp_yyget_text (ctx->scanner)
char *qp_yyget_text (void *scanner);

static void
yyerror (YYLTYPE *yylloc, rua_ctx_t *ctx, const char *s)
{
#ifdef YYERROR_VERBOSE
	error (0, "%s %s\n", qp_yytext, s);
#else
	error (0, "%s before %s", s, qp_yytext);
#endif
}

static void
parse_error (rua_ctx_t *ctx)
{
	error (0, "parse error before %s", qp_yytext);
}

#define PARSE_ERROR do { parse_error (ctx); YYERROR; } while (0)

#define YYLLOC_DEFAULT(Current, Rhs, N) RUA_LOC_DEFAULT(Current, Rhs, N)
#define YYLOCATION_PRINT rua_print_location

int yylex (void);

%}

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
%token              TRUE FALSE
%token				ELLIPSIS
%token				RESERVED

%token	PROGRAM VAR ARRAY OF FUNCTION PROCEDURE PBEGIN END IF THEN ELSE
%token	WHILE DO RANGE ASSIGNOP NOT
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
build_dotmain (symbol_t *program, rua_ctx_t *ctx)
{
	symbol_t   *dotmain = new_symbol (".main");
	expr_t     *code;
	expr_t     *exitcode;

	dotmain->params = 0;
	dotmain->type = parse_params (&type_int, 0);
	dotmain->type = find_type (dotmain->type);
	dotmain = function_symbol ((specifier_t) { .sym = dotmain }, ctx);

	exitcode = new_symbol_expr (symtab_lookup (current_symtab, "ExitCode"));

	auto spec = (specifier_t) {
		.sym = dotmain,
		.storage = current_storage,
	};
	current_func = begin_function (spec, nullptr, current_symtab, ctx);
	code = new_block_expr (0);
	code->block.scope = current_func->locals;
	auto call = new_call_expr (new_symbol_expr (program), nullptr, nullptr);
	append_expr (code, call);
	append_expr (code, new_return_expr (exitcode));
	build_code_function (spec, 0, code, ctx);
}

static const expr_t *
lvalue_expr (const expr_t *expr)
{
	if (expr->type == ex_xvalue) {
		if (expr->xvalue.lvalue) {
			// already an lvalue
			return expr;
		}
		// convert rvalue to lvalue (validity checked later)
		expr = expr->xvalue.expr;
	}
	return new_xvalue_expr (expr, true);
}

static const expr_t *
rvalue_expr (const expr_t *expr)
{
	if (expr->type == ex_xvalue) {
		if (!expr->xvalue.lvalue) {
			// already an rvalue
			return expr;
		}
		// convert lvalue to rvalue
		bug (expr, "lvalue in rvalue?");
		expr = expr->xvalue.expr;
	}
	return new_xvalue_expr (expr, false);
}

static symbol_t *
function_decl (symbol_t *sym, param_t *params, const type_t *ret_type,
			   rua_ctx_t *ctx)
{
	if (sym->table == current_symtab) {
		error (0, "%s redefined", sym->name);
		sym = new_symbol (sym->name);
	}
	// use `@name` so `main` can be used (`.main` is reserved for the entry
	// point)
	auto fsym = new_symbol (va (0, "@%s", sym->name));
	fsym->params = params;
	fsym->type = parse_params (ret_type, params);
	fsym->type = find_type (fsym->type);
	fsym = function_symbol ((specifier_t) { .sym = fsym, }, ctx);
	auto fsym_expr = new_symbol_expr (fsym);
	if (!params) {
		fsym_expr = new_call_expr (fsym_expr, nullptr, nullptr);
	}

	auto csym = new_symbol (sym->name);
	csym->sy_type = sy_xvalue;
	csym->xvalue = (sy_xvalue_t) {
		.lvalue = nullptr,
		.rvalue = fsym_expr,
	};
	symtab_addsymbol (current_symtab, csym);

	// return both symbols:
	//    lvalue has the language-level symbol
	//    rvalue has the internal function symbol
	// XXX NOTE: not a valid symbol
	sym->sy_type = sy_xvalue;
	sym->xvalue = (sy_xvalue_t) {
		.lvalue = (expr_t *) csym,
		.rvalue = (expr_t *) fsym,
	};

	return sym;
}

static symbol_t *
function_value (function_t *func)
{
	symbol_t   *ret = 0;
	if (!is_void (func->type->func.ret_type)) {
		ret = symtab_lookup (func->locals, ".ret");
		if (!ret || ret->table != func->locals) {
			ret = new_symbol_type (".ret", func->type->func.ret_type);
			initialize_def (ret, 0, func->locals->space, sc_local,
							func->locals, nullptr);
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

			auto spec = (specifier_t) {
				.sym = $1,
				.storage = current_storage,
			};
			current_func = begin_function (spec, nullptr, current_symtab, ctx);
			current_symtab = current_func->locals;
			build_code_function (spec, 0, $4, ctx);
			current_symtab = st;

			build_dotmain ($1, ctx);
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
								current_symtab, nullptr);
				if (sym->def) {
					sym->def->nosave = 1;
				}
			}
			$$->type = parse_params (&type_void, 0);
			$$->type = find_type ($$->type);
			$$ = function_symbol ((specifier_t) { .sym = $$ }, ctx);
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
								current_symtab, nullptr);
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
			auto sym = $1;
			// always an sy_xvalue with callable symbol in lvalue and
			// actual function symbol in rvalue
			auto csym = (symbol_t *) sym->xvalue.lvalue;
			auto fsym = (symbol_t *) sym->xvalue.rvalue;
			auto spec = (specifier_t) {
				.sym = fsym,
				.storage = current_storage,
			};
			current_func = begin_function (spec, sym->name, current_symtab,
										   ctx);
			current_symtab = current_func->locals;
			current_storage = sc_local;
			// null for procedures, valid symbol expression for functions
			csym->xvalue.lvalue = function_return (current_func);
			$<spec>$ = spec;
		}
	  declarations compound_statement ';'
		{
			auto spec = $<spec>3;
			auto statements = $5;
			if (!statements) {
				statements = new_block_expr (0);
				statements->block.scope = current_symtab;
			}
			auto ret_expr = new_return_expr (function_return (current_func));
			append_expr (statements, ret_expr);
			build_code_function (spec, 0, statements, ctx);
			current_symtab = current_func->parameters->parent;
			current_storage = spec.storage;
		}
	| subprogram_head ASSIGNOP '#' VALUE ';'
		{
			auto sym = $1;
			// always an sy_xvalue with callable symbol in lvalue and
			// actual function symbol in rvalue
			auto spec = (specifier_t) {
				.sym = (symbol_t *) sym->xvalue.rvalue,
				.storage = current_storage,
			};
			build_builtin_function (spec, sym->name, $4);
		}
	;

subprogram_head
	: FUNCTION ID arguments ':' standard_type
		{
			$$ = function_decl ($2, $3, $5, ctx);
		}
	| PROCEDURE ID arguments
		{
			$$ = function_decl ($2, $3, &type_void, ctx);
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
			$$->block.scope = current_symtab;
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
			auto lvalue = lvalue_expr ($variable);
			$$ = new_assign_expr (lvalue, $expression);
		}
	| procedure_statement
	| compound_statement
		{
			$$ = $1;
		}
	| IF expression[test] THEN statement[true] else statement[false]
		{
			$$ = new_select_expr (false, $test, $true, $else, $false);
		}
	| IF expression[test] THEN statement[true] %prec IFX
		{
			$$ = new_select_expr (false, $test, $true, nullptr, nullptr);
		}
	| WHILE expression[test] DO statement[body]
		{
			$$ = new_loop_expr (false, false, $test, $body, nullptr,
								nullptr, nullptr);
		}
	| RETURN
		{
			$$ = new_return_expr (function_return (current_func));
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
	: name									{ $$ = rvalue_expr ($1); }
	| name '[' expression ']'				{ $$ = new_array_expr ($1, $3); }
	;

//FIXME calling a procedure that takes no parameters with parameters
// results in "Called object is not a function", which I'm sure will be
// very confusing (especially for prececures) as the called object sure looks
// like a function/procedure.
procedure_statement
	: name									{ $$ = rvalue_expr ($1); }
	| name '(' expression_list ')'
		{
			$$ = new_call_expr (rvalue_expr ($1), $3, nullptr);
		}
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
	: variable						{ $$ = rvalue_expr ($1); }
	| VALUE
	| name '(' expression_list ')'
		{
			//FIXME see procedure_statement
			$$ = new_call_expr (rvalue_expr ($1), $3, nullptr);
		}
	| '(' expression ')'			{ $$ = $2; }
	;

expression
	: unary_expr
	| expression RELOP expression		{ $$ = new_binary_expr ($2, $1, $3); }
	| expression ADDOP expression
		{
			int op = $2;
			if (op == 'o') {
				op = OR;
			}
			$$ = new_binary_expr (op, $1, $3);
		}
	| expression MULOP expression
		{
			int op = $2;
			if (op == 'd') {
				op = '/';
			} else if (op == 'm') {
				op = '%';
			} else if (op == 'a') {
				op = AND;
			}
			$$ = new_binary_expr (op, $1, $3);
		}
	;

sign
	: ADDOP
		{
			if ($$ == 'o') {
				// no unary `or`
				PARSE_ERROR;
			}
			$$ = $1;
		}
	;

name
	: ID						{ $$ = new_symbol_expr ($1); }
	;

%%
