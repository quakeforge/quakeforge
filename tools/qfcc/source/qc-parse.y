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
	void       *pointer;			// for ensuring pointer values are null
	struct def_s *def;
	struct hashtab_s *def_list;
	struct type_s	*type;
	struct typedef_s *typename;
	struct expr_s	*expr;
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
	struct symbol_s *symbol;
	struct symtab_s *symtab;
}

%nonassoc IFX
%nonassoc ELSE
%nonassoc BREAK_PRIMARY
%nonassoc ';'
%nonassoc CLASS_NOT_CATEGORY

%nonassoc STORAGEX

%right	<op>	'=' ASX PAS /* pointer assign */
%right			'?' ':'
%left			OR
%left			AND
%left			'|'
%left			'^'
%left			'&'
%left			EQ NE
%left			LE GE LT GT
%left			SHL SHR
%left			'+' '-'
%left			'*' '/' '%'
%right	<op>	UNARY INCOP
%left			HYPERUNARY
%left			'.' '(' '['

%token	<symbol>	CLASS_NAME NAME
%token	<expr>		CONST STRING

%token				LOCAL RETURN WHILE DO IF ELSE FOR BREAK CONTINUE ELLIPSIS
%token				NIL IFBE IFB IFAE IFA SWITCH CASE DEFAULT ENUM TYPEDEF
%token				ARGS EXTERN STATIC SYSTEM SIZEOF OVERLOAD
%token	<op>		STRUCT
%token	<type>		TYPE
%token	<symbol>	TYPE_NAME
%token				CLASS DEFS ENCODE END IMPLEMENTATION INTERFACE PRIVATE
%token				PROTECTED PROTOCOL PUBLIC SELECTOR REFERENCE SELF THIS

%type	<symbol>	tag opt_tag
%type	<type>		struct_def struct_def_item struct_def_list struct_spec
%type	<type>		enum_spec
%type	<symbol>	opt_enum_list enum_list enum enum_init

%type	<expr>		const string

%type	<type>		type non_field_type type_name def simple_def
%type	<type>		ivar_decl ivar_declarator def_item def_list
%type	<type>		ivars func_type non_func_type
%type	<type>		code_func func_defs func_def_list
%type	<def>		fdef_name cfunction_def func_def
%type	<param>		function_decl
%type	<param>		param param_list
%type	<def>		opt_initializer methoddef var_initializer
%type	<expr>		opt_expr fexpr expr element_list element_list1 element
%type	<expr>		opt_state_expr think opt_step array_decl texpr
%type	<expr>		statement statements statement_block
%type	<expr>		label break_label continue_label
%type	<expr>		unary_expr primary cast_expr opt_arg_list arg_list
%type	<switch_block> switch_block
%type	<symbol>	def_name identifier

%type	<expr>		identifier_list
%type	<symbol>	selector reserved_word
%type	<param>		optparmlist unaryselector keyworddecl keywordselector
%type	<method>	methodproto methoddecl
%type	<expr>		obj_expr obj_messageexpr obj_string receiver
%type	<protocol_list>	protocolrefs protocol_list
%type	<keywordarg> messageargs keywordarg keywordarglist selectorarg
%type	<keywordarg> keywordnamelist keywordname
%type	<class>		class_name new_class_name class_with_super
%type	<class>		classdef new_class_with_super
%type	<category>	category_name new_category_name
%type	<protocol>	protocol_name
%type	<methodlist> methodprotolist methodprotolist2
%type	<symtab>	ivar_decl_list
%type	<op>		ci

%{

function_t *current_func;
param_t    *current_params;
expr_t     *current_init;
class_type_t *current_class;
expr_t     *local_expr;
vis_e       current_visibility;
storage_class_t current_storage = st_global;
symtab_t   *current_symtab;

static switch_block_t *switch_block;
static expr_t *break_label;
static expr_t *continue_label;

/*	When defining a new symbol, already existing symbols must be in a
	different scope. However, when they are in a different scope, we want a
	truly new symbol.
*/
static symbol_t *
check_redefined (symbol_t *sym)
{
	if (sym->table == current_symtab)
		error (0, "%s redefined", sym->name);
	if (sym->table)		// truly new symbols are not in any symbol table
		sym = new_symbol (sym->name);
	return sym;
}

/*	Check for undefined symbols. If the symbol is undefined, default its type
	to float or int, depending on compiler mode.
*/
static symbol_t *
check_undefined (symbol_t *sym)
{
	if (!sym->table) {	// truly new symbols are not in any symbol table
		error (0, "%s undefined", sym->name);
		if (options.code.progsversion == PROG_ID_VERSION)
			sym->type = &type_float;
		else
			sym->type = &type_integer;
	}
	return sym;
}

%}

