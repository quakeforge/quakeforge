/*
	glsl-parse.y

	parser for GLSL

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2023/11/25

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
%define api.prefix {glsl_yy}
%define api.pure full
%define api.push-pull push
%define api.token.prefix {GLSL_}
%locations
%parse-param {void *scanner}
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
#include <stdlib.h>

#include <QF/hash.h>
#include <QF/sys.h>
#include <QF/va.h>

#define GLSL_YYDEBUG 1
#define GLSL_YYERROR_VERBOSE 1
#undef GLSL_YYERROR_VERBOSE

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/switch.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/glsl-parse.h"

#define glsl_yytext qc_yyget_text (scanner)
char *qc_yyget_text (void *scanner);

static void
yyerror (YYLTYPE *yylloc, void *scanner, const char *s)
{
#ifdef GLSL_YYERROR_VERBOSE
	error (0, "%s %s\n", glsl_yytext, s);
#else
	error (0, "%s before %s", s, glsl_yytext);
#endif
}

static void __attribute__((used))
parse_error (void *scanner)
{
	error (0, "parse error before %s", glsl_yytext);
}

#define PARSE_ERROR do { parse_error (scanner); YYERROR; } while (0)

#define YYLLOC_DEFAULT(Current, Rhs, N) RUA_LOC_DEFAULT(Current, Rhs, N)
#define YYLOCATION_PRINT rua_print_location

int yylex (YYSTYPE *yylval, YYLTYPE *yylloc);

%}

%code requires { #define glsl_yypstate rua_yypstate }

// these tokens are common between qc and qp
%left LOW
%nonassoc IFX
%nonassoc ELSE
%nonassoc BREAK_PRIMARY
%nonassoc ';'
%nonassoc CLASS_NOT_CATEGORY
%nonassoc STORAGEX

%left			COMMA
%right	<op>	'=' ASX
%right			'?' ':'
%left			OR
%left			XOR
%left			AND
%left			'|'
%left			'^'
%left			'&'
%left			EQ NE
%left			LT GT GE LE
%token			NAND NOR XNOR
// end of tokens common between qc and qp

%left			SHL SHR
%left			'+' '-'
%left			'*' '/' '%' MOD SCALE GEOMETRIC QMUL QVMUL VQMUL
%left           HADAMARD CROSS DOT WEDGE REGRESSIVE
%right	<op>	SIZEOF UNARY INCOP REVERSE STAR DUAL UNDUAL
%left			HYPERUNARY
%left			'.' '(' '['

%token	<expr>		VALUE STRING TOKEN
%token              ELLIPSIS
%token              RESERVED
// end of common tokens

%token	<symbol>	IDENTIFIER

%token				WHILE DO IF ELSE FOR BREAK CONTINUE
%token				RETURN
%token				SWITCH CASE DEFAULT
%token	<op>		STRUCT
%token	<spec>		TYPE_SPEC TYPE_NAME TYPE_QUAL VOID

%token <spec> PRECISION INVARIANT SMOOTH FLAT NOPERSPECTIVE LAYOUT SHARED
%token <spec> PRECISE CONST IN OUT INOUT CENTROID PATCH SAMPLE UNIFORM BUFFER VOLATILE
%token <spec> RESTRICT READONLY WRITEONLY HIGH_PRECISION MEDIUM_PRECISION
%token <spec> LOW_PRECISION DISCARD COHERENT

%type <symbol>  variable_identifier
%type <expr>    expression primary_exprsssion assignment_expression
%type <expr>    for_init_statement conditionopt expressionopt else
%type <expr>    conditional_expression unary_expression postfix_expression
%type <expr>    function_call function_call_or_method function_call_generic
%type <expr>    function_call_header_with_parameters
%type <expr>    function_call_header_no_parameters
%type <expr>    function_call_header function_identifier
%type <expr>    logical_or_expression logical_xor_expression
%type <expr>    logical_and_expression
%type <expr>    inclusive_or_expression exclusive_or_expression and_expression
%type <expr>    equality_expression relational_expression
%type <expr>    shift_expression additive_expression multiplicative_expression
%type <expr>    integer_expression
%type <expr>    initializer
%type <expr>    condition
%type <expr>    compound_statement_no_new_scope compound_statement
%type <expr>    statement statement_no_new_scope statement_list
%type <expr>    simple_statement expression_statement
%type <expr>    declaration_statement selection_statement
%type <expr>    switch_statement switch_statement_list case_label
%type <switch_block> switch_block
%type <expr>    iteration_statement jump_statement
%type <expr>    break_label continue_label
%type <mut_expr> initializer_list
%type <op>      unary_operator assignment_operator

%type <spec>    function_prototype function_declarator
%type <spec>    function_header function_header_with_parameters
%type <spec>    fully_specified_type type_specifier struct_specifier
%type <spec>    type_qualifier single_type_qualifier type_specifier_nonarray
%type <spec>    precision_qualifier interpolation_qualifier invariant_qualifier
%type <spec>    precise_qualifer storage_qualifier layout_qualifier

%type <size>    array_size
%type <type>    array_specifier

%type <param>   parameter_declaration
%type <param>   parameter_declarator parameter_type_specifier

%{

static switch_block_t *switch_block;
static const expr_t *break_label;
static const expr_t *continue_label;

static symbol_t *
function_sym_type (specifier_t spec, symbol_t *sym)
{
	sym->type = append_type (spec.sym->type, spec.type);
	set_func_type_attrs (sym->type, spec);
	sym->type = find_type (sym->type);
	return sym;
}


%}

%expect 0

%%

translation_unit
	: external_declaration
	| translation_unit external_declaration
	;

external_declaration
	: function_definition
	| declaration
	| ';'
	;

function_definition
	: function_prototype
		{
			$<symtab>$ = current_symtab;
			auto spec = $1;
			spec.sym->type = parse_params (spec.sym->type, spec.params);
			auto sym = function_sym_type (spec, spec.sym);
			sym->params = spec.params;
			sym = function_symbol ((specifier_t) {
									.sym = sym,
									.is_overload = true
								   });
			current_func = begin_function (sym, nullptr, current_symtab,
										   false, spec.storage);
			current_symtab = current_func->locals;
			current_storage = sc_local;
		}
	  compound_statement_no_new_scope
		{
			auto spec = $1;
			auto sym = spec.sym;
			build_code_function (sym, nullptr, (expr_t *) $3);
			current_symtab = $<symtab>2;
			current_storage = sc_global;//FIXME
			current_func = nullptr;
		}
	;

variable_identifier
	: IDENTIFIER
	;

primary_exprsssion
	: variable_identifier	{ $$ = new_symbol_expr ($1); }
	| VALUE
	| '(' expression ')'	{ $$ = $2; ((expr_t *) $$)->paren = 1; }
	;

postfix_expression
	: primary_exprsssion
	| postfix_expression '[' integer_expression ']'
		{
			$$ = array_expr ($1, $3);
		}
	| function_call
	| postfix_expression '.' IDENTIFIER/*FIELD_SELECTION*/
		{
			auto sym = new_symbol_expr ($3);
			$$ = field_expr ($1, sym);
		}
	| postfix_expression INCOP			{ $$ = incop_expr ($2, $1, 1); }
	;

