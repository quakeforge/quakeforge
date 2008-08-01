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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include <QF/hash.h>
#include <QF/sys.h>

#include "class.h"
#include "debug.h"
#include "def.h"
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "immediate.h"
#include "method.h"
#include "options.h"
#include "qfcc.h"
#include "reloc.h"
#include "struct.h"
#include "switch.h"
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

hashtab_t *save_local_inits (scope_t *scope);
hashtab_t *merge_local_inits (hashtab_t *dl_1, hashtab_t *dl_2);
void restore_local_inits (hashtab_t *def_list);
void free_local_inits (hashtab_t *def_list);
static def_t *create_def (type_t *type, const char *name, scope_t *scope,
						  storage_class_t storage);

expr_t *argc_expr (void);
expr_t *argv_expr (void);

%}

%union {
	int			op;
	struct def_s *def;
	struct hashtab_s *def_list;
	struct type_s	*type;
	struct typedef_s *typename;
	struct expr_s	*expr;
	int			integer_val;
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
%nonassoc BREAK_PRIMARY
%nonassoc ';'
%nonassoc CLASS_NOT_CATEGORY

%nonassoc STORAGEX

%right	<op> '=' ASX PAS /* pointer assign */
%right	'?' ':'
%left	OR
%left	AND
%left	'|'
%left	'^'
%left	'&'
%left	EQ NE
%left	LE GE LT GT
%left	SHL SHR
%left	'+' '-'
%left	'*' '/' '%'
%right	<op> UNARY INCOP
%left	HYPERUNARY
%left	'.' '(' '['

%token	<string_val> CLASS_NAME NAME STRING_VAL
%token	<integer_val> INT_VAL
%token	<float_val> FLOAT_VAL
%token	<vector_val> VECTOR_VAL
%token	<quaternion_val> QUATERNION_VAL

%token	LOCAL RETURN WHILE DO IF ELSE FOR BREAK CONTINUE ELLIPSIS NIL
%token	IFBE IFB IFAE IFA
%token	SWITCH CASE DEFAULT STRUCT UNION ENUM TYPEDEF SUPER SELF THIS
%token	ARGS ARGC ARGV EXTERN STATIC SYSTEM SIZEOF OVERLOAD
%token	<type> TYPE
%token	<typename> TYPE_NAME
%token	CLASS DEFS ENCODE END IMPLEMENTATION INTERFACE PRIVATE PROTECTED
%token	PROTOCOL PUBLIC SELECTOR

%type	<type>	type non_field_type type_name def simple_def struct_def
%type	<type>	struct_def_item ivar_decl ivar_declarator def_item def_list
%type	<type>	struct_def_list ivars func_type non_func_type
%type	<type>	non_code_func code_func func_init func_defs func_def_list
%type	<def>	fdef_name cfunction_def func_def
%type	<param> function_decl
%type	<param>	param param_list
%type	<def>	opt_initializer methoddef var_initializer
%type	<expr>	const opt_expr fexpr expr element_list element_list1 element
%type	<expr>	string_val opt_state_expr think opt_step array_decl texpr
%type	<expr>	statement statements statement_block
%type	<expr>	label break_label continue_label enum_list enum
%type	<expr>	unary_expr primary cast_expr opt_arg_list arg_list
%type	<function> begin_function builtin_function
%type	<def_list> save_inits
%type	<switch_block> switch_block
%type	<string_val> def_name identifier

%type	<string_val> selector reserved_word
%type	<param>	optparmlist unaryselector keyworddecl keywordselector
%type	<method> methodproto methoddecl
%type	<expr>	obj_expr identifier_list obj_messageexpr obj_string receiver
%type	<protocol_list>	protocolrefs protocol_list
%type	<keywordarg> messageargs keywordarg keywordarglist selectorarg
%type	<keywordarg> keywordnamelist keywordname
%type	<class> class_name new_class_name class_with_super new_class_with_super
%type	<class> classdef
%type	<category> category_name new_category_name
%type	<protocol> protocol_name
%type	<methodlist> methodprotolist methodprotolist2
%type	<strct>	ivar_decl_list
%type	<op>	ci

%{

function_t *current_func;
param_t *current_params;
expr_t	*current_init;
class_type_t *current_class;
expr_t	*local_expr;
expr_t	*break_label;
expr_t	*continue_label;
switch_block_t *switch_block;
struct_t *current_struct;
visibility_type current_visibility;
struct_t *current_ivars;
scope_t *current_scope;
storage_class_t current_storage = st_global;

%}

%expect 2

%%

defs
	: /* empty */
	| defs {if (current_class) PARSE_ERROR;} def
	| defs obj_def
	;

def
	: simple_def				{ }
	| storage_class simple_def	{ current_storage = st_global; }
	| storage_class '{' simple_defs '}' ';'
	  { current_storage = st_global; }
	| STRUCT identifier
	  { current_struct = new_struct ($2); } opt_eq '{' struct_defs '}' ';' { }
	| UNION identifier
	  { current_struct = new_union ($2); } opt_eq '{' struct_defs '}' ';' { }
	| STRUCT identifier ';'		{ decl_struct ($2); }
	| UNION identifier ';'		{ decl_union ($2); }
	| ENUM '{' enum_list opt_comma '}' ';'
	  { process_enum ($3); }
	| TYPEDEF type identifier ';'
	  { new_typedef ($3, $2); }
	| TYPEDEF ENUM '{' enum_list opt_comma '}' identifier ';'
		{
			process_enum ($4);
			new_typedef ($7, &type_integer);
		}
	;

opt_eq
	: /* empty */				{ }
	| '='						{ }
	;

opt_semi
	: /* empty */
	| ';'
	;

simple_defs
	: /* empty */
	| simple_defs simple_def
	;

simple_def
	: non_func_type def_list ';' { }
	| func_type func_defs		{ }
	| cfunction	{ }
	;

cfunction
	: cfunction_def ';'
		{
		}
	| cfunction_def '=' '#' fexpr
		{ $<def>$ = $1; }
		{ $<expr>$ = constant_expr ($4); }
	  builtin_function ';'
		{
			(void) ($<def>5);
			(void) ($<expr>6);
		}
	| cfunction_def opt_state_expr
		{ $<op>$ = current_storage; }
		{ $<def>$ = $1; }
	  begin_function statement_block { $<op>$ = $<op>3; } end_function
	    {
			build_code_function ($5, $2, $6);
			current_func = 0;
			(void) ($<def>4);
			(void) ($<op>7);
		}
	;

cfunction_def
	: OVERLOAD non_func_type identifier function_decl 
		{
			type_t     *type = parse_params ($2, current_params = $4);
			$$ = get_function_def ($3, type, current_scope,
								   current_storage, 1, 1);
		}
	| non_func_type identifier function_decl 
		{
			type_t     *type = parse_params ($1, current_params = $3);
			$$ = get_function_def ($2, type, current_scope,
								   current_storage, 0, 1);
		}
	;

storage_class
	: EXTERN					{ current_storage = st_extern; }
	| STATIC					{ current_storage = st_static; }
	| SYSTEM					{ current_storage = st_system; }
	;

local_storage_class
	: LOCAL						{ current_storage = st_local; }
	| %prec STORAGEX			{ current_storage = st_local; }
	| STATIC					{ current_storage = st_static; }
	;

struct_defs
	: /* empty */
	| struct_defs struct_def ';'
	| DEFS '(' identifier ')'
		{
			class_t    *class = get_class ($3, 0);

			if (class) {
				class_to_struct (class, current_struct);
			} else {
				error (0, "undefined symbol `%s'", $3);
			}
		}
	;

struct_def
	: type { $$ = $1; } struct_def_list { $$ = $<type>2; }
	;

struct_def_list
	: struct_def_list ',' { $$ = $<type>0; } struct_def_item
		{ (void) ($<type>3); }
	| struct_def_item
	;

struct_def_item
	: identifier
		{
			new_struct_field (current_struct, $<type>0, $1, vis_public);
		}
	;

enum_list
	: enum
	| enum_list ',' enum
		{
			if ($3) {
				$3->next = $1;
				$$ = $3;
			} else {
				$$ = $1;
			}
		}
	;

enum
	: identifier				{ $$ = new_name_expr ($1); }
	| identifier '=' fexpr
		{
			$$ = 0;
			$3 = constant_expr ($3);
			if ($3->type < ex_string) {
				error ($3, "non-constant initializer");
			} else if ($3->type != ex_integer) {
				error ($3, "invalid initializer type");
			} else {
				$$ = new_binary_expr ('=', new_name_expr ($1), $3);
			}
		}
	;

type
	: non_func_type
	| func_type
	;

non_func_type
	: '.' type					{ $$ = field_type ($2); }
	| non_field_type			{ $$ = $1; }
	;

func_type
	: non_field_type function_decl
		{
			current_params = $2;
			$$ = parse_params ($1, $2);
		}
	;

non_field_type
	: '(' type ')'				{ $$ = $2; }
	| non_field_type array_decl
		{
			if ($2)
				$$ = array_type ($1, $2->e.integer_val);
			else
				$$ = pointer_type ($1);
		}
	| type_name					{ $$ = $1; }
	;

type_name
	: TYPE						{ $$ = $1; }
	| TYPE_NAME					{ $$ = $1->type; }
	| CLASS_NAME
		{
			class_t    *class = get_class ($1, 0);
			if (!class) {
				error (0, "undefined symbol `%s'", $1);
				class = get_class (0, 1);
			}
			$$ = class->type;
		}
	| STRUCT identifier			{ $$ = decl_struct ($2)->type; }
	| UNION identifier			{ $$ = decl_union ($2)->type; }
	;

function_decl
	: '(' param_list ')'		{ $$ = reverse_params ($2); }
	| '(' param_list ',' ELLIPSIS ')'
		{
			$$ = new_param (0, 0, 0);
			$$->next = $2;
			$$ = reverse_params ($$);
		}
	| '(' ELLIPSIS ')'			{ $$ = new_param (0, 0, 0); }
	| '(' TYPE ')'
		{
			if ($2 != &type_void)
				PARSE_ERROR;
			$$ = 0;
		}
	| '(' ')'					{ $$ = 0; }
	;

param_list
	: param
	| param_list ',' param
		{
			$3->next = $1;
			$$ = $3;
		}
	;

param
	: type identifier			{ $$ = new_param (0, $1, $2); }
	;

array_decl
	: '[' const ']'
		{
			if ($2->type != ex_integer || $2->e.integer_val < 1) {
				error (0, "invalid array size");
				$$ = 0;
			} else {
				$$ = $2;
			}
		}
	| '[' ']'					{ $$ = 0; }
	;

def_list
	: def_list ',' { $$ = $<type>0; } def_item { (void) ($<type>3); }
	| def_item
	;

def_item
	: def_name opt_initializer
		{
			if ($2 && !$2->local
				&& $2->type->type != ev_func)
				def_initialized ($2);
		}
	;

func_defs
	: func_def_list ',' fdef_name func_term
	| func_def func_term		{}
	;

func_term
	: non_code_func ';'			{}
	| code_func opt_semi		{}
	;

func_def_list
	: func_def_list ',' fdef_name func_init
	| func_def func_init		{ $$ = $<type>0; }
	;

fdef_name
	: { $<type>$ = $<type>-1; } func_def { $$ = $2; (void) ($<type>1); }
	;

func_def
	: identifier
		{
			$$ = get_function_def ($1, $<type>0, current_scope,
								   current_storage, 0, 1);
		}
	| OVERLOAD identifier
		{
			$$ = get_function_def ($2, $<type>0, current_scope,
								   current_storage, 1, 1);
		}
	;

func_init
	: non_code_func
	| code_func
	;

builtin_function
	: /* emtpy */
		{
			def_t *def = $<def>-1;
			if (!def->external) {
				$$ = build_builtin_function (def, $<expr>0);
				build_scope ($$, $$->def, current_params);
				flush_scope ($$->scope, 1);
			}
		}
	;

non_code_func
	: '=' '#' fexpr
		{ $<def>$ = $<def>0; }
		{ $<expr>$ = constant_expr ($3); }
	  builtin_function
		{
			(void) ($<def>4);
			(void) ($<expr>5);
		}
	| /* emtpy */
		{
		}
	;

code_func
	: '=' opt_state_expr
		{ $<op>$ = current_storage; }
		{ $<def>$ = $<def>0; }
	  begin_function statement_block { $<op>$ = $<op>3; } end_function
		{
			build_code_function ($5, $2, $6);
			current_func = 0;
			(void) ($<def>4);
			(void) ($<op>7);
		}
	;

def_name
	: identifier				{ $$ = $1; }
	;

opt_initializer
	: /*empty*/
		{
			$$ = create_def ($<type>-1, $<string_val>0, current_scope,
							 current_storage);
		}
	| var_initializer			{ $$ = $1; }
	;

var_initializer
	: '=' expr	// don't bother folding twice
		{
			int         local = 0;
			const char *name = $<string_val>0;
			type_t     *type = $<type>-1;

			if (current_scope->type == sc_params) {
				local = 0;
				$$ = create_def (type, name, current_scope, st_static);
			} else {
				local = current_scope->type == sc_local;
				$$ = create_def (type, name, current_scope, current_storage);
			}
			if (local) {
				expr_t     *expr = assign_expr (new_def_expr ($$), $2);
				expr = fold_constants (expr);
				append_expr (local_expr, expr);
				def_initialized ($$);
			} else {
				$2 = constant_expr ($2);
				if ($2->type >= ex_string) {
					if ($$->constant) {
						error ($2, "%s re-initialized", $$->name);
					} else {
						if ($$->type->type == ev_func) {
							PARSE_ERROR;
						} else {
							ReuseConstant ($2,  $$);
						}
					}
				} else {
					error ($2, "non-constant expression used for initializer");
				}
			}
		}
	| '=' '{' { current_init = new_block_expr (); } element_list '}'
		{
			$$ = create_def ($<type>-1, $<string_val>0, current_scope,
							 current_storage);
			init_elements ($$, $4);
			current_init = 0;
		}
	;

opt_state_expr
	: /* emtpy */
		{
			$$ = 0;
		}
	| '[' fexpr ',' think opt_step ']'
		{
			if ($2->type == ex_integer)
				convert_int ($2);
			else if ($2->type == ex_uinteger)
				convert_uint ($2);
			if (!type_assignable (&type_float, get_type ($2)))
				error ($2, "invalid type for frame number");
			$2 = cast_expr (&type_float, $2);
			if (extract_type ($4) != ev_func)
				error ($2, "invalid type for think");
			if ($5) {
				if ($5->type == ex_integer)
					convert_int ($5);
				else if ($5->type == ex_uinteger)
					convert_uint ($5);
				if (!type_assignable (&type_float, get_type ($5)))
					error ($5, "invalid type for time step");
			}

			$$ = new_state_expr ($2, $4, $5);
		}
	;

think
	: def_name
		{
			$$ = new_def_expr (create_def (&type_function, $1, current_scope,
										   current_storage));
			(void) ($<type>1);
		}
	| '(' fexpr ')'
		{
			$$ = $2;
		}
	;

opt_step
	: ',' fexpr					{ $$ = $2; }
	| /* empty */				{ $$ = 0; }
	;

element_list
	: /* empty */
		{
			$$ = new_block_expr ();
		}
	| element_list1 opt_comma
		{
			$$ = current_init;
		}
	;

element_list1
	: element
		{
			append_expr (current_init, $1);
		}
	| element_list1 ',' element
		{
			append_expr (current_init, $3);
		}
	;

element
	: '{'
		{
			$$ = current_init;
			current_init = new_block_expr ();
		}
	  element_list
		{
			current_init = $<expr>2;
		}
	  '}'
		{
			$$ = $3;
		}
	| fexpr
		{
			$$ = $1;
		}
	;

opt_comma
	: /* empty */
	| ','
	;

begin_function
	: /*empty*/
		{
			if ($<def>0->constant)
				error (0, "%s redefined", $<def>0->name);
			$$ = current_func = new_function ($<def>0->name);
			$$->def = $<def>0;
			if (!$$->def->external) {
				add_function ($$);
				reloc_def_func ($$, $$->def->ofs);
			}
			$$->code = pr.code->size;
			if (options.code.debug && current_func->aux) {
				pr_lineno_t *lineno = new_lineno ();
				$$->aux->source_line = $$->def->line;
				$$->aux->line_info = lineno - pr.linenos;
				$$->aux->local_defs = pr.num_locals;
				$$->aux->return_type = $$->def->type->aux_type->type;

				lineno->fa.func = $$->aux - pr.auxfunctions;
			}
			build_scope ($$, $<def>0, current_params);
			current_scope = $$->scope;
			current_storage = st_local;
		}
	;

end_function
	: /*empty*/
		{
			current_scope = current_scope->parent;
			current_storage = $<op>0;
		}
	;

statement_block
	: '{' 
		{
			if (!options.traditional) {
				scope_t    *scope = new_scope (sc_local, current_scope->space,
											   current_scope);
				current_scope = scope;
			}
		}
	  statements '}'
		{
			if (!options.traditional) {
				def_t      *defs = current_scope->head;
				int         num_defs = current_scope->num_defs;

				flush_scope (current_scope, 1);

				current_scope = current_scope->parent;
				current_scope->num_defs += num_defs;
				*current_scope->tail = defs;
				while (*current_scope->tail) {
					(*current_scope->tail)->scope = current_scope;
					current_scope->tail = &(*current_scope->tail)->def_next;
				}
			}
			$$ = $3;
		}
	;

statements
	: /*empty*/
		{
			$$ = new_block_expr ();
		}
	| statements statement
		{
			$$ = append_expr ($1, $2);
		}
	;

statement
	: ';'						{ $$ = 0; }
	| statement_block			{ $$ = $1; }
	| RETURN fexpr ';'
		{
			$$ = return_expr (current_func, $2);
		}
	| RETURN ';'
		{
			$$ = return_expr (current_func, 0);
		}
	| BREAK ';'
		{
			$$ = 0;
			if (break_label)
				$$ = new_unary_expr ('g', break_label);
			else
				error (0, "break outside of loop or switch");
		}
	| CONTINUE ';'
		{
			$$ = 0;
			if (continue_label)
				$$ = new_unary_expr ('g', continue_label);
			else
				error (0, "continue outside of loop");
		}
	| CASE fexpr ':'
		{
			$$ = case_label_expr (switch_block, $2);
		}
	| DEFAULT ':'
		{
			$$ = case_label_expr (switch_block, 0);
		}
	| SWITCH break_label switch_block '(' fexpr ')'
		{
			switch_block->test = $5;
		}
	  save_inits statement_block
		{
			restore_local_inits ($8);
			free_local_inits ($8);
			$$ = switch_expr (switch_block, break_label, $9);
			switch_block = $3;
			break_label = $2;
		}
	| WHILE break_label continue_label '(' texpr ')' save_inits statement
		{
			expr_t *l1 = new_label_expr ();
			expr_t *l2 = break_label;
			int         line = pr.source_line;
			string_t    file = pr.source_file;

			pr.source_line = $5->line;
			pr.source_file = $5->file;

			restore_local_inits ($7);
			free_local_inits ($7);

			$$ = new_block_expr ();

			append_expr ($$, new_unary_expr ('g', continue_label));
			append_expr ($$, l1);
			append_expr ($$, $8);
			append_expr ($$, continue_label);

			$5 = convert_bool ($5, 1);
			if ($5->type != ex_error) {
				backpatch ($5->e.bool.true_list, l1);
				backpatch ($5->e.bool.false_list, l2);
				append_expr ($5->e.bool.e, l2);
				append_expr ($$, $5);
			}

			break_label = $2;
			continue_label = $3;

			pr.source_line = line;
			pr.source_file = file;
		}
	| DO break_label continue_label statement WHILE '(' texpr ')' ';'
		{
			expr_t *l1 = new_label_expr ();
			int         line = pr.source_line;
			string_t    file = pr.source_file;

			pr.source_line = $7->line;
			pr.source_file = $7->file;

			$$ = new_block_expr ();

			append_expr ($$, l1);
			append_expr ($$, $4);
			append_expr ($$, continue_label);

			$7 = convert_bool ($7, 1);
			if ($7->type != ex_error) {
				backpatch ($7->e.bool.true_list, l1);
				backpatch ($7->e.bool.false_list, break_label);
				append_expr ($7->e.bool.e, break_label);
				append_expr ($$, $7);
			}

			break_label = $2;
			continue_label = $3;

			pr.source_line = line;
			pr.source_file = file;
		}
	| local_storage_class type
		{
			$<type>$ = $2;
			local_expr = new_block_expr ();
		}
	  def_list ';'
		{
			$$ = local_expr;
			local_expr = 0;
			(void) ($<type>3);
			current_storage = st_local;
		}
	| IF '(' texpr ')' save_inits statement %prec IFX
		{
			expr_t *tl = new_label_expr ();
			expr_t *fl = new_label_expr ();
			int         line = pr.source_line;
			string_t    file = pr.source_file;

			pr.source_line = $3->line;
			pr.source_file = $3->file;

			$$ = new_block_expr ();

			restore_local_inits ($5);
			free_local_inits ($5);

			$3 = convert_bool ($3, 1);
			if ($3->type != ex_error) {
				backpatch ($3->e.bool.true_list, tl);
				backpatch ($3->e.bool.false_list, fl);
				append_expr ($3->e.bool.e, tl);
				append_expr ($$, $3);
			}

			append_expr ($$, $6);
			append_expr ($$, fl);

			pr.source_line = line;
			pr.source_file = file;
		}
	| IF '(' texpr ')' save_inits statement ELSE
		{
			$<def_list>$ = save_local_inits (current_scope);
			restore_local_inits ($5);
		}
	  statement
		{
			expr_t     *tl = new_label_expr ();
			expr_t     *fl = new_label_expr ();
			expr_t     *nl = new_label_expr ();
			expr_t     *e;
			hashtab_t  *merged;
			hashtab_t  *else_ini;
			int         line = pr.source_line;
			string_t    file = pr.source_file;

			pr.source_line = $3->line;
			pr.source_file = $3->file;

			$$ = new_block_expr ();

			else_ini = save_local_inits (current_scope);

			restore_local_inits ($5);
			free_local_inits ($5);

			$3 = convert_bool ($3, 1);
			if ($3->type != ex_error) {
				backpatch ($3->e.bool.true_list, tl);
				backpatch ($3->e.bool.false_list, fl);
				append_expr ($3->e.bool.e, tl);
				append_expr ($$, $3);
			}

			append_expr ($$, $6);

			e = new_unary_expr ('g', nl);
			append_expr ($$, e);

			append_expr ($$, fl);
			append_expr ($$, $9);
			append_expr ($$, nl);

			merged = merge_local_inits ($<def_list>8, else_ini);
			restore_local_inits (merged);
			free_local_inits (merged);
			free_local_inits (else_ini);
			free_local_inits ($<def_list>8);

			pr.source_line = line;
			pr.source_file = file;
		}
	| FOR break_label continue_label
			'(' opt_expr ';' opt_expr ';' opt_expr ')' save_inits statement
		{
			expr_t     *tl = new_label_expr ();
			expr_t     *fl = break_label;
			expr_t     *l1 = 0;
			expr_t     *t;
			int         line = pr.source_line;
			string_t    file = pr.source_file;

			if ($9)
				t = $9;
			else if ($7)
				t = $7;
			else if ($5)
				t = $5;
			else
				t = continue_label;
			pr.source_line = t->line;
			pr.source_file = t->file;

			restore_local_inits ($11);
			free_local_inits ($11);

			$$ = new_block_expr ();

			append_expr ($$, $5);
			if ($7) {
				l1 = new_label_expr ();
				append_expr ($$, new_unary_expr ('g', l1));
			}
			append_expr ($$, tl);
			append_expr ($$, $12);
			append_expr ($$, continue_label);
			append_expr ($$, $9);
			if ($7) {
				append_expr ($$, l1);
				$7 = convert_bool ($7, 1);
				if ($7->type != ex_error) {
					backpatch ($7->e.bool.true_list, tl);
					backpatch ($7->e.bool.false_list, fl);
					append_expr ($7->e.bool.e, fl);
					append_expr ($$, $7);
				}
			} else {
				append_expr ($$, new_unary_expr ('g', tl));
				append_expr ($$, fl);
			}
			break_label = $2;
			continue_label = $3;

			pr.source_line = line;
			pr.source_file = file;
		}
	| fexpr ';'
		{
			$$ = $1;
		}
	;

label
	: /* empty */
		{
			$$ = new_label_expr ();
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

switch_block
	: /* empty */
		{
			$$ = switch_block;
			switch_block = new_switch_block ();
		}
	;

save_inits
	: /* empty */
		{
			$$ = save_local_inits (current_scope);
		}
	;

opt_expr
	: fexpr
	| /* empty */
		{
			$$ = 0;
		}
	;

unary_expr
	: primary
	| '+' cast_expr %prec UNARY	{ $$ = $2; }
	| '-' cast_expr %prec UNARY	{ $$ = unary_expr ('-', $2); }
	| '!' cast_expr %prec UNARY	{ $$ = unary_expr ('!', $2); }
	| '~' cast_expr %prec UNARY	{ $$ = unary_expr ('~', $2); }
	| '&' cast_expr %prec UNARY	{ $$ = address_expr ($2, 0, 0); }
	| SIZEOF unary_expr	%prec UNARY	{ $$ = sizeof_expr ($2, 0); }
	| SIZEOF '(' type ')'	 %prec HYPERUNARY	{ $$ = sizeof_expr (0, $3); }
	;

primary
	: NAME      				{ $$ = new_name_expr ($1); }
	| BREAK	%prec BREAK_PRIMARY { $$ = new_name_expr (save_string ("break")); }
	| ARGS						{ $$ = new_name_expr (".args"); }
	| ARGC						{ $$ = argc_expr (); }
	| ARGV						{ $$ = argv_expr (); }
	| SELF						{ $$ = new_self_expr (); }
	| THIS						{ $$ = new_this_expr (); }
	| const						{ $$ = $1; }
	| '(' expr ')'				{ $$ = $2; $$->paren = 1; }
	| primary '(' opt_arg_list ')' { $$ = function_expr ($1, $3); }
	| primary '[' expr ']'		{ $$ = array_expr ($1, $3); }
	| primary '.' primary		{ $$ = binary_expr ('.', $1, $3); }
	| INCOP primary				{ $$ = incop_expr ($1, $2, 0); }
	| primary INCOP				{ $$ = incop_expr ($2, $1, 1); }
	| obj_expr					{ $$ = $1; }
	;

cast_expr
	: unary_expr
	| '(' type ')' cast_expr %prec UNARY	{ $$ = cast_expr ($2, $4); }
	;

expr
	: cast_expr
	| expr '=' expr				{ $$ = assign_expr ($1, $3); }
	| expr ASX expr				{ $$ = asx_expr ($2, $1, $3); }
	| expr '?' expr ':' expr 	{ $$ = conditional_expr ($1, $3, $5); }
	| expr AND label expr		{ $$ = bool_expr (AND, $3, $1, $4); }
	| expr OR label expr		{ $$ = bool_expr (OR,  $3, $1, $4); }
	| expr EQ expr				{ $$ = binary_expr (EQ,  $1, $3); }
	| expr NE expr				{ $$ = binary_expr (NE,  $1, $3); }
	| expr LE expr				{ $$ = binary_expr (LE,  $1, $3); }
	| expr GE expr				{ $$ = binary_expr (GE,  $1, $3); }
	| expr LT expr				{ $$ = binary_expr (LT,  $1, $3); }
	| expr GT expr				{ $$ = binary_expr (GT,  $1, $3); }
	| expr SHL expr				{ $$ = binary_expr (SHL, $1, $3); }
	| expr SHR expr				{ $$ = binary_expr (SHR, $1, $3); }
	| expr '+' expr				{ $$ = binary_expr ('+', $1, $3); }
	| expr '-' expr				{ $$ = binary_expr ('-', $1, $3); }
	| expr '*' expr				{ $$ = binary_expr ('*', $1, $3); }
	| expr '/' expr				{ $$ = binary_expr ('/', $1, $3); }
	| expr '&' expr				{ $$ = binary_expr ('&', $1, $3); }
	| expr '|' expr				{ $$ = binary_expr ('|', $1, $3); }
	| expr '^' expr				{ $$ = binary_expr ('^', $1, $3); }
	| expr '%' expr				{ $$ = binary_expr ('%', $1, $3); }
	;

fexpr
	: expr						{ $$ = fold_constants ($1); }
	;

texpr
	: fexpr						{ $$ = convert_bool ($1, 1); }
	;

opt_arg_list
	: /* emtpy */				{ $$ = 0; }
	| arg_list					{ $$ = $1; }
	;

arg_list
	: fexpr
	| arg_list ',' fexpr
		{
			$3->next = $1;
			$$ = $3;
		}
	;

const
	: FLOAT_VAL					{ $$ = new_float_expr ($1); }
	| string_val				{ $$ = $1; }
	| VECTOR_VAL				{ $$ = new_vector_expr ($1); }
	| QUATERNION_VAL			{ $$ = new_quaternion_expr ($1); }
	| INT_VAL					{ $$ = new_integer_expr ($1); }
	| NIL						{ $$ = new_nil_expr (); }
	;

string_val
	: STRING_VAL				{ $$ = new_string_expr ($1); }
	| string_val STRING_VAL
		{
			$$ = binary_expr ('+', $1, new_string_expr ($2));
		}
	;

identifier
	: NAME
	| BREAK						{ $$ = save_string ("break"); }
	| CLASS_NAME
	| TYPE_NAME					{ $$ = $1->name; }
	;

// Objective-QC stuff

obj_def
	: classdef					{ }
	| classdecl
	| protocoldef
	| { if (!current_class) PARSE_ERROR; } methoddef
	| END
		{
			if (!current_class)
				PARSE_ERROR;
			else
				class_finish (current_class);
			current_class = 0;
		}
	;

identifier_list
	: identifier
		{
			$$ = new_block_expr ();
			append_expr ($$, new_name_expr ($1));
		}
	| identifier_list ',' identifier
		{
			append_expr ($1, new_name_expr ($3));
			$$ = $1;
		}
	;

classdecl
	: CLASS identifier_list ';'
		{
			expr_t     *e;
			for (e = $2->e.block.head; e; e = e->next)
				get_class (e->e.string_val, 1);
		}
	;

class_name
	: identifier %prec CLASS_NOT_CATEGORY
		{
			$$ = get_class ($1, 0);
			if (!$$) {
				error (0, "undefined symbol `%s'", $1);
				$$ = get_class (0, 1);
			}
		}
	;

new_class_name
	: identifier
		{
			$$ = get_class ($1, 1);
			if ($$->defined) {
				error (0, "redefinition of `%s'", $1);
				$$ = get_class (0, 1);
			}
		}
	;

class_with_super
	: class_name ':' class_name
		{
			if ($1->super_class != $3)
				error (0, "%s is not a super class of %s", $3->name, $1->name);
			$$ = $1;
		}
	;

new_class_with_super
	: new_class_name ':' class_name
		{
			$1->super_class = $3;
			$$ = $1;
		}
	;

category_name
	: identifier '(' identifier ')'
		{
			$$ = get_category ($1, $3, 0);
			if (!$$) {
				error (0, "undefined category `%s (%s)'", $1, $3);
				$$ = get_category (0, 0, 1);
			}
		}
	;

new_category_name
	: identifier '(' identifier ')'
		{
			$$ = get_category ($1, $3, 1);
			if ($$->defined) {
				error (0, "redefinition of category `%s (%s)'", $1, $3);
				$$ = get_category (0, 0, 1);
			}
		}
	;

protocol_name
	: identifier
		{
			$$ = get_protocol ($1, 0);
			if ($$) {
				error (0, "redefinition of %s", $1);
				$$ = get_protocol (0, 1);
			} else {
				$$ = get_protocol ($1, 1);
			}
		}
	;

classdef
	: INTERFACE new_class_name
	  protocolrefs						{ class_add_protocols ($2, $3); }
	  '{'								{ $$ = $2; }
	  ivar_decl_list '}'				{ class_add_ivars ($2, $7); $$ = $2; }
	  methodprotolist					{ class_add_methods ($2, $10); }
	  END
		{
			current_class = 0;
			(void) ($<class>6);
			(void) ($<class>9);
		}
	| INTERFACE new_class_name
	  protocolrefs					{ class_add_protocols ($2, $3); }
					{ class_add_ivars ($2, class_new_ivars ($2)); $$ = $2; }
	  methodprotolist					{ class_add_methods ($2, $6); }
	  END
		{
			current_class = 0;
			(void) ($<class>5);
		}
	| INTERFACE new_class_with_super
	  protocolrefs						{ class_add_protocols ($2, $3);}
	  '{'								{ $$ = $2; }
	  ivar_decl_list '}'				{ class_add_ivars ($2, $7); $$ = $2; }
	  methodprotolist					{ class_add_methods ($2, $10); }
	  END
		{
			current_class = 0;
			(void) ($<class>6);
			(void) ($<class>9);
		}
	| INTERFACE new_class_with_super
	  protocolrefs						{ class_add_protocols ($2, $3); }
					{ class_add_ivars ($2, class_new_ivars ($2)); $$ = $2; }
	  methodprotolist					{ class_add_methods ($2, $6); }
	  END
		{
			current_class = 0;
			(void) ($<class>5);
		}
	| INTERFACE new_category_name
	  protocolrefs		{ category_add_protocols ($2, $3); $$ = $2->class;}
	  methodprotolist					{ category_add_methods ($2, $5); }
	  END
		{
			current_class = 0;
			(void) ($<class>4);
		}
	| IMPLEMENTATION class_name			{ class_begin (&$2->class_type); }
	  '{'								{ $$ = $2; }
	  ivar_decl_list '}'
		{
			class_check_ivars ($2, $6);
			(void) ($<class>5);
		}
	| IMPLEMENTATION class_name			{ class_begin (&$2->class_type); }
	| IMPLEMENTATION class_with_super	{ class_begin (&$2->class_type); }
	  '{'								{ $$ = $2; }
	  ivar_decl_list '}'
		{
			class_check_ivars ($2, $6);
			(void) ($<class>5);
		}
	| IMPLEMENTATION class_with_super	{ class_begin (&$2->class_type); }
	| IMPLEMENTATION category_name		{ class_begin (&$2->class_type); }
	;

protocoldef
	: PROTOCOL protocol_name 
	  protocolrefs			{ protocol_add_protocols ($2, $3); $<class>$ = 0; }
	  methodprotolist			{ protocol_add_methods ($2, $5); }
	  END						{ (void) ($<class>4); }
	;

protocolrefs
	: /* emtpy */				{ $$ = 0; }
	| LT 						{ $$ = new_protocol_list (); }
	  protocol_list GT			{ $$ = $3; (void) ($<protocol_list>2); }
	;

protocol_list
	: identifier
		{
			$$ = add_protocol ($<protocol_list>0, $1);
		}
	| protocol_list ',' identifier
		{
			$$ = add_protocol ($1, $3);
		}
	;

ivar_decl_list
	:	/* */
		{
			current_visibility = vis_protected;
			current_ivars = class_new_ivars ($<class>0);
		}
	  ivar_decl_list_2
		{
			$$ = current_ivars;
			current_ivars = 0;
			(void) ($<class>1);
		}
	;

ivar_decl_list_2
	: ivar_decl_list_2 visibility_spec ivar_decls
	| ivar_decls
	;

visibility_spec
	: PRIVATE					{ current_visibility = vis_private; }
	| PROTECTED					{ current_visibility = vis_protected; }
	| PUBLIC					{ current_visibility = vis_public; }
	;

ivar_decls
	: /* empty */
	| ivar_decls ivar_decl ';'
	;

ivar_decl
	: type { $$ = $1; } ivars	{ (void) ($<type>2); }
	;

ivars
	: ivar_declarator
	| ivars ',' { $$ = $<type>0; } ivar_declarator { (void) ($<type>3); }
	;

ivar_declarator
	: identifier
		{
			new_struct_field (current_ivars, $<type>0, $1, current_visibility);
		}
	;

methoddef
	: ci methoddecl
		{
			$2->instance = $1;
			$2 = class_find_method (current_class, $2);
		}
	  opt_state_expr
		{ $<op>$ = current_storage; }
		{
			$$ = $2->def = method_def (current_class, $2);
			current_params = $2->params;
		}
	  begin_function statement_block { $<op>$ = $<op>5; } end_function
		{
			$2->func = build_code_function ($7, $4, $8);
			current_func = 0;
			(void) ($<method>6);
			(void) ($<op>9);
		}
	| ci methoddecl
		{
			$2->instance = $1;
			$2 = class_find_method (current_class, $2);
		}
	  '=' '#' const
		{ $<def>$ = $2->def = method_def (current_class, $2); }
		{ $<expr>$ = $6; }
	  builtin_function ';'
		{
			$2->func = $9;

			(void) ($<def>7);
			(void) ($<expr>8);
		}
	;

ci
	: '+'						{ $$ = 0; }
	| '-'						{ $$ = 1; }
	;

methodprotolist
	: /* emtpy */				{ $$ = 0; }
	| methodprotolist2
	;

methodprotolist2
	: { } methodproto
		{
			$$ = new_methodlist ();
			add_method ($$, $2);
		}
	| methodprotolist2 methodproto
		{
			add_method ($1, $2);
			$$ = $1;
		}
	;

methodproto
	: '+' methoddecl ';'
		{
			$2->instance = 0;
			$$ = $2;
		}
	| '-' methoddecl ';'
		{
			$2->instance = 1;
			$$ = $2;
		}
	;

methoddecl
	: '(' type ')' unaryselector
		{ $$ = new_method ($2, $4, 0); }
	| unaryselector
		{ $$ = new_method (&type_id, $1, 0); }
	| '(' type ')' keywordselector optparmlist
		{ $$ = new_method ($2, $4, $5); }
	| keywordselector optparmlist
		{ $$ = new_method (&type_id, $1, $2); }
	;

optparmlist
	: /* empty */				{ $$ = 0; }
	| ',' ELLIPSIS				{ $$ = new_param (0, 0, 0); }
	| ',' param_list			{ $$ = $2; }
	| ',' param_list ',' ELLIPSIS
		{
			$$ = new_param (0, 0, 0);
			$$->next = $2;
		}
	;

unaryselector
	: selector					{ $$ = new_param ($1, 0, 0); }
	;

keywordselector
	: keyworddecl
	| keywordselector keyworddecl { $2->next = $1; $$ = $2; }
	;

selector
	: NAME						{ $$ = save_string ($1); }
	| CLASS_NAME				{ $$ = save_string ($1); }
	| TYPE						{ $$ = save_string (yytext); }
	| TYPE_NAME					{ $$ = save_string ($1->name); }
	| reserved_word
	;

reserved_word
	: LOCAL						{ $$ = save_string (yytext); }
	| RETURN					{ $$ = save_string (yytext); }
	| WHILE						{ $$ = save_string (yytext); }
	| DO						{ $$ = save_string (yytext); }
	| IF						{ $$ = save_string (yytext); }
	| ELSE						{ $$ = save_string (yytext); }
	| FOR						{ $$ = save_string (yytext); }
	| BREAK						{ $$ = save_string (yytext); }
	| CONTINUE					{ $$ = save_string (yytext); }
	| SWITCH					{ $$ = save_string (yytext); }
	| CASE						{ $$ = save_string (yytext); }
	| DEFAULT					{ $$ = save_string (yytext); }
	| NIL						{ $$ = save_string (yytext); }
	| STRUCT					{ $$ = save_string (yytext); }
	| UNION						{ $$ = save_string (yytext); }
	| ENUM						{ $$ = save_string (yytext); }
	| TYPEDEF					{ $$ = save_string (yytext); }
	| SUPER						{ $$ = save_string (yytext); }
	;

keyworddecl
	: selector ':' '(' type ')' identifier
		{ $$ = new_param ($1, $4, $6); }
	| selector ':' identifier
		{ $$ = new_param ($1, &type_id, $3); }
	| ':' '(' type ')' identifier
		{ $$ = new_param ("", $3, $5); }
	| ':' identifier
		{ $$ = new_param ("", &type_id, $2); }
	;

obj_expr
	: obj_messageexpr
	| SELECTOR '(' selectorarg ')'	{ $$ = selector_expr ($3); }
	| PROTOCOL '(' identifier ')'	{ $$ = protocol_expr ($3); }
	| ENCODE '(' type ')'			{ $$ = encode_expr ($3); }
	| obj_string /* FIXME string object? */
	;

obj_messageexpr
	: '[' receiver messageargs ']'	{ $$ = message_expr ($2, $3); }
	;

receiver
	: fexpr
	| CLASS_NAME				{ $$ = new_name_expr ($1); }
	| SUPER						{ $$ = new_name_expr ("super"); }
	;

messageargs
	: selector					{ $$ = new_keywordarg ($1, 0); }
	| keywordarglist
	;

keywordarglist
	: keywordarg
	| keywordarglist keywordarg
		{
			$2->next = $1;
			$$ = $2;
		}
	;

keywordarg
	: selector ':' arg_list		{ $$ = new_keywordarg ($1, $3); }
	| ':' arg_list				{ $$ = new_keywordarg ("", $2); }
	;

selectorarg
	: selector					{ $$ = new_keywordarg ($1, 0); }
	| keywordnamelist
	;

keywordnamelist
	: keywordname
	| keywordnamelist keywordname
		{
			$2->next = $1;
			$$ = $2;
		}
	;

keywordname
	: selector ':'				{ $$ = new_keywordarg ($1, new_nil_expr ()); }
	| ':'						{ $$ = new_keywordarg ("", new_nil_expr ()); }
	;

obj_string
	: '@' STRING_VAL			{ $$ = new_string_expr ($2); }
	| obj_string '@' STRING_VAL
		{
			$$ = binary_expr ('+', $1, new_string_expr ($3));
		}
	;

%%

typedef struct def_state_s {
	struct def_state_s *next;
	def_t		*def;
	int			state;
} def_state_t;

static def_state_t *free_def_states;

static const char *
ds_get_key (void *_d, void *unused)
{
	return ((def_state_t *)_d)->def->name;
}

static void
ds_free_key (void *_d, void *unused)
{
	def_state_t *d = (def_state_t *)_d;
	d->next = free_def_states;
	free_def_states = d;
}

static void
scan_scope (hashtab_t *tab, scope_t *scope)
{
	def_t      *def;
	if (scope->type == sc_local)
		scan_scope (tab, scope->parent);
	for (def = scope->head; def; def = def->def_next) {
		if  (def->name && !def->removed) {
			def_state_t *ds;
			ALLOC (1024, def_state_t, def_states, ds);
			ds->def = def;
			ds->state = def->initialized;
			Hash_Add (tab, ds);
		}
	}
}

static def_t *
create_def (type_t *type, const char *name, scope_t *scope,
			storage_class_t storage)
{
	scope_type  st = scope->type;
	// traditional functions have only the one scope
	//FIXME should the scope be set to local when the params scope
	//is created in traditional mode?
	if (options.traditional && st == sc_params)
		st = sc_local;
	if (st == sc_local) {
		def_t      *def = get_def (0, name, scope, st_none);
		if (def) {
			if (def->scope->type == sc_params
				&& scope->parent->type == sc_params) {
				warning (0, "local %s shadows param %s", name,
						 def->name);
			}
			if (def->scope == scope) {
				expr_t      e;
				warning (0, "local %s redeclared", name);
				e.file = def->file;
				e.line = def->line;
				warning (&e, "previously declared here");
			}
		}
	}
	return get_def (type, name, scope, storage);
}

hashtab_t *
save_local_inits (scope_t *scope)
{
	hashtab_t  *tab = Hash_NewTable (61, ds_get_key, ds_free_key, 0);
	scan_scope (tab, scope);
	return tab;
}

hashtab_t *
merge_local_inits (hashtab_t *dl_1, hashtab_t *dl_2)
{
	hashtab_t  *tab = Hash_NewTable (61, ds_get_key, ds_free_key, 0);
	def_state_t **ds_list = (def_state_t **)Hash_GetList (dl_1);
	def_state_t **ds;
	def_state_t *d;
	def_state_t *nds;

	for (ds = ds_list; *ds; ds++) {
		d = Hash_Find (dl_2, (*ds)->def->name);
		(*ds)->def->initialized = (*ds)->state;

		ALLOC (1024, def_state_t, def_states, nds);
		nds->def = (*ds)->def;
		nds->state = (*ds)->state && d->state;
		Hash_Add (tab, nds);
	}
	free (ds_list);
	return tab;
}

void
restore_local_inits (hashtab_t *def_list)
{
	def_state_t **ds_list = (def_state_t **)Hash_GetList (def_list);
	def_state_t **ds;

	for (ds = ds_list; *ds; ds++)
		(*ds)->def->initialized = (*ds)->state;
	free (ds_list);
}

void
free_local_inits (hashtab_t *def_list)
{
	Hash_DelTable (def_list);
}

expr_t *
argc_expr (void)
{
	warning (0, "@argc deprecated: use @args.count");
	return binary_expr ('.', new_name_expr (".args"), new_name_expr ("count"));
}

expr_t *
argv_expr (void)
{
	warning (0, "@argv deprecated: use @args.list");
	return binary_expr ('.', new_name_expr (".args"), new_name_expr ("list"));
}