%expect 2

%%

defs
	: /* empty */
	| defs def
	| defs obj_def
	| error END { current_class = 0; yyerrok; }
	| error ';' { yyerrok; }
	| error '}' { yyerrok; }
	;

def
	: simple_def				{ }
	| storage_class simple_def	{ current_storage = st_global; }
	| storage_class '{' simple_defs '}' ';'
	  { current_storage = st_global; }
	| TYPEDEF type identifier ';'
		{
			$3 = check_redefined ($3);
			$3->type = $2;
			$3->sy_type = sy_type;
			symtab_addsymbol (current_symtab, $3);
		}
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
	: non_func_type def_list ';'
	| func_type func_defs
	| cfunction								{}
	;

cfunction
	: cfunction_def ';'
	| cfunction_def '=' '#' fexpr ';'
	| cfunction_def opt_state_expr statement_block
	;

cfunction_def
	: OVERLOAD non_func_type identifier function_decl
		{
		}
	| non_func_type identifier function_decl
		{
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
			$3 = check_undefined ($3);
			if (!$3->type || !is_class ($3->type)) {
				error (0, "`%s' is not a class", $3->name);
			} else {
				// replace the struct symbol table with one built from
				// the class ivars and the current struct fields. ivars
				// will replace any fields of the same name.
				current_symtab = class_to_struct ($3->type->t.class,
												  current_symtab);
			}
		}
	;

struct_def
	: type { $<type>$ = $1; } struct_def_list { $$ = $<type>2; }
	;

struct_def_list
	: struct_def_list ',' { $<type>$ = $<type>0; } struct_def_item
		{ (void) ($<type>3); }
	| struct_def_item
	;

struct_def_item
	: identifier
		{
			$1 = check_redefined ($1);
			$1->type = $<type>0;
			symtab_addsymbol (current_symtab, $1);
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
				$$ = array_type ($1, $2->e.value.v.integer_val);
			else
				$$ = pointer_type ($1);
		}
	| type_name					{ $$ = $1; }
	| struct_spec
	| enum_spec
	;

type_name
	: TYPE						{ $$ = $1; }
	| TYPE_NAME					{ $$ = $1->type; }
	| CLASS_NAME				{ $$ = $1->type; }
	;

opt_tag
	: tag
	| /* empty */				{ $$ = 0; }
	;

tag : NAME ;

struct_spec
	: STRUCT opt_tag '{'
		{
			current_symtab = new_symtab (current_symtab, stab_local);
		}
	  struct_defs '}'
		{
			symtab_t   *symtab = current_symtab;
			current_symtab = symtab->parent;

			$$ = build_struct ($1, $2, symtab, 0)->type;
		}
	| STRUCT tag ';'
		{
			$$ = find_struct ($1, $2, 0)->type;
		}
	;

enum_spec
	: ENUM tag opt_enum_list	{ $$ = $3->type; }
	| ENUM '{' enum_init enum_list opt_comma '}'	{ $$ = $4->type; }
	;

opt_enum_list
	: '{' enum_init enum_list opt_comma '}'	{ $$ = $2; }
	| /* empty */				{ $$ = find_enum ($<symbol>0); }
	;

enum_init
	: /* empty */
		{
			$$ = find_enum ($<symbol>-1);
			start_enum ($$);
		}
	;

enum_list
	: enum						{ $$ = $<symbol>0; }
	| enum_list ',' { $<symbol>$ = $<symbol>0; } enum { $$ = $<symbol>0; }
	;

enum
	: identifier				{ add_enum ($<symbol>0, $1, 0); }
	| identifier '=' fexpr		{ add_enum ($<symbol>0, $1, $3); }
	;

function_decl
	: '(' param_list ')'		{ $$ = $2; }
	| '(' param_list ',' ELLIPSIS ')'
		{
			$$ = param_append_identifiers ($2, 0, 0);
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
	: param						{ $$ = $<param>0; }
	| param_list ',' { $<param>$ = $<param>0; } param	{ $$ = $<param>0; }
	;

param
	: type identifier
		{
			$$ = param_append_identifiers ($<param>0, $2, $1);
		}
	;

array_decl
	: '[' fexpr ']'
		{
			$2 = fold_constants ($2);
			if (($2->type != ex_value && $2->e.value.type != ev_integer)
				|| $2->e.value.v.integer_val < 1) {
				error (0, "invalid array size");
				$$ = 0;
			} else {
				$$ = $2;
			}
		}
	| '[' ']'					{ $$ = 0; }
	;

def_list
	: def_list ',' { $<type>$ = $<type>0; } def_item { (void) ($<type>3); }
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
		}
	| OVERLOAD identifier
		{
		}
	;

