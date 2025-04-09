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
#include <stdlib.h>

#include <QF/hash.h>
#include <QF/sys.h>
#include <QF/va.h>

#define GLSL_YYDEBUG 1
#define GLSL_YYERROR_VERBOSE 1
#undef GLSL_YYERROR_VERBOSE

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/iface_block.h"
#include "tools/qfcc/include/image.h"
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

#define glsl_yytext qc_yyget_text (ctx->scanner)
char *qc_yyget_text (void *scanner);

static void
yyerror (YYLTYPE *yylloc, rua_ctx_t *ctx, const char *s)
{
#ifdef GLSL_YYERROR_VERBOSE
	error (0, "%s %s\n", glsl_yytext, s);
#else
	error (0, "%s before %s", s, glsl_yytext);
#endif
	exit(1);
}

static void __attribute__((used))
parse_error (rua_ctx_t *ctx)
{
	error (0, "parse error before %s", glsl_yytext);
}

#define PARSE_ERROR do { parse_error (scanner); YYERROR; } while (0)

#define YYLLOC_DEFAULT(Current, Rhs, N) RUA_LOC_DEFAULT(Current, Rhs, N)
#define YYLOCATION_PRINT rua_print_location

int yylex (YYSTYPE *yylval, YYLTYPE *yylloc);

%}