integer_expression
	: expression
	;

function_call
	: function_call_or_method
	;

function_call_or_method
	: function_call_generic
	;

function_call_generic
	: function_call_header_with_parameters ')'
	| function_call_header_no_parameters ')'
	;

function_call_header_no_parameters
	: function_call_header VOID
	| function_call_header
	;

function_call_header_with_parameters
	: function_call_header assignment_expression
	| function_call_header_with_parameters ',' assignment_expression
	;

function_call_header
	: function_identifier '('
	;

function_identifier
	: type_specifier					{ }
	| postfix_expression
	;

unary_expression
	: postfix_expression
	| INCOP unary_expression			{ $$ = incop_expr ($1, $2, 0); }
	| unary_operator unary_expression	{ $$ = unary_expr ($1, $2); }
	;

unary_operator
	: '+'								{ $$ = '+'; }
	| '-'								{ $$ = '-'; }
	| '!'								{ $$ = '!'; }
	| '~'								{ $$ = '~'; }
	;

multiplicative_expression
	: unary_expression
	| multiplicative_expression '*' unary_expression
		{
			$$ = binary_expr ('*', $1, $3);
		}
	| multiplicative_expression '/' unary_expression
		{
			$$ = binary_expr ('/', $1, $3);
		}
	| multiplicative_expression '%' unary_expression
		{
			$$ = binary_expr ('%', $1, $3);
		}
	;

additive_expression
	: multiplicative_expression
	| additive_expression '+' unary_expression
		{
			$$ = binary_expr ('+', $1, $3);
		}
	| additive_expression '-' unary_expression
		{
			$$ = binary_expr ('-', $1, $3);
		}
	;

shift_expression
	: additive_expression
	| shift_expression SHL additive_expression
		{
			$$ = binary_expr (QC_SHL, $1, $3);
		}
	| shift_expression SHR additive_expression
		{
			$$ = binary_expr (QC_SHR, $1, $3);
		}
	;

relational_expression
	: shift_expression
	| relational_expression LT shift_expression
		{
			$$ = binary_expr (QC_LT, $1, $3);
		}
	| relational_expression GT shift_expression
		{
			$$ = binary_expr (QC_GT, $1, $3);
		}
	| relational_expression LE shift_expression
		{
			$$ = binary_expr (QC_LE, $1, $3);
		}
	| relational_expression GE shift_expression
		{
			$$ = binary_expr (QC_GE, $1, $3);
		}
	;

equality_expression
	: relational_expression
	| equality_expression EQ relational_expression
		{
			$$ = binary_expr (QC_EQ, $1, $3);
		}
	| relational_expression NE relational_expression
		{
			$$ = binary_expr (QC_NE, $1, $3);
		}
	;

and_expression
	: equality_expression
	| and_expression '&' equality_expression
		{
			$$ = binary_expr ('&', $1, $3);
		}
	;

exclusive_or_expression
	: and_expression
	| exclusive_or_expression '^' and_expression
		{
			$$ = binary_expr ('^', $1, $3);
		}
	;

inclusive_or_expression
	: exclusive_or_expression
	| inclusive_or_expression '|' exclusive_or_expression
		{
			$$ = binary_expr ('|', $1, $3);
		}
	;

logical_and_expression
	: inclusive_or_expression
	| logical_and_expression AND inclusive_or_expression
		{
			$$ = bool_expr (QC_AND, nullptr, $1, $3);
		}
	;

logical_xor_expression
	: logical_and_expression
	| logical_xor_expression XOR logical_and_expression
		{
			$$ = bool_expr (QC_XOR, nullptr, $1, $3);
		}
	;

