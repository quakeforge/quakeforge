%{
/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
static const char rcsid[] = 
	"$Id$";

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

#include "qfcc.h"
#include "expr.h"
#include "function.h"
#include "method.h"
#include "class.h"
#include "struct.h"
#include "switch.h"
#include "type.h"

#define YYDEBUG 1
#define YYERROR_VERBOSE 1

extern char *yytext;
extern int pr_source_line;

void
yyerror (const char *s)
{
	error (0, "%s %s\n", yytext, s);
}

void
parse_error (void)
{
	error (0, "parse error before %s", yytext);
}

#define PARSE_ERROR do { parse_error (); YYERROR; } while (0)

int yylex (void);

hashtab_t *save_local_inits (def_t *scope);
hashtab_t *merge_local_inits (hashtab_t *dl_1, hashtab_t *dl_2);
void restore_local_inits (hashtab_t *def_list);
void free_local_inits (hashtab_t *def_list);

%}

%union {
	int			op;
	def_t		*def;
	struct hashtab_s	*def_list;
	type_t		*type;
	expr_t		*expr;
	int			integer_val;
	float		float_val;
	char		*string_val;
	float		vector_val[3];
	float		quaternion_val[4];
	function_t *function;
	struct switch_block_s	*switch_block;
	struct param_s	*param;
	struct method_s	*method;
	struct class_s	*class;
	struct protocol_s *protocol;
	struct keywordarg_s *keywordarg;
	struct methodlist_s *methodlist;
}

%right	<op> '=' ASX PAS /* pointer assign */
%right	'?' ':'
%left	OR AND
%left	EQ NE LE GE LT GT
%left	SHL SHR %left	'+' '-'
%left	'*' '/' '&' '|' '^' '%'
%right	<op> UNARY INCOP
%right	'(' '['
%left	'.'

%token	<string_val> NAME STRING_VAL
%token	<integer_val> INT_VAL
%token	<float_val> FLOAT_VAL
%token	<vector_val> VECTOR_VAL
%token	<quaternion_val> QUATERNION_VAL

%token	LOCAL RETURN WHILE DO IF ELSE FOR BREAK CONTINUE ELLIPSIS NIL
%token	IFBE IFB IFAE IFA
%token	SWITCH CASE DEFAULT STRUCT ENUM TYPEDEF SUPER SELF THIS
%token	ARGC ARGV
%token	ELE_START
%token	<type> TYPE

%token	CLASS DEFS ENCODE END IMPLEMENTATION INTERFACE PRIVATE PROTECTED
%token	PROTOCOL PUBLIC SELECTOR

%type	<type>	type non_field_type type_name
%type	<param> function_decl
%type	<integer_val> array_decl
%type	<param>	param param_list
%type	<def>	def_name
%type	<def>	def_item def_list
%type	<expr>	const opt_expr expr arg_list element_list element_list1 element
%type	<expr>	string_val opt_state_expr
%type	<expr>	statement statements statement_block
%type	<expr>	break_label continue_label enum_list enum
%type	<function> begin_function
%type	<def_list> save_inits
%type	<switch_block> switch_block

%type	<string_val> selector reserved_word
%type	<param>	optparmlist unaryselector keyworddecl keywordselector
%type	<method> methodproto methoddecl
%type	<expr>	obj_expr identifier_list obj_messageexpr obj_string receiver
%type	<expr>	protocolrefs
%type	<keywordarg> messageargs keywordarg keywordarglist selectorarg
%type	<keywordarg> keywordnamelist keywordname
%type	<class>	class_name new_class_name class_with_super new_class_with_super
%type	<class> category_name new_category_name
%type	<protocol> protocol_name
%type	<methodlist> methodprotolist methodprotolist2
%type	<type>	ivar_decl_list

%expect 2	// statement : if | if else, defs : defs def ';' | defs obj_def

%{

type_t	*current_type;
def_t	*current_def;
function_t *current_func;
param_t *current_params;
expr_t	*current_init;
class_t	*current_class;
expr_t	*local_expr;
expr_t	*break_label;
expr_t	*continue_label;
switch_block_t *switch_block;
type_t	*struct_type;
visibility_t current_visibility;
type_t	*current_ivars;

def_t		*pr_scope;					// the function being parsed, or NULL
string_t	s_file;						// filename for function definition

int      element_flag;

%}