func_init
	: non_code_func
	| code_func
	;

non_code_func
	: '=' '#' fexpr
	| /* emtpy */
	;

code_func
	: '=' opt_state_expr statement_block	{}
	;

def_name
	: identifier				{ $$ = $1; }
	;

opt_initializer
	: /*empty*/
		{
		}
	| var_initializer			{ $$ = $1; }
	;

var_initializer
	: '=' expr	// don't bother folding twice
		{
		}
	| '=' '{' { } element_list '}'
		{
		}
	;

opt_state_expr
	: /* emtpy */
		{
			$$ = 0;
		}
	| '[' fexpr ',' think opt_step ']'
		{
			$$ = build_state_expr ($2, $4, $5);
		}
	;

think
	: def_name
		{
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
			$<expr>$ = current_init;
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

statement_block
	: '{'
		{
			if (!options.traditional)
				current_symtab = new_symtab (current_symtab, stab_local);
		}
	  statements '}'
		{
			if (!options.traditional)
				current_symtab = current_symtab->parent;
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
	| error ';'					{ $$ = 0; yyerrok; }
	| statement_block			{ $$ = $1; }
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
	| RETURN opt_expr ';'
		{
			$$ = return_expr (current_func, $2);
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
	| SWITCH break_label switch_block '(' fexpr ')' statement_block
		{
			switch_block->test = $5;
			$$ = switch_expr (switch_block, break_label, $7);
			switch_block = $3;
			break_label = $2;
		}
	| IF '(' texpr ')' statement %prec IFX
		{
			$$ = build_if_statement ($3, $5, 0);
		}
	| IF '(' texpr ')' statement ELSE statement
		{
			$$ = build_if_statement ($3, $5, $7);
		}
	| FOR break_label continue_label
			'(' opt_expr ';' opt_expr ';' opt_expr ')' statement
		{
			$$ = build_for_statement ($5, $7, $9, $11,
									  break_label, continue_label);
			break_label = $2;
			continue_label = $3;
		}
	| WHILE break_label continue_label '(' texpr ')' statement
		{
			$$ = build_while_statement ($5, $7, break_label, continue_label);
			break_label = $2;
			continue_label = $3;
		}
	| DO break_label continue_label statement WHILE '(' texpr ')' ';'
		{
			$$ = build_do_while_statement ($4, $7,
										   break_label, continue_label);
			break_label = $2;
			continue_label = $3;
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
	| '*' cast_expr %prec UNARY	{ $$ = pointer_expr ($2); }
	| SIZEOF unary_expr	%prec UNARY	{ $$ = sizeof_expr ($2, 0); }
	| SIZEOF '(' type ')'	 %prec HYPERUNARY	{ $$ = sizeof_expr (0, $3); }
	;

primary
	: NAME      				{ $$ = new_symbol_expr ($1); }
	| BREAK	%prec BREAK_PRIMARY { $$ = new_name_expr (save_string ("break")); }
	| ARGS						{ $$ = new_name_expr (".args"); }
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
	: CONST
	| NIL						{ $$ = new_nil_expr (); }
	| string
	;

string
	: STRING
	| string STRING				{ $$ = binary_expr ('+', $1, $2); }
	;

identifier
	: NAME
	| BREAK
		{
			if (!($$ = symtab_lookup (current_symtab, "break")))
				$$ = new_symbol ("break");
		}
	| CLASS_NAME
	| TYPE_NAME
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
			$$ = append_expr (new_block_expr (), new_symbol_expr ($1));
		}
	| identifier_list ',' identifier
		{
			$$ = append_expr ($1, new_symbol_expr ($3));
		}
	;

classdecl
	: CLASS identifier_list ';'
		{
			expr_t     *e;
			for (e = $2->e.block.head; e; e = e->next)
				get_class (e->e.symbol, 1);
		}
	;

class_name
	: identifier %prec CLASS_NOT_CATEGORY
		{
			$1 = check_undefined ($1);
			if (!is_class ($1->type)) {
				error (0, "`%s' is not a class", $1->name);
				$$ = get_class (0, 1);
			} else {
				$$ = $1->type->t.class;
			}
		}
	;

new_class_name
	: identifier
		{
			$$ = get_class ($1, 0);
			if (!$$) {
				$1 = check_redefined ($1);
				$$ = get_class (0, 1);
			}
			current_class = &$$->class_type;
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
			$$ = get_category ($1, $3->name, 0);
			if (!$$) {
				error (0, "undefined category `%s (%s)'", $1->name, $3->name);
				$$ = get_category (0, 0, 1);
			}
		}
	;

new_category_name
	: identifier '(' identifier ')'
		{
			$$ = get_category ($1, $3->name, 1);
			if ($$->defined) {
				error (0, "redefinition of category `%s (%s)'",
					   $1->name, $3->name);
				$$ = get_category (0, 0, 1);
			}
			current_class = &$$->class_type;
		}
	;

class_reference
	: identifier
		{
			emit_class_ref ($1->name);
		}
	;

category_reference
	: identifier '(' identifier ')'
		{
			emit_category_ref ($1->name, $3->name);
		}
	;


protocol_name
	: identifier
		{
			$$ = get_protocol ($1->name, 0);
			if ($$) {
				error (0, "redefinition of %s", $1->name);
				$$ = get_protocol (0, 1);
			} else {
				$$ = get_protocol ($1->name, 1);
			}
			current_class = &$$->class_type;
		}
	;

classdef
	: INTERFACE new_class_name
	  protocolrefs						{ class_add_protocols ($2, $3); }
	  '{'								{ $<class>$ = $2; }
	  ivar_decl_list '}'
		{
			class_add_ivars ($2, $7);
			$<class>$ = $2;
		}
	  methodprotolist					{ class_add_methods ($2, $10); }
	  END
		{
			current_class = 0;
			(void) ($<class>6);
			(void) ($<class>9);
		}
	| INTERFACE new_class_name
	  protocolrefs					{ class_add_protocols ($2, $3); }
		{
			class_add_ivars ($2, class_new_ivars ($2));
			$<class>$ = $2;
		}
	  methodprotolist					{ class_add_methods ($2, $6); }
	  END
		{
			current_class = 0;
			(void) ($<class>5);
		}
	| INTERFACE new_class_with_super
	  protocolrefs						{ class_add_protocols ($2, $3);}
	  '{'								{ $<class>$ = $2; }
	  ivar_decl_list '}'
		{
			class_add_ivars ($2, $7);
			$<class>$ = $2;
		}
	  methodprotolist					{ class_add_methods ($2, $10); }
	  END
		{
			current_class = 0;
			(void) ($<class>6);
			(void) ($<class>9);
		}
	| INTERFACE new_class_with_super
	  protocolrefs						{ class_add_protocols ($2, $3); }
		{
			class_add_ivars ($2, class_new_ivars ($2));
			$<class>$ = $2;
		}
	  methodprotolist					{ class_add_methods ($2, $6); }
	  END
		{
			current_class = 0;
			(void) ($<class>5);
		}
	| INTERFACE new_category_name
	  protocolrefs
		{
			category_add_protocols ($2, $3);
			$<class>$ = $2->class;
		}
	  methodprotolist					{ category_add_methods ($2, $5); }
	  END
		{
			current_class = 0;
			(void) ($<class>4);
		}
	| IMPLEMENTATION class_name			{ class_begin (&$2->class_type); }
	  '{'								{ $<class>$ = $2; }
	  ivar_decl_list '}'
		{
			class_check_ivars ($2, $6);
			(void) ($<class>5);
		}
	| IMPLEMENTATION class_name			{ class_begin (&$2->class_type); }
	| IMPLEMENTATION class_with_super	{ class_begin (&$2->class_type); }
	  '{'								{ $<class>$ = $2; }
	  ivar_decl_list '}'
		{
			class_check_ivars ($2, $6);
			(void) ($<class>5);
		}
	| IMPLEMENTATION class_with_super	{ class_begin (&$2->class_type); }
	| IMPLEMENTATION category_name		{ class_begin (&$2->class_type); }
	| REFERENCE class_reference ';'		{ }
	| REFERENCE category_reference ';'	{ }
	;

protocoldef
	: PROTOCOL protocol_name
	  protocolrefs			{ protocol_add_protocols ($2, $3); $<class>$ = 0; }
	  methodprotolist			{ protocol_add_methods ($2, $5); }
	  END						{ current_class = 0; (void) ($<class>4); }
	;

protocolrefs
	: /* emtpy */				{ $$ = 0; }
	| LT 						{ $<protocol_list>$ = new_protocol_list (); }
	  protocol_list GT			{ $$ = $3; (void) ($<protocol_list>2); }
	;

protocol_list
	: identifier
		{
			$$ = add_protocol ($<protocol_list>0, $1->name);
		}
	| protocol_list ',' identifier
		{
			$$ = add_protocol ($1, $3->name);
		}
	;

ivar_decl_list
	:	/* */
		{
			current_visibility = vis_protected;
			current_symtab = class_new_ivars ($<class>0);
		}
	  ivar_decl_list_2
		{
			$$ = current_symtab;
			current_symtab = $$->parent;

			build_struct ('s', 0, $$, 0);
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
	: type { $<type>$ = $1; } ivars	{ (void) ($<type>2); }
	;

ivars
	: ivar_declarator
	| ivars ',' { $<type>$ = $<type>0; } ivar_declarator { (void) ($<type>3); }
	;

ivar_declarator
	: identifier
		{
			$1 = check_redefined ($1);
			$1->type = $<type>0;
			$1->visibility = current_visibility;
			symtab_addsymbol (current_symtab, $1);
		}
	;

methoddef
	: ci methoddecl opt_state_expr statement_block	{}
	| ci methoddecl '=' '#' const ';'				{}
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
	: selector					{ $$ = new_param ($1->name, 0, 0); }
	;

keywordselector
	: keyworddecl
	| keywordselector keyworddecl { $2->next = $1; $$ = $2; }
	;

selector
	: NAME						{ $$ = $1; }
	| CLASS_NAME				{ $$ = $1; }
	| TYPE						{ $$ = new_symbol (yytext); }
	| TYPE_NAME					{ $$ = $1; }
	| reserved_word
	;

reserved_word
	: LOCAL						{ $$ = new_symbol (yytext); }
	| RETURN					{ $$ = new_symbol (yytext); }
	| WHILE						{ $$ = new_symbol (yytext); }
	| DO						{ $$ = new_symbol (yytext); }
	| IF						{ $$ = new_symbol (yytext); }
	| ELSE						{ $$ = new_symbol (yytext); }
	| FOR						{ $$ = new_symbol (yytext); }
	| BREAK						{ $$ = new_symbol (yytext); }
	| CONTINUE					{ $$ = new_symbol (yytext); }
	| SWITCH					{ $$ = new_symbol (yytext); }
	| CASE						{ $$ = new_symbol (yytext); }
	| DEFAULT					{ $$ = new_symbol (yytext); }
	| NIL						{ $$ = new_symbol (yytext); }
	| STRUCT					{ $$ = new_symbol (yytext); }
	| ENUM						{ $$ = new_symbol (yytext); }
	| TYPEDEF					{ $$ = new_symbol (yytext); }
	;

keyworddecl
	: selector ':' '(' type ')' identifier
		{ $$ = new_param ($1->name, $4, $6->name); }
	| selector ':' identifier
		{ $$ = new_param ($1->name, &type_id, $3->name); }
	| ':' '(' type ')' identifier
		{ $$ = new_param ("", $3, $5->name); }
	| ':' identifier
		{ $$ = new_param ("", &type_id, $2->name); }
	;

obj_expr
	: obj_messageexpr
	| SELECTOR '(' selectorarg ')'	{ $$ = selector_expr ($3); }
	| PROTOCOL '(' identifier ')'	{ $$ = protocol_expr ($3->name); }
	| ENCODE '(' type ')'			{ $$ = encode_expr ($3); }
	| obj_string /* FIXME string object? */
	;

obj_messageexpr
	: '[' receiver messageargs ']'	{ $$ = message_expr ($2, $3); }
	;

receiver
	: fexpr
	| CLASS_NAME				{ $$ = new_symbol_expr ($1); }
	;

messageargs
	: selector					{ $$ = new_keywordarg ($1->name, 0); }
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
	: selector ':' arg_list		{ $$ = new_keywordarg ($1->name, $3); }
	| ':' arg_list				{ $$ = new_keywordarg ("", $2); }
	;

selectorarg
	: selector					{ $$ = new_keywordarg ($1->name, 0); }
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
	: selector ':'		{ $$ = new_keywordarg ($1->name, new_nil_expr ()); }
	| ':'				{ $$ = new_keywordarg ("", new_nil_expr ()); }
	;

obj_string
	: '@' STRING
		{
			//FIXME string object
			$$ = $2;
		}
	;

%%