logical_or_expression
	: logical_xor_expression
	| logical_or_expression OR logical_xor_expression
		{
			$$ = bool_expr (QC_OR, nullptr, $1, $3);
		}
	;

conditional_expression
	: logical_or_expression
	| logical_or_expression '?' expression ':' assignment_expression
		{
			$$ = conditional_expr ($1, $3, $5);
		}
	;

assignment_expression
	: conditional_expression
	| unary_expression assignment_operator assignment_expression
		{
			if ($2 == '=') {
				$$ = assign_expr ($1, $3);
			} else {
				$$ = asx_expr ($2, $1, $3);
			}
		}
	;

assignment_operator
	: '='								{ $$ = '='; }
	| ASX
	;

expression
	: assignment_expression
	| expression ',' assignment_expression
	;

constant_expression
	: conditional_expression
	;

declaration
	: function_prototype ';'
	| init_declarator_list ';'
	| PRECISION precision_qualifier type_specifier ';'
	| type_qualifier IDENTIFIER '{' struct_declaration_list '}' ';'
	| type_qualifier IDENTIFIER '{' struct_declaration_list '}' IDENTIFIER ';'
	| type_qualifier IDENTIFIER '{' struct_declaration_list '}' IDENTIFIER array_specifier ';'
	| type_qualifier ';'
	| type_qualifier IDENTIFIER ';'
	| type_qualifier IDENTIFIER identifier_list ';'
	;

identifier_list
	: ',' IDENTIFIER
	| identifier_list ',' IDENTIFIER
	;

function_prototype
	: function_declarator ')'
	;

function_declarator
	: function_header
	| function_header_with_parameters
	;

function_header_with_parameters
	: function_header parameter_declaration
		{
			auto spec = $1;
			spec.params = $2;
			$$ = spec;
		}
	| function_header_with_parameters ',' parameter_declaration
		{
			auto spec = $1;
			spec.params = append_params (spec.params, $3);
			$$ = spec;
		}
	;

function_header
	: fully_specified_type IDENTIFIER '('
		{
			auto spec = $1;
			if ($2->table) {
				spec.sym = new_symbol ($2->name);
			} else {
				spec.sym = $2;
			}
			$$ = spec;
		}
	;

parameter_declarator
	: type_specifier IDENTIFIER
		{
			$$ = new_param (nullptr, $1.type, $2->name);
		}
	| type_specifier IDENTIFIER array_specifier
		{
			$$ = new_param (nullptr, append_type ($1.type, $3), $2->name);
		}
	;

parameter_declaration
	: type_qualifier parameter_declarator		{ $$ = $2; }//XXX
	| parameter_declarator
	| type_qualifier parameter_type_specifier	{ $$ = $2; }//XXX
	| parameter_type_specifier
	;

parameter_type_specifier
	: type_specifier
		{
			$$ = new_param (nullptr, $1.type, nullptr);
		}
	;

init_declarator_list
	: single_declaration
	| init_declarator_list ',' IDENTIFIER
		{
			auto symtab = current_symtab;
			auto space = symtab->space;
			auto storage = current_storage;
			initialize_def ($3, nullptr, space, storage, symtab);
		}
	| init_declarator_list ',' IDENTIFIER array_specifier
	| init_declarator_list ',' IDENTIFIER array_specifier '=' initializer
	| init_declarator_list ',' IDENTIFIER '=' initializer
		{
			auto symtab = current_symtab;
			auto space = symtab->space;
			auto storage = current_storage;
			initialize_def ($3, $5, space, storage, symtab);
		}
	;

single_declaration
	: fully_specified_type
	| fully_specified_type IDENTIFIER
	| fully_specified_type IDENTIFIER array_specifier
	| fully_specified_type IDENTIFIER array_specifier '=' initializer
	| fully_specified_type IDENTIFIER '=' initializer
	;

fully_specified_type
	: type_specifier
	| type_qualifier type_specifier
	;

invariant_qualifier
	: INVARIANT
	;

interpolation_qualifier
	: SMOOTH
	| FLAT
	| NOPERSPECTIVE
	;

layout_qualifier
	: LAYOUT '(' layout_qualifer_id_list ')'
	;

layout_qualifer_id_list
	: layout_qualifer_id
	| layout_qualifer_id_list ',' layout_qualifer_id
	;

layout_qualifer_id
	: IDENTIFIER
	| IDENTIFIER '=' constant_expression
	| SHARED
	;

precise_qualifer
	: PRECISE
	;

type_qualifier
	: single_type_qualifier
	| type_qualifier single_type_qualifier
	;

single_type_qualifier
	: storage_qualifier
	| layout_qualifier
	| precision_qualifier
	| interpolation_qualifier
	| invariant_qualifier
	| precise_qualifer
	;

storage_qualifier
	: CONST
	| IN
	| OUT
	| INOUT
	| CENTROID
	| PATCH
	| SAMPLE
	| UNIFORM
	| BUFFER
	| SHARED
	| COHERENT
	| VOLATILE
	| RESTRICT
	| READONLY
	| WRITEONLY
	;

type_specifier
	: type_specifier_nonarray
	| type_specifier_nonarray array_specifier
	;

array_specifier
	: array_size				{ $$ = (type_t *) array_type (nullptr, $1); }
	| array_specifier array_size
		{
			$$ = (type_t *) append_type ($1, array_type (nullptr, $2));
		}
	;

