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

#include "qfcc.h"
#include <QF/hash.h>
#include <QF/sys.h>

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

int yylex (void);
type_t *PR_FindType (type_t *new);

type_t *parse_params (type_t *type, def_t *parms, int elipsis);
function_t *new_function (void);
void build_function (function_t *f);
void finish_function (function_t *f);
void emit_function (function_t *f, expr_t *e);
void build_scope (function_t *f, def_t *func);
type_t *build_type (int is_field, type_t *type);
type_t *build_array_type (type_t *type, int size);

hashtab_t *save_local_inits (def_t *scope);
hashtab_t *merge_local_inits (hashtab_t *dl_1, hashtab_t *dl_2);
void restore_local_inits (hashtab_t *def_list);
void free_local_inits (hashtab_t *def_list);

typedef struct {
	type_t	*type;
	def_t	*scope;
} scope_t;

%}

%union {
	int			op;
	def_t		*scope;
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
	param_t		params;
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

%token	LOCAL RETURN WHILE DO IF ELSE FOR BREAK CONTINUE ELIPSIS NIL
%token	IFBE IFB IFAE IFA
%token	SWITCH CASE DEFAULT STRUCT ENUM TYPEDEF
%token	ELE_START
%token	<type> TYPE

%token	CLASS DEFS ENCODE END IMPLEMENTATION INTERFACE PRIVATE PROTECTED
%token	PROTOCOL PUBLIC SELECTOR

%type	<type>	type type_name
%type	<params> function_decl
%type	<integer_val> array_decl
%type	<def>	param param_list def_name
%type	<def>	def_item def_list
%type	<expr>	const opt_expr expr arg_list element_list element_list1 element
%type	<expr>	string_val opt_state_expr
%type	<expr>	statement statements statement_block
%type	<expr>	break_label continue_label enum_list enum
%type	<function> begin_function
%type	<def_list> save_inits
%type	<switch_block> switch_block
%type	<scope>	param_scope

%expect 1

%{

type_t	*current_type;
def_t	*current_def;
function_t *current_func;
expr_t	*local_expr;
expr_t	*break_label;
expr_t	*continue_label;
switch_block_t *switch_block;
type_t	*struct_type;
expr_t	*current_init;

def_t		*pr_scope;					// the function being parsed, or NULL
string_t	s_file;						// filename for function definition

int      element_flag;

%}

%%

defs
	: /* empty */
	| defs def ';'
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
	| obj_def
	;

struct_defs
	: /* empty */
	| struct_defs struct_def ';'
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
	: '.' type { $$ = build_type (1, $2); }
	| type array_decl { $$ = build_type (0, build_array_type ($1, $2)); }
	| type_name function_decl
		{
			$$ = build_type (0, parse_params ($1, $2.params, $2.elipsis));
		}
	| type_name { $$ = build_type (0, $1); }
	| '(' type ')' { $$ = $2; }
	;

type_name
	: TYPE { $$ = $1; }
	;

function_decl
	: '(' param_scope param_list
		{
			PR_FlushScope (pr_scope, 1);
			pr_scope = $<scope>2;
		}
	  ')'
		{
			$$.params = $3;
			$$.elipsis = 0;
		}
	| '(' param_scope param_list ',' ELIPSIS ')'
		{
			PR_FlushScope (pr_scope, 1);
			pr_scope = $<scope>2;

			$$.params = $3;
			$$.elipsis = 1;
		}
	| '(' ELIPSIS ')'
		{
			$$.params = 0;
			$$.elipsis = 1;
		}
	| '(' ')'
		{
			$$.params = 0;
			$$.elipsis = 0;
		}
	;

param_scope
	: /* empty */
		{
			$$ = pr_scope;
			pr_scope = PR_NewDef (0, 0, 0);
			pr_scope->alloc = &pr_scope->locals;
			*pr_scope->alloc = 0;
		}
	;

param_list
	: param
	| param_list ',' param
		{
			if ($3->next) {
				error (0, "parameter redeclared: %s", $3->name);
				$$ = $1;
			} else {
				$3->next = $1;
				$$ = $3;
			}
		}
	;

param
	: type {current_type = $1;} def_name
		{
			$3->type = $1;
			$$ = $3;
		}
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
	: NAME { new_struct_field (struct_type, current_type, $1); }
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
				expr_t *e = new_expr ();
				e->type = ex_def;
				e->e.def = current_def;
				append_expr (local_expr, assign_expr (e, $2));
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
			if (current_type->type != ev_func) {
				error (0, "%s is not a function", current_def->name);
			} else {
				if ($3->type != ex_integer && $3->type != ex_float) {
					error (0, "invalid constant for = #");
				} else {
					function_t	*f;

					f = new_function ();
					f->builtin = $3->type == ex_integer
								 ? $3->e.integer_val : (int)$3->e.float_val;
					f->def = current_def;
					build_function (f);
					finish_function (f);
				}
			}
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
	  def_name { current_def = $<def>5; } ']'
		{
			if ($2->type == ex_integer)
				convert_int ($2);
			if ($2->type != ex_float)
				error ($2, "invalid type for frame number");
			if ($5->type->type != ev_func)
				error ($2, "invalid type for think");

			$$ = new_expr ();
			$$->type = ex_expr;
			$$->e.expr.op = 's';
			$$->e.expr.e1 = $2;
			$$->e.expr.e2 = new_expr ();
			$$->e.expr.e2->type = ex_def;
			$$->e.expr.e2->e.def = $5;
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
			build_scope ($$, current_def);
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
	| obj_expr					{ /* XXX */ }
	| NAME						{ $$ = new_name_expr ($1); }
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
	| methoddef
	| END
	;

identifier_list
	: NAME { /* XXX */ }
	| identifier_list ',' NAME
	;

classdecl
	: CLASS identifier_list
	;

classdef
	: INTERFACE NAME protocolrefs '{'
	  ivar_decl_list '}'
	  methodprotolist
	  END
	| INTERFACE NAME protocolrefs
	  methodprotolist
	  END
	| INTERFACE NAME ':' NAME protocolrefs '{'
	  ivar_decl_list '}'
	  methodprotolist
	  END
	| INTERFACE NAME ':' NAME protocolrefs
	  methodprotolist
	  END
	| INTERFACE NAME '(' NAME ')' protocolrefs
	  methodprotolist
	  END
	| IMPLEMENTATION NAME '{'
	  ivar_decl_list '}'
	| IMPLEMENTATION NAME
	| IMPLEMENTATION NAME ':' NAME '{'
	  ivar_decl_list '}'
	| IMPLEMENTATION NAME ':' NAME
	| IMPLEMENTATION NAME '(' NAME ')'
	;

protocoldef
	: PROTOCOL NAME protocolrefs
	  methodprotolist END
	;

protocolrefs
	: /* emtpy */
	| '<' identifier_list '>'
	;

ivar_decl_list
	: ivar_decl_list visibility_spec ivar_decls
	| ivar_decls
	;

visibility_spec
	: PRIVATE
	| PROTECTED
	| PUBLIC
	;

ivar_decls
	: /* empty */
	| ivar_decls ivar_decl ';'
	;

ivar_decl
	: type ivars { /* XXX */ }
	;

ivars
	: ivar_declarator
	| ivars ',' ivar_declarator
	;

ivar_declarator
	: NAME { /* XXX */ }
	;

methoddef
	: '+'
	  methoddecl opt_state_expr
	  begin_function statement_block end_function
	| '-'
	  methoddecl opt_state_expr
	  begin_function statement_block end_function
	;

methodprotolist
	: /* emtpy */
	| methodprotolist2
	;

methodprotolist2
	: methodproto
	| methodprotolist2 methodproto
	;

methodproto
	: '+' methoddecl ';'
	| '-' methoddecl ';'
	;

methoddecl
	: '(' type ')' unaryselector
	| unaryselector
	| '(' type ')' keywordselector optparmlist
	| keywordselector optparmlist
	;

optparmlist
	: /* empty */
	| ',' ELIPSIS
	;

unaryselector
	: selector
	;

keywordselector
	: keyworddecl
	| keywordselector keyworddecl
	;

selector
	: NAME { /* XXX */ }
	;

keyworddecl
	: selector ':' '(' type ')' NAME
	| selector ':' NAME
	| ':' '(' type ')' NAME
	| ':' NAME
	;

messageargs
	: selector
	| keywordarglist
	;

keywordarglist
	: keywordarg
	| keywordarglist keywordarg
	;

keywordexpr
	: expr { /* XXX */ }
	;

keywordarg
	: selector ':' keywordexpr
	| ':' keywordexpr
	;

receiver
	: expr { /* XXX */ }
	;

obj_expr
	: obj_messageexpr
	| SELECTOR '(' selectorarg ')'
	| PROTOCOL '(' NAME ')'
	| ENCODE '(' type ')'
	| obj_string
	;

obj_messageexpr
	: '[' receiver messageargs ']'
	;

selectorarg
	: selector
	| keywordnamelist
	;

keywordnamelist
	: keywordname
	| keywordnamelist keywordname
	;

keywordname
	: selector ':'
	| ':'
	;

obj_string
	: '@' STRING_VAL
	| obj_string '@' STRING_VAL
	;

%%

type_t *
parse_params (type_t *type, def_t *parms, int elipsis)
{
	type_t		new;
	def_t		*p;
	int			i;

	memset (&new, 0, sizeof (new));
	new.type = ev_func;
	new.aux_type = type;
	new.num_parms = 0;

	if (elipsis) {
		new.num_parms = -1;			// variable args
	} else if (!parms) {
	} else {
		for (p = parms; p; p = p->next, new.num_parms++)
			;
		if (new.num_parms > MAX_PARMS) {
			error (0, "too many params");
			return current_type;
		}
		i = 1;
		do {
			//puts (parms->name);
			strcpy (pr_parm_names[new.num_parms - i], parms->name);
			new.parm_types[new.num_parms - i] = parms->type;
			i++;
			parms = parms->next;
		} while (parms);
	}
	//print_type (&new); puts("");
	return PR_FindType (&new);
}

void
build_scope (function_t *f, def_t *func)
{
	int i;
	def_t *def;
	type_t *ftype = func->type;

	func->alloc = &func->locals;

	for (i = 0; i < ftype->num_parms; i++) {
		def = PR_GetDef (ftype->parm_types[i], pr_parm_names[i], func, func->alloc);
		f->parm_ofs[i] = def->ofs;
		if (i > 0 && f->parm_ofs[i] < f->parm_ofs[i - 1])
			Error ("bad parm order");
		def->used = 1;				// don't warn for unused params
		PR_DefInitialized (def);	// params are assumed to be initialized
	}
}

type_t *
build_type (int is_field, type_t *type)
{
	if (is_field) {
		type_t new;
		memset (&new, 0, sizeof (new));
		new.type = ev_field;
		new.aux_type = type;
		return PR_FindType (&new);
	} else {
		return type;
	}
}

type_t *
build_array_type (type_t *type, int size)
{
	type_t      new;

	memset (&new, 0, sizeof (new));
	new.type = ev_pointer;
	new.aux_type = type;
	new.num_parms = size;
	return PR_FindType (&new);
}

function_t *
new_function (void)
{
	function_t	*f;

	f = calloc (1, sizeof (function_t));
	f->next = pr_functions;
	pr_functions = f;

	return f;
}

void
build_function (function_t *f)
{
	f->def->constant = 1;
	f->def->initialized = 1;
	G_FUNCTION (f->def->ofs) = numfunctions;
}

void
finish_function (function_t *f)
{
	dfunction_t *df;
	int i;

	// fill in the dfunction
	df = &functions[numfunctions];
	numfunctions++;
	f->dfunc = df;

	if (f->builtin)
		df->first_statement = -f->builtin;
	else
		df->first_statement = f->code;

	df->s_name = ReuseString (f->def->name);
	df->s_file = s_file;
	df->numparms = f->def->type->num_parms;
	df->locals = f->def->locals;
	df->parm_start = 0;
	for (i = 0; i < df->numparms; i++)
		df->parm_size[i] = pr_type_size[f->def->type->parm_types[i]->type];

	if (f->aux) {
		def_t *def;
		f->aux->function = df - functions;
		for (def = f->def->scope_next; def; def = def->scope_next) {
			if (def->name) {
				ddef_t *d = new_local ();
				d->type = def->type->type;
				d->ofs = def->ofs;
				d->s_name = ReuseString (def->name);

				f->aux->num_locals++;
			}
		}
	}
}

void
emit_function (function_t *f, expr_t *e)
{
	//printf (" %s =\n", f->def->name);

	if (f->aux)
		lineno_base = f->aux->source_line;

	pr_scope = f->def;
	while (e) {
		//printf ("%d ", pr_source_line);
		//print_expr (e);
		//puts("");

		emit_expr (e);
		e = e->next;
	}
	emit_statement (pr_source_line, op_done, 0, 0, 0);
	PR_FlushScope (pr_scope, 0);
	pr_scope = 0;
	PR_ResetTempDefs ();

	//puts ("");
}

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
			if (!ds)
				Sys_Error ("scan_scope: Memory Allocation Failure\n");
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
		if (!nds)
			Sys_Error ("merge_local_inits: Memory Allocation Failure\n");
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