%%

defs
	: /* empty */
	| defs {if (current_class) PARSE_ERROR;} def ';'
	| defs obj_def
	;

def
	: type { current_type = $1; } def_list
	| STRUCT NAME
	  { struct_type = new_struct ($2); } '=' '{' struct_defs '}'
	| ENUM '{' enum_list opt_comma '}'
	  { process_enum ($3); }
	| TYPEDEF type NAME
	  { new_typedef ($3, $2); }
	| TYPEDEF ENUM '{' enum_list opt_comma '}' NAME
		{
			process_enum ($4);
			new_typedef ($7, &type_integer);
		}
	;

struct_defs
	: /* empty */
	| struct_defs struct_def ';'
	| DEFS '(' NAME ')'
		{
			class_t    *class = get_class ($3, 0);

			if (class) {
				copy_struct_fields (struct_type, class->ivars);
			} else {
				error (0, "undefined symbol `%s'", $3);
			}
		}
	;

struct_def
	: type struct_def_list
	  {}
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
	: NAME { $$ = new_name_expr ($1); }
	| NAME '=' expr
		{
			$$ = 0;
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
	: '.' type { $$ = field_type ($2); }
	| non_field_type { $$ = $1; }
	| non_field_type function_decl
		{
			current_params = $2;
			$$ = parse_params ($1, $2);
		}
	| non_field_type array_decl
		{
			$$ = array_type ($1, $2);
		}
	;

non_field_type
	: '(' type ')' { $$ = $2; }
	| type_name { $$ = $1; }
	;

type_name
	: TYPE { $$ = $1; }
	| class_name { $$ = $1->type; }
	;

function_decl
	: '(' param_list ')' { $$ = reverse_params ($2); }
	| '(' param_list ',' ELLIPSIS ')'
		{
			$$ = new_param (0, 0, 0);
			$$->next = $2;
			$$ = reverse_params ($$);
		}
	| '(' ELLIPSIS ')' { $$ = new_param (0, 0, 0); }
	| '(' ')'
		{
			$$ = 0;
		}
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
	: type NAME { $$ = new_param (0, $1, $2); }
	;

array_decl
	: '[' const ']'
		{
			if ($2->type != ex_integer || $2->e.integer_val < 1) {
				error (0, "invalid array size");
				$$ = 0;
			} else {
				$$ = $2->e.integer_val;
			}
		}
	| '[' ']' { $$ = 0; }
	;

struct_def_list
	: struct_def_list ',' struct_def_item
	| struct_def_item
	;

struct_def_item
	: NAME { new_struct_field (struct_type, current_type, $1, vis_public); }
	;

def_list
	: def_list ',' def_item
	| def_item
	;

def_item
	: def_name opt_initializer
		{
			$$ = $1;
			if ($$ && !$$->scope && $$->type->type != ev_func)
				PR_DefInitialized ($$);
		}
	;

def_name
	: NAME
		{
			int *alloc = &numpr_globals;

			if (pr_scope) {
				alloc = pr_scope->alloc;
				if (pr_scope->scope && !pr_scope->scope->scope) {
					def_t      *def = PR_GetDef (0, $1, pr_scope->scope, 0);
					if (def && def->scope && !def->scope->scope)
						warning (0, "local %s shadows param %s", $1, def->name);
				}
			}
			$$ = PR_GetDef (current_type, $1, pr_scope, alloc);
			current_def = $$;
		}
	;

opt_initializer
	: /*empty*/
	| { element_flag = current_type->type != ev_func; } var_initializer
		{ element_flag = 0; }
	;

var_initializer
	: '=' expr
		{
			if (pr_scope) {
				append_expr (local_expr,
							 assign_expr (new_def_expr (current_def), $2));
				PR_DefInitialized (current_def);
			} else {
				if ($2->type >= ex_string) {
					current_def = PR_ReuseConstant ($2,  current_def);
				} else {
					error ($2, "non-constant expression used for initializer");
				}
			}
		}
	| '=' ELE_START { current_init = new_block_expr (); } element_list '}'
		{
			init_elements (current_def, $4);
			current_init = 0;
		}
	| '=' '#' const
		{
			build_builtin_function (current_def, $3);
		}
	| '=' opt_state_expr begin_function statement_block end_function
		{
			build_function ($3);
			if ($2) {
				$2->next = $4;
				emit_function ($3, $2);
			} else {
				emit_function ($3, $4);
			}
			finish_function ($3);
		}
	;

opt_state_expr
	: /* emtpy */
		{
			$$ = 0;
		}
	| '[' const ',' { $<def>$ = current_def; }
	  def_name { current_def = $<def>4; } ']'
		{
			if ($2->type == ex_integer)
				convert_int ($2);
			if ($2->type != ex_float)
				error ($2, "invalid type for frame number");
			if ($5->type->type != ev_func)
				error ($2, "invalid type for think");

			$$ = new_binary_expr ('s', $2, new_def_expr ($5));
		}
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
	| expr
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
			$$ = current_func = new_function ();
			$$->def = current_def;
			$$->code = numstatements;
			if (options.code.debug) {
				pr_lineno_t *lineno = new_lineno ();
				$$->aux = new_auxfunction ();
				$$->aux->source_line = pr_source_line;
				$$->aux->line_info = lineno - linenos;
				$$->aux->local_defs = num_locals;

				lineno->fa.func = $$->aux - auxfunctions;
			}
			pr_scope = current_def;
			build_scope ($$, current_def, current_params);
		}
	;