array_size
	: '[' ']'							{ $$ = 0; }
	| '[' conditional_expression ']'
		{
			if (is_int_val ($2) && expr_int ($2) > 0) {
				$$ = expr_int ($2);
			} else if (is_uint_val ($2) && expr_uint ($2) > 0) {
				$$ = expr_uint ($2);
			} else{
				error (0, "invalid array size");
				$$ = 0;
			}
		}
	;

type_specifier_nonarray
	: VOID
	| TYPE_SPEC
	| struct_specifier
	| TYPE_NAME
	;

precision_qualifier
	: HIGH_PRECISION
	| MEDIUM_PRECISION
	| LOW_PRECISION
	;

struct_specifier
	: STRUCT IDENTIFIER '{'
		{
			int op = 's';
			current_symtab = start_struct (&op, $2, current_symtab);
		}
	  struct_declaration_list '}'
		{
			auto symtab = current_symtab;
			current_symtab = symtab->parent;

			auto sym = build_struct ('s', $2, symtab, nullptr, 0);
			if (!sym->table) {
				symtab_addsymbol (current_symtab, sym);
			}
			auto s = $2;
			s->sy_type = sy_type;
			s->type = find_type (alias_type (sym->type, sym->type, s->name));
			symtab_addsymbol (current_symtab, s);
		}
	| STRUCT '{'
		{
			int op = 's';
			current_symtab = start_struct (&op, nullptr, current_symtab);
		}
	  struct_declaration_list '}'
		{
			auto symtab = current_symtab;
			current_symtab = symtab->parent;

			auto sym = build_struct ('s', nullptr, symtab, nullptr, 0);
			if (!sym->table) {
				symtab_addsymbol (current_symtab, sym);
			}
		}
	;

struct_declaration_list
	: struct_declaration
	| struct_declaration_list struct_declaration
	;

struct_declaration
	: type_specifier struct_declarator_list ';'
	| type_qualifier type_specifier struct_declarator_list ';'
	;

struct_declarator_list
	: struct_declarator
	| struct_declarator_list ',' struct_declarator
	;

struct_declarator
	: IDENTIFIER
	| IDENTIFIER array_specifier
	;

initializer
	: assignment_expression
	| '{' initializer_list '}'		{ $$ = $2; }
	| '{' initializer_list ',' '}'	{ $$ = $2; }
	;

initializer_list
	: initializer
		{
			$$ = new_compound_init ();
			append_element ($$, new_element ($1, nullptr));
		}
	| initializer_list ',' initializer
		{
			append_element ($$, new_element ($3, nullptr));
		}
	;

declaration_statement
	: declaration				{ $$ = nullptr; }
	;

statement
	: compound_statement
	| simple_statement
	;

simple_statement
	: declaration_statement
	| expression_statement
	| selection_statement
	| switch_statement
	| case_label
	| iteration_statement
	| jump_statement
	;

compound_statement
	: '{' '}'							{ $$ = nullptr; }
	| '{' statement_list '}'			{ $$ = $2; }
	;

statement_no_new_scope
	: compound_statement_no_new_scope
	| simple_statement
	;

compound_statement_no_new_scope
	: '{' '}'							{ $$ = nullptr; }
	| '{' statement_list '}'			{ $$ = $2; }
	;

statement_list
	: statement
		{
			auto list = new_block_expr (nullptr);
			append_expr (list, $1);
			$$ = list;
		}
	| statement_list statement
		{
			auto list = (expr_t *) $1;
			append_expr (list, $2);
			$$ = list;
		}
	;

expression_statement
	: ';'						{ $$ = nullptr; }
	| expression ';'			{ $$ = (expr_t *) $expression; }
	;

selection_statement
//	: IF '(' expression ')' selection_rest_statement
	: IF '(' expression ')' statement[true] %prec IFX
		{
			$$ = build_if_statement (false, $expression,
									 $true, nullptr, nullptr);
		}
	| IF '(' expression ')' statement[true] else statement[false]
		{
			$$ = build_if_statement (false, $expression,
									 $true, $else, $false);
		}
	;

else
	: ELSE
		{
			// this is only to get the the file and line number info
			$$ = new_nil_expr ();
		}
	;

//selection_rest_statement
//	: statement ELSE statement
//	| statement %prec IFX
//	;

condition
	: expression
	| fully_specified_type IDENTIFIER '=' initializer
		{
			auto symtab = current_symtab;
			auto space = symtab->space;
			auto storage = current_storage;
			initialize_def ($2, $initializer, space, storage, symtab);
		}
	;

break_label
	: /* empty */
		{
			$$ = break_label;
			break_label = new_label_expr ();
		}
	;

continue_label
	: /* empty */
		{
			$$ = continue_label;
			continue_label = new_label_expr ();
		}
	;

switch_statement
	: SWITCH break_label '(' expression switch_block')'
			'{' switch_statement_list '}'
		{
			$$ = switch_expr (switch_block, break_label,
							  $switch_statement_list);
			switch_block = $switch_block;
			break_label = $break_label;
		}
	;

switch_block
	: /* empty */
		{
			$$ = switch_block;
			switch_block = new_switch_block ();
			switch_block->test = $<expr>0;
		}
	;

switch_statement_list
	: /* empty */				{ $$ = nullptr; }
	| statement_list
	;

case_label
	: CASE expression ':'	{ $$ = case_label_expr (switch_block, $2); }
	| DEFAULT ':'			{ $$ = case_label_expr (switch_block, nullptr); }
	;