%code requires { #define glsl_yypstate rua_yypstate }

// these tokens are common between qc and glsl
%precedence LOW
%precedence ';' '{'
%nonassoc IFX
%nonassoc ELSE
%nonassoc CLASS_NOT_CATEGORY

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
%left           HADAMARD CROSS DOT OUTER WEDGE REGRESSIVE
%right	<op>	SIZEOF UNARY INCOP REVERSE STAR DUAL UNDUAL
%left			HYPERUNARY
%left			'.' '(' '['

%token	<expr>		VALUE STRING EBUFFER TOKEN
%token              TRUE FALSE
%token              ELLIPSIS
%token              RESERVED
// end of common tokens

%token	<symbol>	IDENTIFIER

%token				WHILE DO IF ELSE FOR BREAK CONTINUE
%token				RETURN
%token				SWITCH CASE DEFAULT
%token	<op>		STRUCT
%token	<spec>		TYPE_SPEC TYPE_NAME TYPE_QUAL VOID

%token        PRECISION
%token <string> INVARIANT SMOOTH FLAT NOPERSPECTIVE PRECISE
%token <string> HIGH_PRECISION MEDIUM_PRECISION LOW_PRECISION
%token <string> COHERENT VOLATILE RESTRICT READONLY WRITEONLY
%token <string> CENTROID PATCH SAMPLE
%token <spec> LAYOUT SHARED
%token <spec> CONST IN OUT INOUT UNIFORM BUFFER
%token <spec> DISCARD

%type <symbol>  variable_identifier new_identifier
%type <block>   block_declaration
%type <expr>    declaration constant_expression
%type <expr>    expression primary_exprsssion assignment_expression
%type <expr>    for_init_statement opt_cond opt_expr else line
%type <expr>    conditional_expression unary_expression postfix_expression
%type <expr>    function_call function_call_or_method function_call_generic
%type <mut_expr> function_call_header_with_parameters identifier_list
%type <mut_expr> struct_declarator struct_declarator_list
%type <mut_expr> struct_declaration struct_declaration_list
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
%type <mut_expr> new_block new_scope
%type <expr>    simple_statement expression_statement
%type <expr>    declaration_statement selection_statement
%type <expr>    switch_statement switch_statement_list case_label
%type <switch_block> switch_block
%type <expr>    iteration_statement jump_statement
%type <expr>    break_label continue_label
%type <mut_expr> initializer_list
%type <mut_expr> single_declaration init_declarator_list
%type <op>      unary_operator assignment_operator

%type <spec>    function_prototype function_declarator
%type <spec>    function_header function_header_with_parameters
%type <spec>    fully_specified_type type_specifier struct_specifier
%type <spec>    type_qualifier single_type_qualifier type_specifier_nonarray
%type <string>  precision_qualifier interpolation_qualifier invariant_qualifier
%type <string>  precise_qualifier memory_qualifier aux_storage_qualifier
%type <spec>    storage_qualifier layout_qualifier

%type <expr>    layout_qualifier_id
%type <mut_expr> layout_qualifier_id_list

%type <size>    array_size
%type <type>    array_specifier

%type <param>   parameter_declaration

%printer { fprintf (yyo, "%s", $$->name); } <symbol>
%printer { fprintf (yyo, "%s", ($$ && $$->type == ex_value) ? get_value_string ($$->value) : "<expr>"); } <expr>
%printer { fprintf (yyo, "%p", $<pointer>$); } <*>
%printer { fprintf (yyo, "<>"); } <>

%{

static switch_block_t *switch_block;
static const expr_t *break_label;
static const expr_t *continue_label;

static specifier_t
spec_merge (specifier_t spec, specifier_t new)
{
	if (spec.type && new.type) {
		if (!spec.multi_type) {
			error (0, "two or more data types in declaration specifiers");
			spec.multi_type = true;
		}
	}
	if (spec.storage && new.storage) {
		if (!spec.multi_store) {
			error (0, "multiple storage classes in declaration specifiers");
			spec.multi_store = true;
		}
	}
	if (!spec.type) {
		spec.type = new.type;
	}
	if (!spec.storage) {
		spec.storage = new.storage;
	}
	for (auto attr = &spec.attributes; *attr; attr = &(*attr)->next) {
		if (!(*attr)->next) {
			(*attr)->next = new.attributes;
			break;
		}
	}
	spec.sym = new.sym;
	spec.spec_bits |= new.spec_bits;
	return spec;
}

static param_t *
make_param (specifier_t spec)
{
	//FIXME should not be sc_global
	if (spec.storage == sc_global) {
		spec.storage = sc_param;
	}
	auto interface = iftype_from_sc (spec.storage);
	if (interface == iface_in) {
		spec.storage = sc_in;
	} else if (interface == iface_out) {
		spec.storage = sc_out;
	}

	param_t *param;
	if (spec.type_expr) {
		param = new_generic_param (spec.type_expr, spec.sym->name);
	} else {
		if (spec.sym) {
			param = new_param (nullptr, spec.type, spec.sym->name);
		} else {
			param = new_param (nullptr, spec.type, nullptr);
		}
	}
	if (spec.is_const) {
		if (spec.storage == sc_out) {
			error (0, "cannot use const with @out");
		} else if (spec.storage == sc_inout) {
			error (0, "cannot use const with @inout");
		} else {
			if (spec.storage != sc_in && spec.storage != sc_param) {
				internal_error (0, "unexpected parameter storage: %d",
								spec.storage);
			}
		}
		param->qual = pq_const;
	} else {
		if (spec.storage == sc_out) {
			param->qual = pq_out;
		} else if (spec.storage == sc_inout) {
			param->qual = pq_inout;
		} else {
			if (spec.storage != sc_in && spec.storage != sc_param) {
				internal_error (0, "unexpected parameter storage: %d",
								spec.storage);
			}
			param->qual = pq_in;
		}
	}
	return param;
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
	| declaration { if ($1) expr_process ($1, ctx); }
	| ';'
	;

function_definition
	: function_prototype
		{
			$<symtab>$ = current_symtab;
			auto spec = $1;
			spec.sym->params = spec.params;
			spec.is_overload = strcmp (spec.sym->name, "main") != 0;
			spec.type = find_type (parse_params (spec.type, spec.params));
			spec.sym = function_symbol (spec, ctx);
			current_func = begin_function (spec, nullptr, current_symtab, ctx);
			current_symtab = current_func->locals;
			current_storage = sc_local;
			$1 = spec;
		}
	  compound_statement_no_new_scope
		{
			auto spec = $1;
			build_code_function (spec, nullptr, (expr_t *) $3, ctx);
			current_symtab = $<symtab>2;
			current_storage = sc_global;//FIXME
			current_func = nullptr;
		}
	;

variable_identifier
	: IDENTIFIER
	;

new_identifier
	: IDENTIFIER			{ $$ = new_symbol ($1->name); }
	;

primary_exprsssion
	: variable_identifier	{ $$ = new_symbol_expr ($1); }
	| VALUE
	| STRING
	| TRUE					{ $$ = new_bool_expr (true); }
	| FALSE					{ $$ = new_bool_expr (false); }
	| '(' expression ')'	{ $$ = paren_expr ($2); }
	;

postfix_expression
	: primary_exprsssion
	| postfix_expression '[' integer_expression ']'
		{
			$$ = new_array_expr ($1, $3);
		}
	| function_call
	| postfix_expression '.' IDENTIFIER/*FIELD_SELECTION*/
		{
			auto sym = new_symbol_expr ($3);
			$$ = new_field_expr ($1, sym);
		}
	| postfix_expression INCOP			{ $$ = new_incop_expr ($2, $1, 1); }
	;

integer_expression
	: expression
	;

function_call
	: function_call_or_method
	;

function_call_or_method
	: function_call_generic
		{
			scoped_src_loc ($1);
			// pull apart the args+func list
			// args are in reverse order, so f(a,b,c) <- c,b,a,f
			auto list = &$1->list;
			int count = list_count (list);
			const expr_t *exprs[count];
			list_scatter (list, exprs);
			auto func = exprs[count - 1];
			auto args = new_list_expr (nullptr);
			list_gather (&args->list, exprs, count - 1);
			$$ = new_call_expr (func, args, nullptr);
		}
	;

function_call_generic
	: function_call_header_with_parameters ')' { $$ = $1; }
	| function_call_header_no_parameters ')'
		{
			$$ = new_list_expr ($1);
		}
	;

function_call_header_no_parameters
	: function_call_header VOID
	| function_call_header
	;

function_call_header_with_parameters
	: function_call_header assignment_expression
		{
			scoped_src_loc ($1);
			auto list = new_list_expr ($1);
			$$ = expr_prepend_expr (list, $2);
		}
	| function_call_header_with_parameters ',' assignment_expression
		{
			auto list = $1;
			$$ = expr_prepend_expr (list, $3);
		}
	;

function_call_header
	: function_identifier '('
	;

function_identifier
	: type_specifier					{ $$ = new_type_expr ($1.type); }
	| postfix_expression
	;

unary_expression
	: postfix_expression
	| INCOP unary_expression			{ $$ = new_incop_expr ($1, $2, 0); }
	| unary_operator unary_expression	{ $$ = new_unary_expr ($1, $2); }
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
			$$ = new_binary_expr ('*', $1, $3);
		}
	| multiplicative_expression '/' unary_expression
		{
			$$ = new_binary_expr ('/', $1, $3);
		}
	| multiplicative_expression '%' unary_expression
		{
			$$ = new_binary_expr ('%', $1, $3);
		}
	;

additive_expression
	: multiplicative_expression
	| additive_expression '+' multiplicative_expression
		{
			$$ = new_binary_expr ('+', $1, $3);
		}
	| additive_expression '-' multiplicative_expression
		{
			$$ = new_binary_expr ('-', $1, $3);
		}
	;

shift_expression
	: additive_expression
	| shift_expression SHL additive_expression
		{
			$$ = new_binary_expr (QC_SHL, $1, $3);
		}
	| shift_expression SHR additive_expression
		{
			$$ = new_binary_expr (QC_SHR, $1, $3);
		}
	;

relational_expression
	: shift_expression
	| relational_expression LT shift_expression
		{
			$$ = new_binary_expr (QC_LT, $1, $3);
		}
	| relational_expression GT shift_expression
		{
			$$ = new_binary_expr (QC_GT, $1, $3);
		}
	| relational_expression LE shift_expression
		{
			$$ = new_binary_expr (QC_LE, $1, $3);
		}
	| relational_expression GE shift_expression
		{
			$$ = new_binary_expr (QC_GE, $1, $3);
		}
	;

equality_expression
	: relational_expression
	| equality_expression EQ relational_expression
		{
			$$ = new_binary_expr (QC_EQ, $1, $3);
		}
	| relational_expression NE relational_expression
		{
			$$ = new_binary_expr (QC_NE, $1, $3);
		}
	;

and_expression
	: equality_expression
	| and_expression '&' equality_expression
		{
			$$ = new_binary_expr ('&', $1, $3);
		}
	;

exclusive_or_expression
	: and_expression
	| exclusive_or_expression '^' and_expression
		{
			$$ = new_binary_expr ('^', $1, $3);
		}
	;

inclusive_or_expression
	: exclusive_or_expression
	| inclusive_or_expression '|' exclusive_or_expression
		{
			$$ = new_binary_expr ('|', $1, $3);
		}
	;

logical_and_expression
	: inclusive_or_expression
	| logical_and_expression AND inclusive_or_expression
		{
			$$ = new_binary_expr (QC_AND, $1, $3);
		}
	;

logical_xor_expression
	: logical_and_expression
	| logical_xor_expression XOR logical_and_expression
		{
			$$ = new_binary_expr (QC_XOR, $1, $3);
		}
	;

logical_or_expression
	: logical_xor_expression
	| logical_or_expression OR logical_xor_expression
		{
			$$ = new_binary_expr (QC_OR, $1, $3);
		}
	;

conditional_expression
	: logical_or_expression
	| logical_or_expression '?' expression ':' assignment_expression
		{
			$$ = new_cond_expr ($1, $3, $5);
		}
	;

assignment_expression
	: conditional_expression
	| unary_expression assignment_operator assignment_expression
		{
			auto expr = $3;
			if ($2 != '=') {
				expr = paren_expr (expr);
				expr = new_binary_expr ($2, $1, expr);
			}
			$$ = new_assign_expr ($1, expr);
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
	: function_prototype ';'	{ $$ = nullptr; }
	| init_declarator_list ';'	{ $$ = $1; }
	| PRECISION precision_qualifier type_specifier ';'
		{
			notice (0, "PRECISION precision_qualifier");
			$$ = nullptr;
		}
	| type_qualifier block_declaration ';'
		{
			if (current_symtab != pr.symtab) {
				error (0, "blocks must be declared globally");
			} else {
				auto spec = $type_qualifier;
				auto block = $block_declaration;
				declare_block_instance (spec, block, nullptr, ctx);
			}
			$$ = nullptr;
		}
	| type_qualifier block_declaration new_identifier ';'
		{
			if (current_symtab != pr.symtab) {
				error (0, "blocks must be declared globally");
			} else {
				auto spec = $type_qualifier;
				auto block = $block_declaration;
				auto instance_name = $new_identifier;
				declare_block_instance (spec, block, instance_name, ctx);
			}
			$$ = nullptr;
		}
	| type_qualifier block_declaration new_identifier array_specifier ';'
		{
			if (current_symtab != pr.symtab) {
				error (0, "blocks must be declared globally");
			} else {
				auto spec = $type_qualifier;
				auto block = $block_declaration;
				auto instance_name = $new_identifier;
				instance_name->type = $array_specifier;
				declare_block_instance (spec, block, instance_name, ctx);
			}
			$$ = nullptr;
		}
	| type_qualifier ';'
		{
			glsl_parse_declaration ($type_qualifier, nullptr,
									nullptr, current_symtab, nullptr, ctx);
			$$ = nullptr;
		}
	| type_qualifier new_identifier ';'
		{
			auto decl = new_decl_expr ($type_qualifier, current_symtab);
			$$ = append_decl (decl, $new_identifier, nullptr);
		}
	| type_qualifier new_identifier identifier_list ';'
		{
			auto decl = new_decl_expr ($type_qualifier, current_symtab);
			append_decl (decl, $new_identifier, nullptr);
			$$ = append_decl_list (decl, $identifier_list);
		}
	;

block_declaration
	: IDENTIFIER '{'
		{
			auto sym = $IDENTIFIER;
			auto block = create_block (sym);
			current_symtab = block->members;
			$<block>$ = block;
		}
	  struct_declaration_list[decls] '}'
		{
			auto block = $<block>3;
			current_symtab = block->members->parent;
			block->member_decls = $decls;
			$$ = block;
		}
	;

identifier_list
	: ',' new_identifier
		{
			auto expr = new_symbol_expr ($2);
			$$ = new_list_expr (expr);
		}
	| identifier_list ',' new_identifier
		{
			auto expr = new_symbol_expr ($3);
			$$ = expr_append_expr ($1, expr);
		}
	;

function_prototype
	: function_declarator ')'
		{
			auto spec = $1;
			spec.sym->type = parse_params (0, spec.params);
			$$ = spec;
		}
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
	: fully_specified_type new_identifier '('
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

parameter_declaration
	: type_qualifier type_specifier new_identifier
		{
			auto spec = spec_merge ($1, $2);
			spec.sym = $3;
			$$ = make_param (spec);
		}
	| type_qualifier type_specifier new_identifier array_specifier
		{
			auto spec = spec_merge ($1, $2);
			spec.type = append_type ($1.type, $4);
			spec.sym = $3;
			$$ = make_param (spec);
		}
	| type_qualifier type_specifier
		{
			auto spec = spec_merge ($1, $2);
			$$ = make_param (spec);
		}
	| type_specifier new_identifier
		{
			auto spec = $1;
			spec.sym = $2;
			$$ = make_param (spec);
		}
	| type_specifier new_identifier array_specifier
		{
			auto spec = $1;
			spec.type = append_type ($1.type, $3);
			spec.sym = $2;
			$$ = make_param (spec);
		}
	| type_specifier
		{
			auto spec = $1;
			$$ = make_param (spec);
		}
	;

init_declarator_list
	: single_declaration
	| init_declarator_list ',' new_identifier
		{
			auto decl = $1;
			$$ = append_decl (decl, $new_identifier, nullptr);
		}
	| init_declarator_list ',' new_identifier array_specifier
		{
			auto decl = $1;
			$new_identifier->type = $array_specifier;
			$$ = append_decl (decl, $new_identifier, nullptr);
		}
	| init_declarator_list ',' new_identifier array_specifier '=' initializer
		{
			auto decl = $1;
			$new_identifier->type = $array_specifier;
			$$ = append_decl (decl, $new_identifier, $initializer);
		}
	| init_declarator_list ',' new_identifier '=' initializer
		{
			auto decl = $1;
			$$ = append_decl (decl, $new_identifier, $initializer);
		}
	;

single_declaration
	: fully_specified_type
		{
			$$ = new_decl_expr ($fully_specified_type, current_symtab);
		}
	| fully_specified_type new_identifier
		{
			auto decl = new_decl_expr ($fully_specified_type, current_symtab);
			$$ = append_decl (decl, $new_identifier, nullptr);
		}
	| fully_specified_type new_identifier array_specifier
		{
			auto spec = $fully_specified_type;
			spec.type = append_type ($array_specifier, spec.type);
			spec.type = find_type (spec.type);
			auto decl = new_decl_expr (spec, current_symtab);
			$$ = append_decl (decl, $new_identifier, nullptr);
		}
	| fully_specified_type new_identifier array_specifier '=' initializer
		{
			auto spec = $fully_specified_type;
			spec.type = append_type ($array_specifier, spec.type);
			spec.type = find_type (spec.type);
			auto decl = new_decl_expr (spec, current_symtab);
			$$ = append_decl (decl, $new_identifier, $initializer);
		}
	| fully_specified_type new_identifier '=' initializer
		{
			auto decl = new_decl_expr ($fully_specified_type, current_symtab);
			$$ = append_decl (decl, $new_identifier, $initializer);
		}
	;

fully_specified_type
	: type_specifier
	| type_qualifier type_specifier		{ $$ = spec_merge ($1, $2); }
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
	: LAYOUT '(' layout_qualifier_id_list ')'
		{
			auto attr = new_attribute ("layout", $3, ctx);
			$$ = (specifier_t) { .attributes = attr };
		}
	;

layout_qualifier_id_list
	: layout_qualifier_id
		{
			$$ = new_list_expr ($1);
		}
	| layout_qualifier_id_list ',' layout_qualifier_id
		{
			$$ = expr_append_expr ($1, $3);
		}
	;

layout_qualifier_id
	: IDENTIFIER					{ $$ = new_string_expr ($1->name); }
	| IDENTIFIER '=' constant_expression
		{
			auto id = new_string_expr ($1->name);
			$$ = new_binary_expr ('=', id, $3);
		}
	| SHARED
		{
			$$ = new_string_expr ("shared");
		}
	;

precise_qualifier
	: PRECISE
	;

type_qualifier
	: single_type_qualifier
	| type_qualifier single_type_qualifier
		{
			$<spec>$ = spec_merge ($1, $2);
		}
	;

single_type_qualifier
	: storage_qualifier
	| aux_storage_qualifier
		{
			auto attr = new_attribute ($1, nullptr, ctx);
			$$ = (specifier_t) { .attributes = attr };
		}
	| layout_qualifier
	| precision_qualifier
		{
			auto attr = new_attribute ($1, nullptr, ctx);
			$$ = (specifier_t) { .attributes = attr };
		}
	| interpolation_qualifier
		{
			auto attr = new_attribute ($1, nullptr, ctx);
			$$ = (specifier_t) { .attributes = attr };
		}
	| invariant_qualifier
		{
			auto attr = new_attribute ($1, nullptr, ctx);
			$$ = (specifier_t) { .attributes = attr };
		}
	| precise_qualifier
		{
			auto attr = new_attribute ($1, nullptr, ctx);
			$$ = (specifier_t) { .attributes = attr };
		}
	| memory_qualifier
		{
			auto attr = new_attribute ($1, nullptr, ctx);
			$$ = (specifier_t) { .attributes = attr };
		}
	;

storage_qualifier
	: CONST
	| IN
	| OUT
	| INOUT
	| UNIFORM
	| BUFFER
	| SHARED
	;

aux_storage_qualifier
	: CENTROID
	| PATCH
	| SAMPLE
	;

memory_qualifier
	: COHERENT
	| VOLATILE
	| RESTRICT
	| READONLY
	| WRITEONLY
	;

type_specifier
	: type_specifier_nonarray
	| type_specifier_nonarray array_specifier
		{
			auto spec = $1;
			auto type = append_type ($2, spec.type);
			spec.type = find_type (type);
			$$ = spec;
		}
	;

array_specifier
	: array_size				{ $$ = array_type (nullptr, $1); }
	| array_specifier array_size
		{
			$$ = append_type ($1, array_type (nullptr, $2));
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
	: VOID							{ $$ = $1; $$.storage = current_storage; }
	| TYPE_SPEC						{ $$ = $1; $$.storage = current_storage; }
	| struct_specifier				{ $$ = $1; $$.storage = current_storage; }
	| TYPE_NAME						{ $$ = $1; $$.storage = current_storage; }
	;

precision_qualifier
	: HIGH_PRECISION
	| MEDIUM_PRECISION
	| LOW_PRECISION
	;

struct_specifier
	: STRUCT IDENTIFIER '{'
		{
			if (current_symtab->type == stab_struct
				|| current_symtab->type == stab_block) {
				error (0, "nested struct declaration");
			}
			int op = 's';
			current_symtab = start_struct (&op, $2, current_symtab);
		}
	  struct_declaration_list[decls] '}'
		{
			auto symtab = current_symtab;
			current_symtab = symtab->parent;

			struct_process (symtab, $decls, ctx);
			auto sym = build_struct ('s', $2, symtab, nullptr, 0);
			if (!sym->table) {
				symtab_addsymbol (current_symtab, sym);
			}
			auto s = $2;
			s->sy_type = sy_type;
			s->type = find_type (alias_type (sym->type, sym->type, s->name));
			symtab_addsymbol (current_symtab, s);
			$$ = (specifier_t) { .type = s->type };
		}
	| STRUCT '{'
		{
			if (current_symtab->type == stab_struct) {
				error (0, "nested struct declaration");
			}
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
			$$ = (specifier_t) { .type = sym->type };
		}
	;

struct_declaration_list
	: struct_declaration[decl]			{ $$ = new_list_expr ($decl); }
	| struct_declaration_list[list] struct_declaration[decl]
		{
			$$ = expr_append_expr ($list, $decl);
		}
	;

struct_declaration
	: type_specifier[spec]
		{
			$<mut_expr>$ = new_decl_expr ($spec, nullptr);
		}
	  struct_declarator_list[list] ';'	{ $$ = $list; }
	| type_qualifier[qual] type_specifier[spec]
		{
			auto spec = spec_merge ($qual, $spec);
			$<mut_expr>$ = new_decl_expr (spec, nullptr);
		}
	  struct_declarator_list[list] ';'	{ $$ = $list; }
	;

struct_declarator_list
	: struct_declarator
	| struct_declarator_list[list] ','	{ $<mut_expr>$ = $list; }
	  struct_declarator[decl]			{ $$ = $decl; }
	;

struct_declarator
	: new_identifier
		{
			auto decl = $<mut_expr>0;
			append_decl (decl, $new_identifier, nullptr);
			$$ = decl;
		}
	| new_identifier array_specifier
		{
			auto decl = $<mut_expr>0;
			auto sym = $new_identifier;
			sym->type = $array_specifier;
			append_decl (decl, $new_identifier, nullptr);
			$$ = decl;
		}
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
	: declaration
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
	: '{' '}'										{ $$ = nullptr; }
	| '{' new_scope statement_list '}' end_scope	{ $$ = $3; }
	;

statement_no_new_scope
	: compound_statement_no_new_scope
	| simple_statement
	;

compound_statement_no_new_scope
	: '{' '}'							{ $$ = nullptr; }
	| '{' new_block statement_list '}'	{ $$ = $3; }
	;

new_block
	: /* empty */
		{
			auto block = new_block_expr (nullptr);
			block->block.scope = current_symtab;
			$$ = block;
		}
	;

new_scope
	: /* empty */
		{
			auto block = new_block_expr (nullptr);
			block->block.scope = new_symtab (current_symtab, stab_local);
			current_symtab = block->block.scope;
			$$ = block;
		}
	;

end_scope
	: /* empty */
		{
			current_symtab = current_symtab->parent;
		}
	;

statement_list
	: statement
		{
			auto list = $<mut_expr>0;
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
	| expression ';'			{ $$ = $expression; }
	;

selection_statement
	: IF line '(' expression[test] ')' statement[true] %prec IFX
		{
			scoped_src_loc ($line);
			$$ = new_select_expr (false, $test, $true, nullptr, nullptr);
		}
	| IF line '(' expression[test] ')' statement[true] else statement[false]
		{
			scoped_src_loc ($line);
			$$ = new_select_expr (false, $test, $true, $else, $false);
		}
	;

line
	: /* empty */
		{
			// this is only to get the the file and line number info
			$$ = new_nil_expr ();
		}
	;

else
	: ELSE line					{ $$ = $2; }
	;

condition
	: expression
	| fully_specified_type new_identifier '=' initializer
		{
			auto decl = new_decl_expr ($fully_specified_type, current_symtab);
			$$ = append_decl (decl, $new_identifier, $initializer);
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
	| new_block statement_list	{ $$ = $2; }
	;

case_label
	: CASE expression ':'	{ $$ = case_label_expr (switch_block, $2); }
	| DEFAULT ':'			{ $$ = case_label_expr (switch_block, nullptr); }
	;

iteration_statement
	: WHILE new_scope break_label continue_label '(' condition ')'
			statement_no_new_scope
		{
			$$ = new_loop_expr (false, false, $condition,
								$statement_no_new_scope,
								continue_label, nullptr, break_label);
			break_label = $break_label;
			continue_label = $continue_label;
			current_symtab = $new_scope->block.scope->parent;
		}
	| DO break_label continue_label statement WHILE '(' expression ')' ';'
		{
			$$ = new_loop_expr (false, true, $expression, $statement,
								continue_label, nullptr, break_label);
			break_label = $break_label;
			continue_label = $continue_label;
		}
	| FOR new_scope break_label continue_label
			'(' for_init_statement[init] opt_cond[test] ';' opt_expr[cont] ')'
			statement_no_new_scope[body] end_scope
		{
			auto test = $test;
			if (!test) {
				test = new_bool_expr (true);
			}
			auto loop = new_loop_expr (false, false, test, $body,
									   continue_label, $cont, break_label);
			auto block = $new_scope;
			append_expr (block, $init);
			$$ = append_expr (block, loop);
			break_label = $break_label;
			continue_label = $continue_label;
		}
	;

for_init_statement
	: expression_statement
	| declaration_statement
	;

opt_cond
	: condition
	| /* emtpy */	{ $$ = nullptr; }
	;

opt_expr
	: expression
	| /* emtpy */	{ $$ = nullptr; }
	;

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
				error (nullptr, "break outside of loop or switch");
			}
		}
	| RETURN ';'				{ $$ = new_return_expr (nullptr); }
	| RETURN expression ';'		{ $$ = new_return_expr ($2); }
	| DISCARD ';'
		{
			auto func = new_name_expr ("__discard");
			$$ = new_call_expr (func, nullptr, nullptr);
		}
	;

%%

#define GLSL_INTERFACE(bt) .spec = { .storage = sc_from_iftype (bt) }
static keyword_t glsl_keywords[] = {
	{"const",           GLSL_CONST,     .spec = { .is_const = true }},
	{"uniform",         GLSL_UNIFORM,   GLSL_INTERFACE (iface_uniform)},
	{"buffer",          GLSL_BUFFER,    GLSL_INTERFACE (iface_buffer)},
	{"shared",          GLSL_SHARED,    GLSL_INTERFACE (iface_shared)},
	// in and out are both parameter qualifiers (sc_in and sc_out) and
	// interface types (iface_in and iface_out). Assume interface type
	// here.
	{"in",              GLSL_IN,        GLSL_INTERFACE (iface_in )},
	{"out",             GLSL_OUT,       GLSL_INTERFACE (iface_out )},
	{"inout",           GLSL_INOUT,     .spec = { .storage = sc_inout }},
	{"attribute",       GLSL_RESERVED},
	{"varying",         GLSL_RESERVED},
	{"coherent",        GLSL_COHERENT,		.use_name = true},
	{"volatile",        GLSL_VOLATILE,		.use_name = true},
	{"restrict",        GLSL_RESTRICT,		.use_name = true},
	{"readonly",        GLSL_READONLY,		.use_name = true},
	{"writeonly",       GLSL_WRITEONLY,		.use_name = true},
	{"atomic_uint",     GLSL_RESERVED},
	{"layout",          GLSL_LAYOUT},
	{"centroid",        GLSL_CENTROID,		.use_name = true},
	{"flat",            GLSL_FLAT,          .use_name = true},
	{"smooth",          GLSL_SMOOTH,        .use_name = true},
	{"noperspective",   GLSL_NOPERSPECTIVE, .use_name = true},
	{"patch",           GLSL_PATCH,			.use_name = true},
	{"sample",          GLSL_SAMPLE,		.use_name = true},
	{"invariant",       GLSL_INVARIANT,     .use_name = true},
	{"precise",         GLSL_PRECISE,       .use_name = true},
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
	{"int",             GLSL_TYPE_SPEC, .spec = {.type = &type_int}},
	{"void",            GLSL_VOID,      .spec = {.type = &type_void}},
	{"bool",            GLSL_TYPE_SPEC, .spec = {.type = &type_bool}},
	{"true",            GLSL_TRUE},
	{"false",           GLSL_FALSE},
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
	{"uvec2",           GLSL_TYPE_SPEC, .spec = {.type = &type_uvec2}},
	{"uvec3",           GLSL_TYPE_SPEC, .spec = {.type = &type_uvec3}},
	{"uvec4",           GLSL_TYPE_SPEC, .spec = {.type = &type_uvec4}},
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

	{"lowp",                    GLSL_LOW_PRECISION,     .use_name = true},
	{"mediump",                 GLSL_MEDIUM_PRECISION,  .use_name = true},
	{"highp",                   GLSL_HIGH_PRECISION,    .use_name = true},
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
	{"sampler",             GLSL_TYPE_SPEC, .spec = {.type = &type_sampler}},
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
	{"bug",         PRE_BUG},
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

typedef struct {
	const char *substr;
	int         val;
	const type_t *type;
	const type_t *htype;
} image_sub_t;

static image_sub_t * __attribute__((pure))
find_image_sub (image_sub_t *sub, const char *str)
{
	for (; sub->substr; sub++) {
		if (strncmp (sub->substr, str, strlen(sub->substr)) == 0) {
			return sub;
		}
	}
	return nullptr;
}

static const type_t *
glsl_parse_image (const char *token, rua_ctx_t *ctx)
{
	static image_sub_t image_sub_type[] = {
		{ .substr = "sampler",  .val = 1, .type = &type_float,
		  .htype = &type_sampled_image },
		{ .substr = "image",    .val = 2, .type = &type_float,
		  .htype = &type_image },
		{ .substr = "texture",  .val = 1, .type = &type_float,
		  .htype = &type_image },
		{ .substr = "isampler", .val = 1, .type = &type_int,
		  .htype = &type_sampled_image },
		{ .substr = "iimage",   .val = 2, .type = &type_int,
		  .htype = &type_image },
		{ .substr = "itexture", .val = 1, .type = &type_int,
		  .htype = &type_image },
		{ .substr = "usampler", .val = 1, .type = &type_uint,
		  .htype = &type_sampled_image },
		{ .substr = "uimage",   .val = 2, .type = &type_uint,
		  .htype = &type_image },
		{ .substr = "utexture", .val = 1, .type = &type_uint,
		  .htype = &type_image },
		// subpassInput is dimension in spir-v
		{ .substr = "i",        .val = 2, .type = &type_int,
		  .htype = &type_image },
		{ .substr = "u",        .val = 2, .type = &type_uint,
		  .htype = &type_image },
		{ .substr = "",         .val = 2, .type = &type_float,
		  .htype = &type_image },
		{}
	};
	static image_sub_t image_dim[] = {
		{ "2DRect",       img_rect        },	// so 2D doesn't falsly match
		{ "1D",           img_1d          },
		{ "2D",           img_2d          },
		{ "3D",           img_3d          },
		{ "Cube",         img_cube        },
		{ "Buffer",       img_buffer      },
		{ "subpassInput", img_subpassdata },
		{}
	};
	static image_sub_t image_ms[] = {
		{ "MS", true  },
		{ "",   false },
		{}
	};
	static image_sub_t image_array[] = {
		{ "Array", true  },
		{ "",      false },
		{}
	};
	static image_sub_t image_shadow[] = {
		{ "Shadow", true  },
		{ "",       false },
		{}
	};

	int offset = 0;
	// always succeeds
	auto type = *find_image_sub (image_sub_type, token + offset);
	offset += strlen (type.substr);
	auto dim = find_image_sub (image_dim, token + offset);
	if (!dim) {
		goto invalid;
	}
	offset += strlen (dim->substr);
	if (!is_float (type.type)) {
		type.substr++;	// skip over type char
	}
	if (dim->val == img_subpassdata) {
		type.substr = dim->substr;
	}
	auto ms = *find_image_sub (image_ms, token + offset);
	offset += strlen (ms.substr);
	auto array = *find_image_sub (image_array, token + offset);
	offset += strlen (array.substr);

	image_t image = {
		.sample_type = type.type,
		.dim = dim->val,
		.depth = 0,		//set below for shadow samplers
		.arrayed = array.val,
		.multisample = ms.val,
		.sampled = type.val,
		.format = 0,	//unknown (comes from layout)
	};
	if (type.val == 1) {	// sampler
		auto shad = *find_image_sub (image_shadow, token + offset);
		offset += strlen (shad.substr);
		image.depth = shad.val;
	}
	if (token[offset]) {
		goto invalid;
	}

	return create_image_type (&image, type.htype);
invalid:
	internal_error (0, "invalid image type: %s", token);
}

static int
glsl_process_keyword (rua_val_t *lval, keyword_t *keyword, const char *token,
					  rua_ctx_t *ctx)
{
	if (keyword->value == GLSL_STRUCT) {
		lval->op = token[0];
	} else if (keyword->value == GLSL_TYPE_SPEC && !keyword->spec.type) {
		keyword->spec.type = glsl_parse_image (token, ctx);
		lval->spec = keyword->spec;
	} else if (keyword->use_name) {
		lval->string = keyword->name;
	} else {
		lval->spec = keyword->spec;
	}
	return keyword->value;
}

static int
glsl_keyword_or_id (rua_val_t *lval, const char *token, rua_ctx_t *ctx)
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
		return glsl_process_keyword (lval, keyword, token, ctx);
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
glsl_yyparse (FILE *in, rua_ctx_t *ctx)
{
	rua_parser_t parser = {
		.parse = glsl_yypush_parse,
		.state = glsl_yypstate_new (),
		.directive = glsl_directive,
		.keyword_or_id = glsl_keyword_or_id,
	};
	int ret = rua_parse (in, &parser, ctx);
	glsl_yypstate_delete (parser.state);
	return ret;
}

int
glsl_parse_string (const char *str, rua_ctx_t *ctx)
{
	rua_parser_t parser = {
		.parse = glsl_yypush_parse,
		.state = glsl_yypstate_new (),
		.directive = glsl_directive,
		.keyword_or_id = glsl_keyword_or_id,
	};
	auto glsl_ctx = *ctx;
	int ret = rua_parse_string (str, &parser, &glsl_ctx);
	glsl_yypstate_delete (parser.state);
	return ret;
}

static const char *glsl_behaviors[] = {
	"disable",
	"enable",
	"require",
	"warn",
};
#define num_behaviors (sizeof (glsl_behaviors) / sizeof (glsl_behaviors[0]))

typedef struct {
	const char *name;
	void      (*set_behavior) (int behavior, void *scanner);
} glsl_ext_t;

static glsl_ext_t glsl_extensions[] = {
	{"GL_EXT_multiview", glsl_multiview },
	{"GL_GOOGLE_include_directive", glsl_include },
};
#define num_extensions (sizeof (glsl_extensions) / sizeof (glsl_extensions[0]))

static int
extension_cmp (const void *_a, const void *_b)
{
	auto a = (const glsl_ext_t *) _a;
	auto b = (const glsl_ext_t *) _b;
	return strcmp (a->name, b->name);
}

static int
behavior_cmp (const void *_a, const void *_b)
{
	auto a = (const char **) _a;
	auto b = (const char **) _b;
	return strcmp (*a, *b);
}

static void
glsl_extension (const char *name, const char *value, rua_ctx_t *ctx)
{
	if (ctx->language->initialized) {
		error (0, "extensions directives must occur before any "
			   "non-preprocessor tokens");
		return;
	}
	const char **res;
	res = bsearch (&value, glsl_behaviors, num_behaviors, sizeof (char *),
				   behavior_cmp);
	if (!res) {
		error (0, "invalid behavior: %s", value);
		return;
	}
	int behavior = res - glsl_behaviors;
	if (strcmp (name, "all") == 0) {
		if (behavior != 0 && behavior != 3) {
			error (0, "behavior must be 'warn' or 'disable' for 'all'");
			return;
		}
		for (size_t i = 0; i < num_extensions; i++) {
			glsl_extensions[i].set_behavior (behavior, ctx);
		}
	} else {
		glsl_ext_t  key = { .name = name };
		glsl_ext_t *ext;
		ext = bsearch (&key, glsl_extensions, num_extensions,
					   sizeof (glsl_ext_t), extension_cmp);
		if (ext) {
			ext->set_behavior (behavior, ctx);
		} else {
			if (behavior == 2) {
				error (0, "unknown extension '%s'", name);
			} else {
				warning (0, "unknown extension '%s'", name);
			}
		}
	}
}

static void
glsl_version (int version, const char *profile, rua_ctx_t *ctx)
{
	if (!profile || strcmp (profile, "core") == 0) {
		// ok
	} else if (strcmp (profile, "es") == 0
			   || strcmp (profile, "compatibility") == 0) {
		error (0, "profile '%s' not supported", profile);
	} else {
		error (0, "bad profile name '%s': use es, core or compatibility",
			   profile);
	}
	if (version != 450 && version != 460) {
		error (0, "version not supported");
	}
}

language_t lang_glsl_comp = {
	.always_overload = true,
	.short_circuit = false,
	.default_float = true,
	.init = glsl_init_comp,
	.parse = glsl_yyparse,
	.finish = glsl_finish_comp,
	.extension = glsl_extension,
	.version = glsl_version,
	.on_include = glsl_on_include,
	.parse_declaration = glsl_parse_declaration,
	.field_attributes = glsl_field_attributes,
	.sublanguage = &glsl_comp_sublanguage,
};

language_t lang_glsl_vert = {
	.always_overload = true,
	.short_circuit = false,
	.default_float = true,
	.init = glsl_init_vert,
	.parse = glsl_yyparse,
	.extension = glsl_extension,
	.version = glsl_version,
	.on_include = glsl_on_include,
	.parse_declaration = glsl_parse_declaration,
	.field_attributes = glsl_field_attributes,
	.sublanguage = &glsl_vert_sublanguage,
};

language_t lang_glsl_tesc = {
	.always_overload = true,
	.short_circuit = false,
	.default_float = true,
	.init = glsl_init_tesc,
	.parse = glsl_yyparse,
	.extension = glsl_extension,
	.version = glsl_version,
	.on_include = glsl_on_include,
	.parse_declaration = glsl_parse_declaration,
	.field_attributes = glsl_field_attributes,
	.sublanguage = &glsl_tesc_sublanguage,
};

language_t lang_glsl_tese = {
	.always_overload = true,
	.short_circuit = false,
	.default_float = true,
	.init = glsl_init_tese,
	.parse = glsl_yyparse,
	.extension = glsl_extension,
	.version = glsl_version,
	.on_include = glsl_on_include,
	.parse_declaration = glsl_parse_declaration,
	.field_attributes = glsl_field_attributes,
	.sublanguage = &glsl_tese_sublanguage,
};

language_t lang_glsl_geom = {
	.always_overload = true,
	.short_circuit = false,
	.default_float = true,
	.init = glsl_init_geom,
	.parse = glsl_yyparse,
	.finish = glsl_finish_geom,
	.extension = glsl_extension,
	.version = glsl_version,
	.on_include = glsl_on_include,
	.parse_declaration = glsl_parse_declaration,
	.field_attributes = glsl_field_attributes,
	.sublanguage = &glsl_geom_sublanguage,
};

language_t lang_glsl_frag = {
	.always_overload = true,
	.short_circuit = false,
	.default_float = true,
	.init = glsl_init_frag,
	.parse = glsl_yyparse,
	.extension = glsl_extension,
	.version = glsl_version,
	.on_include = glsl_on_include,
	.parse_declaration = glsl_parse_declaration,
	.field_attributes = glsl_field_attributes,
	.sublanguage = &glsl_frag_sublanguage,
};