end_function
	: /*empty*/
		{
			pr_scope = 0;
		}
	;

statement_block
	: '{' 
		{
			def_t      *scope = PR_NewDef (&type_void, ".scope", pr_scope);
			scope->alloc = pr_scope->alloc;
			scope->used = 1;
			pr_scope->scope_next = scope->scope_next;
			scope->scope_next = 0;
			pr_scope = scope;
		}
	  statements '}'
		{
			def_t      *scope = pr_scope;

			PR_FlushScope (pr_scope, 1);

			while (scope->scope_next)
				 scope = scope->scope_next;

			scope->scope_next = pr_scope->scope->scope_next;
			pr_scope->scope->scope_next = pr_scope;
			pr_scope = pr_scope->scope;
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
	: ';' { $$ = 0; }
	| statement_block { $$ = $1; }
	| RETURN expr ';'
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
	| CASE expr ':'
		{
			$$ = case_label_expr (switch_block, $2);
		}
	| DEFAULT ':'
		{
			$$ = case_label_expr (switch_block, 0);
		}
	| SWITCH break_label switch_block '(' expr ')'
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
	| WHILE break_label continue_label '(' expr ')' save_inits statement
		{
			expr_t *l1 = new_label_expr ();
			expr_t *l2 = break_label;
			expr_t *e;

			restore_local_inits ($7);
			free_local_inits ($7);

			$$ = new_block_expr ();

			e = new_binary_expr ('n', test_expr ($5, 1), l2);
			e->line = $5->line;
			e->file = $5->file;
			append_expr ($$, e);
			append_expr ($$, l1);
			append_expr ($$, $8);
			append_expr ($$, continue_label);
			e = new_binary_expr ('i', test_expr ($5, 1), l1);
			e->line = $5->line;
			e->file = $5->file;
			append_expr ($$, e);
			append_expr ($$, l2);
			break_label = $2;
			continue_label = $3;
		}
	| DO break_label continue_label statement WHILE '(' expr ')' ';'
		{
			expr_t *l1 = new_label_expr ();

			$$ = new_block_expr ();

			append_expr ($$, l1);
			append_expr ($$, $4);
			append_expr ($$, continue_label);
			append_expr ($$, new_binary_expr ('i', test_expr ($7, 1), l1));
			append_expr ($$, break_label);
			break_label = $2;
			continue_label = $3;
		}
	| LOCAL type
		{
			current_type = $2;
			local_expr = new_block_expr ();
		}
	  def_list ';'
		{
			$$ = local_expr;
			local_expr = 0;
		}
	| IF '(' expr ')' save_inits statement
		{
			expr_t *l1 = new_label_expr ();
			expr_t *e;

			$$ = new_block_expr ();

			restore_local_inits ($5);
			free_local_inits ($5);

			e = new_binary_expr ('n', test_expr ($3, 1), l1);
			e->line = $3->line;
			e->file = $3->file;
			append_expr ($$, e);
			append_expr ($$, $6);
			append_expr ($$, l1);
		}
	| IF '(' expr ')' save_inits statement ELSE
		{
			$<def_list>$ = save_local_inits (pr_scope);
			restore_local_inits ($5);
		}
	  statement
		{
			expr_t     *l1 = new_label_expr ();
			expr_t     *l2 = new_label_expr ();
			expr_t     *e;
			hashtab_t  *merged;
			hashtab_t  *else_ini;

			$$ = new_block_expr ();

			else_ini = save_local_inits (pr_scope);

			restore_local_inits ($5);
			free_local_inits ($5);

			e = new_binary_expr ('n', test_expr ($3, 1), l1);
			e->line = $3->line;
			e->file = $3->file;
			append_expr ($$, e);

			append_expr ($$, $6);

			e = new_unary_expr ('g', l2);
			append_expr ($$, e);

			append_expr ($$, l1);
			append_expr ($$, $9);
			append_expr ($$, l2);
			merged = merge_local_inits ($<def_list>8, else_ini);
			restore_local_inits (merged);
			free_local_inits (merged);
			free_local_inits (else_ini);
			free_local_inits ($<def_list>8);
		}
	| FOR break_label continue_label
			'(' opt_expr ';' opt_expr ';' opt_expr ')' save_inits statement
		{
			expr_t *l1 = new_label_expr ();
			expr_t *l2 = break_label;

			restore_local_inits ($11);
			free_local_inits ($11);

			$$ = new_block_expr ();

			append_expr ($$, $5);
			if ($7)
				append_expr ($$, new_binary_expr ('n', test_expr ($7, 1), l2));
			append_expr ($$, l1);
			append_expr ($$, $12);
			append_expr ($$, continue_label);
			append_expr ($$, $9);
			if ($5)
				append_expr ($$, new_binary_expr ('i', test_expr ($7, 1), l1));
			append_expr ($$, l2);
			break_label = $2;
			continue_label = $3;
		}
	| expr ';'
		{
			$$ = $1;
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
			$$ = save_local_inits (pr_scope);
		}
	;

opt_expr
	: expr
	| /* empty */
		{
			$$ = 0;
		}
	;

expr
	: expr '=' expr				{ $$ = assign_expr ($1, $3); }
	| expr ASX expr				{ $$ = asx_expr ($2, $1, $3); }
	| expr '?' expr ':' expr 	{ $$ = conditional_expr ($1, $3, $5); }
	| expr AND expr				{ $$ = binary_expr (AND, $1, $3); }
	| expr OR expr				{ $$ = binary_expr (OR,  $1, $3); }
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
	| expr '(' arg_list ')'		{ $$ = function_expr ($1, $3); }
	| expr '(' ')'				{ $$ = function_expr ($1, 0); }
	| TYPE '(' expr ')'			{ $$ = cast_expr ($1, $3); }
	| expr '[' expr ']'			{ $$ = array_expr ($1, $3); }
	| expr '.' expr				{ $$ = binary_expr ('.', $1, $3); }
	| '+' expr %prec UNARY		{ $$ = $2; }
	| '-' expr %prec UNARY		{ $$ = unary_expr ('-', $2); }
	| '!' expr %prec UNARY		{ $$ = unary_expr ('!', $2); }
	| '~' expr %prec UNARY		{ $$ = unary_expr ('~', $2); }
	| '&' expr %prec UNARY		{ $$ = address_expr ($2, 0, 0); }
	| INCOP expr				{ $$ = incop_expr ($1, $2, 0); }
	| expr INCOP				{ $$ = incop_expr ($2, $1, 1); }
	| obj_expr					{ $$ = $1; }
	| NAME						{ $$ = new_name_expr ($1); }
	| ARGC						{ $$ = new_name_expr (".argc"); }
	| ARGV						{ $$ = new_name_expr (".argv"); }
	| SELF						{ $$ = new_self_expr (); }
	| THIS						{ $$ = new_this_expr (); }
	| const						{ $$ = $1; }
	| '(' expr ')'				{ $$ = $2; $$->paren = 1; }
	;

arg_list
	: expr
	| arg_list ',' expr
		{
			$3->next = $1;
			$$ = $3;
		}
	;

const
	: FLOAT_VAL
		{
			$$ = new_expr ();
			$$->type = ex_float;
			$$->e.float_val = $1;
		}
	| string_val
		{
			$$ = $1;
		}
	| VECTOR_VAL
		{
			$$ = new_expr ();
			$$->type = ex_vector;
			memcpy ($$->e.vector_val, $1, sizeof ($$->e.vector_val));
		}
	| QUATERNION_VAL
		{
			$$ = new_expr ();
			$$->type = ex_quaternion;
			memcpy ($$->e.quaternion_val, $1, sizeof ($$->e.quaternion_val));
		}
	| INT_VAL
		{
			$$ = new_expr ();
			$$->type = ex_integer;
			$$->e.integer_val = $1;
		}
	| NIL
		{
			$$ = new_expr ();
			$$->type = ex_nil;
		}
	;

string_val
	: STRING_VAL
		{
			$$ = new_expr ();
			$$->type = ex_string;
			$$->e.string_val = $1;
		}
	| string_val STRING_VAL
		{
			expr_t     *e = new_expr ();
			e->type = ex_string;
			e->e.string_val = $2;
			$$ = binary_expr ('+', $1, e);
		}
	;

obj_def
	: classdef
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
	: NAME
		{
			$$ = new_block_expr ();
			append_expr ($$, new_name_expr ($1));
		}
	| identifier_list ',' NAME
		{
			append_expr ($1, new_name_expr ($3));
			$$ = $1;
		}
	;

classdecl
	: CLASS identifier_list
		{
			expr_t     *e;
			for (e = $2->e.block.head; e; e = e->next)
				get_class (e->e.string_val, 1);
		}
	;

class_name
	: NAME
		{
			$$ = get_class ($1, 0);
			if (!$$) {
				error (0, "undefined symbol `%s'", $1);
				$$ = get_class (0, 1);
			}
		}
	;

new_class_name
	: NAME
		{
			$$ = get_class ($1, 1);
			if ($$->defined) {
				error (0, "redefinition of `%s'", $1);
				$$ = get_class (0, 1);
			}
			current_class = $$;
		}
	;

class_with_super
	: class_name ':' class_name
		{
			if ($1->super_class != $3)
				error (0, "%s is not a super class of %s",
					   $3->class_name, $1->class_name);
			$$ = $1;
		}
	;

new_class_with_super
	: new_class_name ':' class_name
		{
			$1->super_class = $3;
			$$ = $1;
			current_class = $$;
		}
	;

category_name
	: NAME '(' NAME ')'
		{
			$$ = get_category ($1, $3, 0);
			if (!$$) {
				error (0, "undefined category `%s (%s)'", $1, $3);
				$$ = get_category (0, 0, 1);
			}
		}
	;

new_category_name
	: NAME '(' NAME ')'
		{
			$$ = get_category ($1, $3, 1);
			if ($$->defined) {
				error (0, "redefinition of category `%s (%s)'", $1, $3);
				$$ = get_category (0, 0, 1);
			}
			current_class = $$;
		}
	;

protocol_name
	: NAME
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
	  protocolrefs						{ class_add_protocol_methods ($2, $3);}
	  '{' ivar_decl_list '}'			{ class_add_ivars ($2, $6); }
	  methodprotolist					{ class_add_methods ($2, $9); }
	  END								{ current_class = 0; }
	| INTERFACE new_class_name
	  protocolrefs						{ class_add_protocol_methods ($2, $3);}
	  methodprotolist					{ class_add_methods ($2, $5); }
	  END								{ current_class = 0; }
	| INTERFACE new_class_with_super
	  protocolrefs						{ class_add_protocol_methods ($2, $3);}
	  '{' ivar_decl_list '}'			{ class_add_ivars ($2, $6); }
	  methodprotolist					{ class_add_methods ($2, $9); }
	  END								{ current_class = 0; }
	| INTERFACE new_class_with_super
	  protocolrefs						{ class_add_protocol_methods ($2, $3);}
	  methodprotolist					{ class_add_methods ($2, $5); }
	  END								{ current_class = 0; }
	| INTERFACE new_category_name
	  protocolrefs						{ class_add_protocol_methods ($2, $3);}
	  methodprotolist					{ class_add_methods ($2, $5); }
	  END								{ current_class = 0; }
	| IMPLEMENTATION class_name			{ class_begin ($2); }
	  '{' ivar_decl_list '}'			{ class_check_ivars ($2, $5); }
	| IMPLEMENTATION class_name			{ class_begin ($2); }
	| IMPLEMENTATION class_with_super	{ class_begin ($2); }
	  '{' ivar_decl_list '}'			{ class_check_ivars ($2, $5); }
	| IMPLEMENTATION class_with_super	{ class_begin ($2); }
	| IMPLEMENTATION category_name		{ class_begin ($2); }
	;

protocoldef
	: PROTOCOL protocol_name 
	  protocolrefs				{ protocol_add_protocol_methods ($2, $3); }
	  methodprotolist			{ protocol_add_methods ($2, $5); }
	  END
	;

protocolrefs
	: /* emtpy */				{ $$ = 0; }
	| LT identifier_list GT		{ $$ = $2->e.block.head; }
	;

ivar_decl_list
	:	/* */
		{
			current_visibility = vis_protected;
			current_ivars = new_struct (0);
			if (current_class->super_class)
				new_struct_field (current_ivars,
								  current_class->super_class->ivars, 0,
								  vis_private);
		}
	  ivar_decl_list_2
		{
			$$ = current_ivars;
			current_ivars = 0;
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
	: type { current_type = $1; } ivars
	;

ivars
	: ivar_declarator
	| ivars ',' ivar_declarator
	;

ivar_declarator
	: NAME
		{
			new_struct_field (current_ivars, current_type,
							  $1, current_visibility);
		}
	;

methoddef
	: '+' methoddecl
		{
			$2->instance = 0;
			$2 = class_find_method (current_class, $2);
		}
	  opt_state_expr
		{
			current_def = $2->def = method_def (current_class, $2);
			current_params = $2->params;
		}
	  begin_function statement_block end_function
		{
			build_function ($6);
			if ($4) {
				$4->next = $7;
				emit_function ($6, $4);
			} else {
				emit_function ($6, $7);
			}
			finish_function ($6);
		}
	| '+' methoddecl '=' '#' const ';'
		{
			$2->instance = 0;
			$2 = class_find_method (current_class, $2);
			$2->def = method_def (current_class, $2);

			build_builtin_function ($2->def, $5);
		}
	| '-' methoddecl
		{
			$2->instance = 1;
			$2 = class_find_method (current_class, $2);
		}
	  opt_state_expr
		{
			current_def = $2->def = method_def (current_class, $2);
			current_params = $2->params;
		}
	  begin_function statement_block end_function
		{
			build_function ($6);
			if ($4) {
				$4->next = $7;
				emit_function ($6, $4);
			} else {
				emit_function ($6, $7);
			}
			finish_function ($6);
		}
	| '-' methoddecl '=' '#' const ';'
		{
			$2->instance = 1;
			$2 = class_find_method (current_class, $2);
			$2->def = method_def (current_class, $2);

			build_builtin_function ($2->def, $5);
		}
	;

methodprotolist
	: /* emtpy */				{ $$ = 0; }
	| methodprotolist2
	;

methodprotolist2
	: methodproto
		{
			$$ = new_methodlist ();
			add_method ($$, $1);
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
			$2->params->type = &type_Class;
			$$ = $2;
		}
	| '-' methoddecl ';'
		{
			$2->instance = 1;
			$2->params->type = current_class->type;
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
	: NAME
	| TYPE						{ $$ = strdup (yytext); }
	| reserved_word
	;

reserved_word
	: LOCAL						{ $$ = strdup (yytext); }
	| RETURN					{ $$ = strdup (yytext); }
	| WHILE						{ $$ = strdup (yytext); }
	| DO						{ $$ = strdup (yytext); }
	| IF						{ $$ = strdup (yytext); }
	| ELSE						{ $$ = strdup (yytext); }
	| FOR						{ $$ = strdup (yytext); }
	| BREAK						{ $$ = strdup (yytext); }
	| CONTINUE					{ $$ = strdup (yytext); }
	| SWITCH					{ $$ = strdup (yytext); }
	| CASE						{ $$ = strdup (yytext); }
	| DEFAULT					{ $$ = strdup (yytext); }
	| NIL						{ $$ = strdup (yytext); }
	| STRUCT					{ $$ = strdup (yytext); }
	| ENUM						{ $$ = strdup (yytext); }
	| TYPEDEF					{ $$ = strdup (yytext); }
	| SUPER						{ $$ = strdup (yytext); }
	;

keyworddecl
	: selector ':' '(' type ')' NAME
		{ $$ = new_param ($1, $4, $6); }
	| selector ':' NAME
		{ $$ = new_param ($1, &type_id, $3); }
	| ':' '(' type ')' NAME
		{ $$ = new_param ("", $3, $5); }
	| ':' NAME
		{ $$ = new_param ("", &type_id, $2); }
	;

obj_expr
	: obj_messageexpr
	| SELECTOR '(' selectorarg ')'	{ $$ = selector_expr ($3); }
	| PROTOCOL '(' NAME ')'			{ $$ = protocol_expr ($3); }
	| ENCODE '(' type ')'			{ $$ = encode_expr ($3); }
	| obj_string /* FIXME string object? */
	;

obj_messageexpr
	: '[' receiver messageargs ']'	{ $$ = message_expr ($2, $3); }
	;

receiver
	: expr
	| SUPER							{ $$ = new_name_expr ("super"); }
	;

messageargs
	: selector			{ $$ = new_keywordarg ($1, 0); }
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
	: selector ':' arg_list	{ $$ = new_keywordarg ($1, $3); }
	| ':' arg_list			{ $$ = new_keywordarg ("", $2); }
	;

selectorarg
	: selector			{ $$ = new_keywordarg ($1, 0); }
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
	: selector ':'		{ $$ = new_keywordarg ($1, 0); }
	| ':'				{ $$ = new_keywordarg ("", 0); }
	;

obj_string
	: '@' STRING_VAL
		{
			$$ = new_expr ();
			$$->type = ex_string;
			$$->e.string_val = $2;
		}
	| obj_string '@' STRING_VAL
		{
			expr_t     *e = new_expr ();
			e->type = ex_string;
			e->e.string_val = $3;
			$$ = binary_expr ('+', $1, e);
		}
	;

%%

typedef struct {
	def_t		*def;
	int			state;
} def_state_t;

static const char *
get_key (void *_d, void *unused)
{
	return ((def_state_t *)_d)->def->name;
}

static void
free_key (void *_d, void *unused)
{
	free (_d);
}

static void
scan_scope (hashtab_t *tab, def_t *scope)
{
	def_t      *def;
	if (scope->scope)
		scan_scope (tab, scope->scope);
	for (def = scope->scope_next; def; def = def->scope_next) {
		if  (def->name && !def->removed) {
			def_state_t *ds = malloc (sizeof (def_state_t));
			SYS_CHECKMEM (ds);
			ds->def = def;
			ds->state = def->initialized;
			Hash_Add (tab, ds);
		}
	}
}

hashtab_t *
save_local_inits (def_t *scope)
{
	hashtab_t  *tab = Hash_NewTable (61, get_key, free_key, 0);
	scan_scope (tab, scope);
	return tab;
}

hashtab_t *
merge_local_inits (hashtab_t *dl_1, hashtab_t *dl_2)
{
	hashtab_t  *tab = Hash_NewTable (61, get_key, free_key, 0);
	def_state_t **ds_list = (def_state_t **)Hash_GetList (dl_1);
	def_state_t **ds;
	def_state_t *d;
	def_state_t *nds;

	for (ds = ds_list; *ds; ds++) {
		d = Hash_Find (dl_2, (*ds)->def->name);
		(*ds)->def->initialized = (*ds)->state;

		nds = malloc (sizeof (def_state_t));
		SYS_CHECKMEM (nds);
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