iteration_statement
	: WHILE break_label continue_label '(' condition ')' statement_no_new_scope
		{
			$$ = build_while_statement (false, $continue_label,
										$statement_no_new_scope, break_label,
										continue_label);
			break_label = $break_label;
			continue_label = $continue_label;
		}
	| DO break_label continue_label statement WHILE '(' expression ')' ';'
		{
			$$ = build_do_while_statement ($statement, false, $expression,
										   break_label, continue_label);
			break_label = $break_label;
			continue_label = $continue_label;
		}
	| FOR break_label continue_label
//			'(' for_init_statement for_rest_statement ')'
			'(' for_init_statement ';' conditionopt ';' expressionopt ')'
			statement_no_new_scope
		{
			if ($for_init_statement) {
				$for_init_statement = build_block_expr ((expr_t *) $for_init_statement,
														false);
			}
			$$ = build_for_statement ($for_init_statement, $conditionopt,
									  $expressionopt, $statement_no_new_scope,
									  break_label, continue_label);
			break_label = $break_label;
			continue_label = $continue_label;
		}
	;

for_init_statement
	: expression_statement	{ $$ = $1; }
	| declaration_statement	{ $$ = $1; }
	;

conditionopt
	: condition
	| /* emtpy */	{ $$ = nullptr; }
	;

expressionopt
	: expression
	| /* emtpy */	{ $$ = nullptr; }
	;
/*
for_rest_statement
	: conditionopt ';'
	| conditionopt ';' expression
	;
*/
jump_statement
	: CONTINUE ';'
		{
			$$ = nullptr;
			if (continue_label) {
				$$ = goto_expr (continue_label);
			} else {
				error (nullptr, "continue outside of loop");
			}
		}
	| BREAK ';'
		{
			$$ = nullptr;
			if (break_label) {
				$$ = goto_expr (break_label);
			} else {
				error (nullptr, "continue outside of loop or switch");
			}
		}
	| RETURN ';'				{ $$ = return_expr (current_func, nullptr); }
	| RETURN expression ';'		{ $$ = return_expr (current_func, $2); }
	| DISCARD ';'				{ $$ = nullptr; } //XXX
	;

%%


static keyword_t glsl_keywords[] = {
	{"const",           GLSL_CONST},
	{"uniform",         GLSL_UNIFORM},
	{"buffer",          GLSL_BUFFER},
	{"shared",          GLSL_SHARED},
	{"attribute",       GLSL_RESERVED},
	{"varying",         GLSL_RESERVED},
	{"coherent",        GLSL_COHERENT},
	{"volatile",        GLSL_VOLATILE},
	{"restrict",        GLSL_RESTRICT},
	{"readonly",        GLSL_READONLY},
	{"writeonly",       GLSL_WRITEONLY},
	{"atomic_uint",     GLSL_RESERVED},
	{"layout",          GLSL_LAYOUT},
	{"centroid",        GLSL_CENTROID},
	{"flat",            GLSL_FLAT},
	{"smooth",          GLSL_SMOOTH},
	{"noperspective",   GLSL_NOPERSPECTIVE},
	{"patch",           GLSL_PATCH},
	{"sample",          GLSL_SAMPLE},
	{"invariant",       GLSL_INVARIANT},
	{"precise",         GLSL_PRECISE},
	{"break",           GLSL_BREAK},
	{"continue",        GLSL_CONTINUE},
	{"do",              GLSL_DO},
	{"for",             GLSL_FOR},
	{"while",           GLSL_WHILE},
	{"switch",          GLSL_SWITCH},
	{"case",            GLSL_CASE},
	{"default",         GLSL_DEFAULT},
	{"if",              GLSL_IF},
	{"else",            GLSL_ELSE},
	{"subroutine",      GLSL_RESERVED},
	{"in",              GLSL_IN},
	{"out",             GLSL_OUT},
	{"inout",           GLSL_INOUT},
	{"int",             GLSL_TYPE_SPEC, .spec = {.type = &type_int}},
	{"void",            GLSL_VOID, .spec = {.type = &type_void}},
	{"bool",            GLSL_TYPE_SPEC, .spec = {.type = &type_bool}},
	{"true",            0},
	{"false",           0},
	{"float",           GLSL_TYPE_SPEC, .spec = {.type = &type_float}},
	{"double",          GLSL_TYPE_SPEC, .spec = {.type = &type_double}},
	{"discard",         GLSL_DISCARD},
	{"return",          GLSL_RETURN},
	{"vec2",            GLSL_TYPE_SPEC, .spec = {.type = &type_vec2}},
	{"vec3",            GLSL_TYPE_SPEC, .spec = {.type = &type_vec3}},
	{"vec4",            GLSL_TYPE_SPEC, .spec = {.type = &type_vec4}},
	{"ivec2",           GLSL_TYPE_SPEC, .spec = {.type = &type_ivec2}},
	{"ivec3",           GLSL_TYPE_SPEC, .spec = {.type = &type_ivec3}},
	{"ivec4",           GLSL_TYPE_SPEC, .spec = {.type = &type_ivec4}},
	{"bvec2",           GLSL_TYPE_SPEC, .spec = {.type = &type_bvec2}},
	{"bvec3",           GLSL_TYPE_SPEC, .spec = {.type = &type_bvec3}},
	{"bvec4",           GLSL_TYPE_SPEC, .spec = {.type = &type_bvec4}},
	{"uint",            GLSL_TYPE_SPEC, .spec = {.type = &type_uint}},
	{"uvec2",           GLSL_TYPE_SPEC, .spec = {.type = &type_uivec2}},
	{"uvec3",           GLSL_TYPE_SPEC, .spec = {.type = &type_uivec3}},
	{"uvec4",           GLSL_TYPE_SPEC, .spec = {.type = &type_uivec4}},
	{"dvec2",           GLSL_TYPE_SPEC, .spec = {.type = &type_dvec2}},
	{"dvec3",           GLSL_TYPE_SPEC, .spec = {.type = &type_dvec3}},
	{"dvec4",           GLSL_TYPE_SPEC, .spec = {.type = &type_dvec4}},
	{"mat2",            GLSL_TYPE_SPEC, .spec = {.type = &type_mat2x2}},
	{"mat3",            GLSL_TYPE_SPEC, .spec = {.type = &type_mat3x3}},
	{"mat4",            GLSL_TYPE_SPEC, .spec = {.type = &type_mat4x4}},
	{"mat2x2",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat2x2}},
	{"mat2x3",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat2x3}},
	{"mat2x4",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat2x4}},
	{"mat3x2",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat3x2}},
	{"mat3x3",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat3x3}},
	{"mat3x4",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat3x4}},
	{"mat4x2",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat4x2}},
	{"mat4x3",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat4x3}},
	{"mat4x4",          GLSL_TYPE_SPEC, .spec = {.type = &type_mat4x4}},
	{"dmat2",           GLSL_TYPE_SPEC, .spec = {.type = &type_dmat2x2}},
	{"dmat3",           GLSL_TYPE_SPEC, .spec = {.type = &type_dmat3x3}},
	{"dmat4",           GLSL_TYPE_SPEC, .spec = {.type = &type_dmat4x4}},
	{"dmat2x2",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat2x2}},
	{"dmat2x3",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat2x3}},
	{"dmat2x4",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat2x4}},
	{"dmat3x2",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat3x2}},
	{"dmat3x3",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat3x3}},
	{"dmat3x4",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat3x4}},
	{"dmat4x2",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat4x2}},
	{"dmat4x3",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat4x3}},
	{"dmat4x4",         GLSL_TYPE_SPEC, .spec = {.type = &type_dmat4x4}},

	{"lowp",                    GLSL_LOW_PRECISION},
	{"mediump",                 GLSL_MEDIUM_PRECISION},
	{"highp",                   GLSL_HIGH_PRECISION},
	{"precision",               GLSL_PRECISION},
	{"sampler1D",               GLSL_TYPE_SPEC},	// spec init at runtime
	{"sampler1DShadow",         GLSL_TYPE_SPEC},
	{"sampler1DArray",          GLSL_TYPE_SPEC},
	{"sampler1DArrayShadow",    GLSL_TYPE_SPEC},
	{"isampler1D",              GLSL_TYPE_SPEC},
	{"isampler1DArray",         GLSL_TYPE_SPEC},
	{"usampler1D",              GLSL_TYPE_SPEC},
	{"usampler1DArray",         GLSL_TYPE_SPEC},
	{"sampler2D",               GLSL_TYPE_SPEC},
	{"sampler2DShadow",         GLSL_TYPE_SPEC},
	{"sampler2DArray",          GLSL_TYPE_SPEC},
	{"sampler2DArrayShadow",    GLSL_TYPE_SPEC},
	{"isampler2D",              GLSL_TYPE_SPEC},
	{"isampler2DArray",         GLSL_TYPE_SPEC},
	{"usampler2D",              GLSL_TYPE_SPEC},
	{"usampler2DArray",         GLSL_TYPE_SPEC},
	{"sampler2DRect",           GLSL_TYPE_SPEC},
	{"sampler2DRectShadow",     GLSL_TYPE_SPEC},
	{"isampler2DRect",          GLSL_TYPE_SPEC},
	{"usampler2DRect",          GLSL_TYPE_SPEC},
	{"sampler2DMS",             GLSL_TYPE_SPEC},
	{"isampler2DMS",            GLSL_TYPE_SPEC},
	{"usampler2DMS",            GLSL_TYPE_SPEC},
	{"sampler2DMSArray",        GLSL_TYPE_SPEC},
	{"isampler2DMSArray",       GLSL_TYPE_SPEC},
	{"usampler2DMSArray",       GLSL_TYPE_SPEC},
	{"sampler3D",               GLSL_TYPE_SPEC},
	{"isampler3D",              GLSL_TYPE_SPEC},
	{"usampler3D",              GLSL_TYPE_SPEC},
	{"samplerCube",             GLSL_TYPE_SPEC},
	{"samplerCubeShadow",       GLSL_TYPE_SPEC},
	{"isamplerCube",            GLSL_TYPE_SPEC},
	{"usamplerCube",            GLSL_TYPE_SPEC},
	{"samplerCubeArray",        GLSL_TYPE_SPEC},
	{"samplerCubeArrayShadow",  GLSL_TYPE_SPEC},
	{"isamplerCubeArray",       GLSL_TYPE_SPEC},
	{"usamplerCubeArray",       GLSL_TYPE_SPEC},
	{"samplerBuffer",           GLSL_TYPE_SPEC},
	{"isamplerBuffer",          GLSL_TYPE_SPEC},
	{"usamplerBuffer",          GLSL_TYPE_SPEC},
	{"image1D",                 GLSL_TYPE_SPEC},
	{"iimage1D",                GLSL_TYPE_SPEC},
	{"uimage1D",                GLSL_TYPE_SPEC},
	{"image1DArray",            GLSL_TYPE_SPEC},
	{"iimage1DArray",           GLSL_TYPE_SPEC},
	{"uimage1DArray",           GLSL_TYPE_SPEC},
	{"image2D",                 GLSL_TYPE_SPEC},
	{"iimage2D",                GLSL_TYPE_SPEC},
	{"uimage2D",                GLSL_TYPE_SPEC},
	{"image2DArray",            GLSL_TYPE_SPEC},
	{"iimage2DArray",           GLSL_TYPE_SPEC},
	{"uimage2DArray",           GLSL_TYPE_SPEC},
	{"image2DRect",             GLSL_TYPE_SPEC},
	{"iimage2DRect",            GLSL_TYPE_SPEC},
	{"uimage2DRect",            GLSL_TYPE_SPEC},
	{"image2DMS",               GLSL_TYPE_SPEC},
	{"iimage2DMS",              GLSL_TYPE_SPEC},
	{"uimage2DMS",              GLSL_TYPE_SPEC},
	{"image2DMSArray",          GLSL_TYPE_SPEC},
	{"iimage2DMSArray",         GLSL_TYPE_SPEC},
	{"uimage2DMSArray",         GLSL_TYPE_SPEC},
	{"image3D",                 GLSL_TYPE_SPEC},
	{"iimage3D",                GLSL_TYPE_SPEC},
	{"uimage3D",                GLSL_TYPE_SPEC},
	{"imageCube",               GLSL_TYPE_SPEC},
	{"iimageCube",              GLSL_TYPE_SPEC},
	{"uimageCube",              GLSL_TYPE_SPEC},
	{"imageCubeArray",          GLSL_TYPE_SPEC},
	{"iimageCubeArray",         GLSL_TYPE_SPEC},
	{"uimageCubeArray",         GLSL_TYPE_SPEC},
	{"imageBuffer",             GLSL_TYPE_SPEC},
	{"iimageBuffer",            GLSL_TYPE_SPEC},
	{"uimageBuffer",            GLSL_TYPE_SPEC},

	{"struct",                  GLSL_STRUCT},

	//vulkan
	{"texture1D",               GLSL_TYPE_SPEC},	// spec init at runtime
	{"texture1DArray",          GLSL_TYPE_SPEC},
	{"itexture1D",              GLSL_TYPE_SPEC},
	{"itexture1DArray",         GLSL_TYPE_SPEC},
	{"utexture1D",              GLSL_TYPE_SPEC},
	{"utexture1DArray",         GLSL_TYPE_SPEC},
	{"texture2D",               GLSL_TYPE_SPEC},
	{"texture2DArray",          GLSL_TYPE_SPEC},
	{"itexture2D",              GLSL_TYPE_SPEC},
	{"itexture2DArray",         GLSL_TYPE_SPEC},
	{"utexture2D",              GLSL_TYPE_SPEC},
	{"utexture2DArray",         GLSL_TYPE_SPEC},
	{"texture2DRect",           GLSL_TYPE_SPEC},
	{"itexture2DRect",          GLSL_TYPE_SPEC},
	{"utexture2DRect",          GLSL_TYPE_SPEC},
	{"texture2DMS",             GLSL_TYPE_SPEC},
	{"itexture2DMS",            GLSL_TYPE_SPEC},
	{"utexture2DMS",            GLSL_TYPE_SPEC},
	{"texture2DMSArray",        GLSL_TYPE_SPEC},
	{"itexture2DMSArray",       GLSL_TYPE_SPEC},
	{"utexture2DMSArray",       GLSL_TYPE_SPEC},
	{"texture3D",               GLSL_TYPE_SPEC},
	{"itexture3D",              GLSL_TYPE_SPEC},
	{"utexture3D",              GLSL_TYPE_SPEC},
	{"textureCube",             GLSL_TYPE_SPEC},
	{"itextureCube",            GLSL_TYPE_SPEC},
	{"utextureCube",            GLSL_TYPE_SPEC},
	{"textureCubeArray",        GLSL_TYPE_SPEC},
	{"itextureCubeArray",       GLSL_TYPE_SPEC},
	{"utextureCubeArray",       GLSL_TYPE_SPEC},
	{"textureBuffer",           GLSL_TYPE_SPEC},
	{"itextureBuffer",          GLSL_TYPE_SPEC},
	{"utextureBuffer",          GLSL_TYPE_SPEC},
	{"sampler",                 GLSL_TYPE_SPEC},
	{"samplerShadow",           GLSL_TYPE_SPEC},
	{"subpassInput",            GLSL_TYPE_SPEC},
	{"isubpassInput",           GLSL_TYPE_SPEC},
	{"usubpassInput",           GLSL_TYPE_SPEC},
	{"subpassInputMS",          GLSL_TYPE_SPEC},
	{"isubpassInputMS",         GLSL_TYPE_SPEC},
	{"usubpassInputMS",         GLSL_TYPE_SPEC},

	//reserved
	{"common",          GLSL_RESERVED},
	{"partition",       GLSL_RESERVED},
	{"active",          GLSL_RESERVED},
	{"asm",             GLSL_RESERVED},
	{"class",           GLSL_RESERVED},
	{"union",           GLSL_RESERVED},
	{"enum",            GLSL_RESERVED},
	{"typedef",         GLSL_RESERVED},
	{"template",        GLSL_RESERVED},
	{"this",            GLSL_RESERVED},
	{"resource",        GLSL_RESERVED},
	{"goto",            GLSL_RESERVED},
	{"inline",          GLSL_RESERVED},
	{"noinline",        GLSL_RESERVED},
	{"public",          GLSL_RESERVED},
	{"static",          GLSL_RESERVED},
	{"extern",          GLSL_RESERVED},
	{"external",        GLSL_RESERVED},
	{"interface",       GLSL_RESERVED},
	{"long",            GLSL_RESERVED},
	{"short",           GLSL_RESERVED},
	{"half",            GLSL_RESERVED},
	{"fixed",           GLSL_RESERVED},
	{"unsigned",        GLSL_RESERVED},
	{"superp",          GLSL_RESERVED},
	{"input",           GLSL_RESERVED},
	{"output",          GLSL_RESERVED},
	{"hvec2",           GLSL_RESERVED},
	{"hvec3",           GLSL_RESERVED},
	{"hvec4",           GLSL_RESERVED},
	{"fvec2",           GLSL_RESERVED},
	{"fvec3",           GLSL_RESERVED},
	{"fvec4",           GLSL_RESERVED},
	{"filter",          GLSL_RESERVED},
	{"sizeof",          GLSL_RESERVED},
	{"cast",            GLSL_RESERVED},
	{"namespace",       GLSL_RESERVED},
	{"using",           GLSL_RESERVED},
	{"sampler3DRect",   GLSL_RESERVED},
};

// preprocessor directives in ruamoko and quakec
static directive_t glsl_directives[] = {
	{"include",     PRE_INCLUDE},
	{"define",      PRE_DEFINE},
	{"undef",       PRE_UNDEF},
	{"error",       PRE_ERROR},
	{"warning",     PRE_WARNING},
	{"notice",      PRE_NOTICE},
	{"pragma",      PRE_PRAGMA},
	{"line",        PRE_LINE},

	{"extension",   PRE_EXTENSION},
	{"version",     PRE_VERSION},
};

static directive_t *
glsl_directive (const char *token)
{
	static hashtab_t *directive_tab;

	if (!directive_tab) {
		directive_tab = Hash_NewTable (253, rua_directive_get_key, 0, 0, 0);
		for (size_t i = 0; i < ARRCOUNT(glsl_directives); i++) {
			Hash_Add (directive_tab, &glsl_directives[i]);
		}
	}
	directive_t *directive = Hash_Find (directive_tab, token);
	return directive;
}

static int
glsl_process_keyword (rua_val_t *lval, keyword_t *keyword, const char *token)
{
	if (keyword->value == GLSL_STRUCT) {
		lval->op = token[0];
	} else if (keyword->value == GLSL_TYPE_SPEC && !keyword->spec.type) {
		auto sym = new_symbol (token);
		sym = find_handle (sym, &type_int);
		keyword->spec.type = sym->type;
		lval->spec = keyword->spec;
	} else {
		lval->spec = keyword->spec;
	}
	return keyword->value;
}

static int
glsl_keyword_or_id (rua_val_t *lval, const char *token)
{
	static hashtab_t *glsl_keyword_tab;

	keyword_t  *keyword = 0;
	symbol_t   *sym;

	if (!glsl_keyword_tab) {
		size_t      i;

		glsl_keyword_tab = Hash_NewTable (253, rua_keyword_get_key, 0, 0, 0);

		for (i = 0; i < ARRCOUNT(glsl_keywords); i++)
			Hash_Add (glsl_keyword_tab, &glsl_keywords[i]);
	}
	keyword = Hash_Find (glsl_keyword_tab, token);
	if (keyword && keyword->value)
		return glsl_process_keyword (lval, keyword, token);
	if (token[0] == '@') {
		return '@';
	}
	sym = symtab_lookup (current_symtab, token);
	if (!sym)
		sym = new_symbol (token);
	lval->symbol = sym;
	if (sym->sy_type == sy_type) {
		lval->spec = (specifier_t) {
			.type = sym->type,
			.sym = sym,
		};
		return GLSL_TYPE_NAME;
	}
	return GLSL_IDENTIFIER;
}

static int
glsl_yyparse (FILE *in)
{
	rua_parser_t parser = {
		.parse = glsl_yypush_parse,
		.state = glsl_yypstate_new (),
		.directive = glsl_directive,
		.keyword_or_id = glsl_keyword_or_id,
	};
	int ret = rua_parse (in, &parser);
	glsl_yypstate_delete (parser.state);
	return ret;
}

int
glsl_parse_string (const char *str)
{
	rua_parser_t parser = {
		.parse = glsl_yypush_parse,
		.state = glsl_yypstate_new (),
		.directive = glsl_directive,
		.keyword_or_id = glsl_keyword_or_id,
	};
	int ret = rua_parse_string (str, &parser);
	glsl_yypstate_delete (parser.state);
	return ret;
}

language_t lang_glsl_comp = {
	.init = glsl_init_comp,
	.parse = glsl_yyparse,
};

language_t lang_glsl_vert = {
	.init = glsl_init_vert,
	.parse = glsl_yyparse,
};

language_t lang_glsl_tesc = {
	.init = glsl_init_tesc,
	.parse = glsl_yyparse,
};

language_t lang_glsl_tese = {
	.init = glsl_init_tese,
	.parse = glsl_yyparse,
};

language_t lang_glsl_geom = {
	.init = glsl_init_geom,
	.parse = glsl_yyparse,
};

language_t lang_glsl_frag = {
	.init = glsl_init_frag,
	.parse = glsl_yyparse,
};
