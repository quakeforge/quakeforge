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
#include "diagnostic.h"
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "immediate.h"
#include "method.h"
#include "options.h"
#include "qfcc.h"
#include "reloc.h"
#include "strpool.h"
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
	int         size;
	specifier_t spec;
	void       *pointer;			// for ensuring pointer values are null
	struct hashtab_s *def_list;
	struct type_s	*type;
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

%left			COMMA
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

%type	<symbol>	type_name_or_class_name
%type	<spec>		optional_specifiers specifiers storage_class type
%type	<spec>		type_specifier type_specifier_or_storage_class

%type	<param>		function_params var_list param_declaration
%type	<symbol>	var_decl function_decl
%type	<symbol>	abstract_decl abs_decl

%type	<symbol>	tag optional_tag
%type	<spec>		struct_specifier struct_def struct_decl
%type	<spec>		enum_specifier
%type	<symbol>	optional_enum_list enum_list enumerator_list enumerator
%type	<symbol>	enum_init
%type	<size>		array_decl

%type	<spec>		func_def_list comma

%type	<expr>		const string

%type	<spec>		ivar_decl def_item def_list
%type	<spec>		ivars
%type	<param>		param param_list
%type	<symbol>	methoddef
%type	<expr>		opt_initializer var_initializer
%type	<expr>		opt_expr fexpr expr element_list element
%type	<expr>		optional_state_expr think opt_step texpr
%type	<expr>		statement statements compound_statement
%type	<expr>		label break_label continue_label
%type	<expr>		unary_expr primary cast_expr opt_arg_list arg_list
%type	<switch_block> switch_block
%type	<symbol>	identifier

%type	<symbol>	overloaded_identifier

%type	<expr>		identifier_list
%type	<symbol>	selector reserved_word
%type	<param>		optional_param_list unaryselector keyworddecl
%type	<param>		keywordselector
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

static specifier_t
make_spec (type_t *type, storage_class_t storage, int is_typedef,
		   int is_overload)
{
	specifier_t spec;

	memset (&spec, 0, sizeof (spec));
	spec.type = type;
	spec.storage = storage;
	spec.is_typedef = is_typedef;
	spec.is_overload = is_overload;
	if (spec.storage && spec.is_typedef)
		internal_error (0, "setting both storage and is_typedef");
	return spec;
}

static specifier_t
spec_merge (specifier_t spec, specifier_t new)
{
	if (new.type) {
		if (spec.type && !spec.multi_type) {
			error (0, "two or more data types in declaration specifiers");
			spec.multi_type = 1;
		}
		spec.type = new.type;
	}
	if (new.is_typedef || new.storage) {
		if ((spec.is_typedef || spec.storage) && !spec.multi_store) {
			error (0, "multiple storage classes in declaration specifiers");
			spec.multi_store = 1;
		}
		spec.storage = new.storage;
		spec.is_typedef = new.is_typedef;
	}
	spec.is_overload |= new.is_overload;
	return spec;
}

%}

%expect 0

%%

external_def_list
	: /* empty */
		{
			current_symtab = pr.symtab;
			current_storage = st_global;
		}
	| external_def_list external_def
	| external_def_list obj_def
	| error END { current_class = 0; yyerrok; current_symtab = pr.symtab; }
	| error ';' { yyerrok; current_symtab = pr.symtab; }
	| error '}' { yyerrok; current_symtab = pr.symtab; }
	;

external_def
	: optional_specifiers external_decl_list ';' { }
	| optional_specifiers ';' { }
	| optional_specifiers function_params
		{
			$<spec>$ = $1;		// copy spec bits and storage
			$<spec>$.type = parse_params ($1.type, $2), st_global, 0;
			$<spec>$.type = find_type ($<spec>$.type);
		}
	  function_def_list
	| optional_specifiers function_decl function_body
	| storage_class '{'						{ current_storage = $1.storage; }
	  external_def_list '}' ';'				{ current_storage = st_global; }
	;

function_body
	: optional_state_expr
		{
			symbol_t   *sym = $<symbol>0;

			sym->type = find_type (append_type (sym->type, $<spec>-1.type));
			sym = function_symbol (sym, $<spec>-1.is_overload, 1);
			$<symtab>$ = current_symtab;
			current_func = begin_function (sym, 0, current_symtab);
			current_symtab = current_func->symtab;
		}
	  compound_statement
		{
			build_code_function ($<symbol>0, $1, $3);
			current_symtab = $<symtab>2;
		}
	| '=' '#' expr ';'
		{
			symbol_t   *sym = $<symbol>0;

			sym->type = find_type (append_type (sym->type, $<spec>-1.type));
			sym = function_symbol (sym, $<spec>-1.is_overload, 1);
			build_builtin_function (sym, $3);
		}
	;

external_decl_list
	: external_decl
	| external_decl_list ',' external_decl
	;

external_decl
	: var_decl
		{
			specifier_t spec = $<spec>0;

			$1->type = find_type (append_type ($1->type, $<spec>0.type));
			if (spec.is_typedef) {
				$1->sy_type = sy_type;
			} else {
			}
			symtab_addsymbol (current_symtab, $1);
		}
	| var_decl '=' var_initializer
	| function_decl
		{
			specifier_t spec = $<spec>0;
			$1->type = find_type (append_type ($1->type, spec.type));
			$1 = function_symbol ($1, spec.is_overload, 1);
			make_function ($1, 0, spec.storage);
		}
	;

storage_class
	: EXTERN					{ $$ = make_spec (0, st_extern, 0, 0); }
	| STATIC					{ $$ = make_spec (0, st_static, 0, 0); }
	| SYSTEM					{ $$ = make_spec (0, st_system, 0, 0); }
	| TYPEDEF					{ $$ = make_spec (0, st_global, 1, 0); }
	| OVERLOAD					{ $$ = make_spec (0, current_storage, 0, 1); }
	;

optional_specifiers
	: storage_class type_name_or_class_name
		{
			$$ = make_spec ($2->type, current_storage, 0, 0);
			$$ = spec_merge ($1, $$);
		}
	| type_name_or_class_name
		{
			$$ = make_spec ($1->type, current_storage, 0, 0);
		}
	| specifiers
	| /* empty */
		{
			$$ = make_spec (0, current_storage, 0, 0);
		}
	;

type_name_or_class_name
	: TYPE_NAME
	| CLASS_NAME
	;

specifiers
	: type_specifier_or_storage_class
	| specifiers type_specifier_or_storage_class
		{
			$$ = spec_merge ($1, $2);
		}
	;

type
	: type_specifier
	| type type_specifier
	;

type_specifier_or_storage_class
	: type_specifier
	| storage_class
	;

type_specifier
	: TYPE
		{
			$$ = make_spec ($1, current_storage, 0, 0);
		}
	| enum_specifier
	| struct_specifier
	// NOTE: fields don't parse the way they should
	| '.' type_specifier
		{
			$$ = make_spec (field_type ($2.type), current_storage, 0, 0);
		}
	;

optional_tag
	: tag
	| /* empty */				{ $$ = 0; }
	;

tag : NAME ;

enum_specifier
	: ENUM tag optional_enum_list
		{
			$$ = make_spec ($3->type, 0, 0, 0);
			if (!$3->table)
				symtab_addsymbol (current_symtab, $3);
		}
	| ENUM enum_list
		{
			$$ = make_spec ($2->type, 0, 0, 0);
			if (!$2->table)
				symtab_addsymbol (current_symtab, $2);
		}
	;

optional_enum_list
	: '{' enum_init enumerator_list optional_comma '}'	{ $$ = $2; }
	| /* empty */				{ $$ = find_enum ($<symbol>0); }
	;

enum_list
	: '{' enum_init enumerator_list optional_comma '}'	{ $$ = $3; }
	;

enum_init
	: /* empty */
		{
			$$ = find_enum ($<symbol>-1);
			start_enum ($$);
		}
	;

enumerator_list
	: enumerator							{ $$ = $<symbol>0; }
	| enumerator_list ','					{ $<symbol>$ = $<symbol>0; }
	  enumerator							{ $$ = $<symbol>0; }
	;

enumerator
	: identifier				{ add_enum ($<symbol>0, $1, 0); }
	| identifier '=' fexpr		{ add_enum ($<symbol>0, $1, $3); }
	;

struct_specifier
	: STRUCT optional_tag '{'
		{
			current_symtab = new_symtab (current_symtab, stab_local);
		}
	  struct_defs '}'
		{
			symbol_t   *sym;
			symtab_t   *symtab = current_symtab;
			current_symtab = symtab->parent;

			sym = build_struct ($1, $2, symtab, 0);
			$$ = make_spec (sym->type, 0, 0, 0);
			if (!sym->table)
				symtab_addsymbol (current_symtab, sym);
		}
	| STRUCT tag
		{
			symbol_t   *sym;

			sym = find_struct ($1, $2, 0);
			$$ = make_spec (sym->type, 0, 0, 0);
			if (!sym->table)
				symtab_addsymbol (current_symtab, sym);
		}
	;

struct_defs
	: /* empty */	//FIXME for new syntax
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
	: type struct_decl_list
	| type
	;

struct_decl_list
	: struct_decl_list ',' { $<spec>$ = $<spec>0; } struct_decl
	| struct_decl
	;

struct_decl
	: function_decl
		{
			$1->type = append_type ($1->type, $<spec>0.type);
			$1->type = find_type ($1->type);
			symtab_addsymbol (current_symtab, $1);
		}
	| var_decl
		{
			$1->type = append_type ($1->type, $<spec>0.type);
			$1->type = find_type ($1->type);
			symtab_addsymbol (current_symtab, $1);
		}
	| var_decl ':' expr		%prec COMMA		{}
	| ':' expr				%prec COMMA		{}
	;

//FIXME function overloading
var_decl
	: NAME			%prec COMMA
		{
			$$ = check_redefined ($1);
			$$->type = 0;
		}
	| var_decl function_params
		{
			$$->type = append_type ($$->type, parse_params (0, $2));
			print_type ($$->type);
		}
	| var_decl array_decl
		{
			$$->type = append_type ($$->type, array_type (0, $2));
		}
	| '*' var_decl	%prec UNARY
		{
			$$ = $2;
			$$->type = append_type ($$->type, pointer_type (0));
		}
	| '(' var_decl ')'						{ $$ = $2; }
	;

//FIXME function overloading
function_decl
	: '*' function_decl
		{
			$$ = $2;
			$$->type = append_type ($$->type, pointer_type (0));
		}
	| function_decl array_decl
		{
			$$ = $1;
			$$->type = append_type ($$->type, array_type (0, $2));
		}
	| '(' function_decl ')'					{ $$ = $2; }
	| function_decl function_params
		{
			$$ = $1;
			$$->params = $2;
			$$->type = append_type ($$->type, parse_params (0, $2));
		}
	| NAME function_params
		{
			$$ = $1;
			$$->params = $2;
			$$->type = parse_params (0, $2);
		}
	;

function_params
	: '(' ')'								{ $$ = 0; }
	| '(' ps var_list ')'					{ $$ = check_params ($3); }
	;

ps : ;

var_list
	: param_declaration
	| var_list ',' param_declaration
		{
			param_t    *p;

			for (p = $1; p->next; p = p->next)
				;
			p->next = $3;
			$$ = $1;
		}
	;

param_declaration
	: type var_decl
		{
			$2->type = find_type (append_type ($2->type, $1.type));
			$$ = new_param (0, $2->type, $2->name);
		}
	| type_name_or_class_name var_decl
		{
			$2->type = find_type (append_type ($2->type, $1->type));
			$$ = new_param (0, $2->type, $2->name);
		}
	| abstract_decl			{ $$ = new_param (0, $1->type, 0); }
	| ELLIPSIS				{ $$ = new_param (0, 0, 0); }
	;

abstract_decl
	: type abs_decl
		{
			$$ = $2;
			$$->type = find_type (append_type ($$->type, $1.type));
		}
	| type_name_or_class_name abs_decl
		{
			$$ = $2;
			$$->type = find_type (append_type ($$->type, $1->type));
		}
	;

//FIXME type construction is inside-out
abs_decl
	: /* empty */			{ $$ = new_symbol (""); }
	| '(' abs_decl ')' function_params
		{
			$$ = $2;
			$$->type = append_type ($$->type, parse_params (0, $4)); 
		}
	| '*' abs_decl
		{
			$$ = $2;
			$$->type = append_type ($$->type, pointer_type (0));
		}
	| abs_decl array_decl
		{
			$$ = $1;
			$$->type = append_type ($$->type, array_type (0, $2));
		}
	| '(' abs_decl ')'
		{
			$$ = $2;
		}
	;

array_decl
	: '[' fexpr ']'
		{
			$2 = fold_constants ($2);
			if (!is_integer_val ($2) || expr_integer ($2) < 1) {
				error (0, "invalid array size");
				$$ = 0;
			} else {
				$$ = expr_integer ($2);
			}
		}
	| '[' ']'					{ $$ = 0; }
	;

local_storage_class
	: LOCAL						{ current_storage = st_local; }
	| %prec STORAGEX			{ current_storage = st_local; }
	| STATIC					{ current_storage = st_static; }
	;
/*
param_scope
	: / * empty * /
		{
			$$ = current_symtab;
			current_symtab = new_symtab (current_symtab, stab_local);
		}
	;
*/
param_list
	: param						{ $$ = $1; }
	| param_list ',' { $<param>$ = $<param>1; } param	{ $$ = $4; }
	;

param
	: type identifier
		{
			$2 = check_redefined ($2);
			$$ = param_append_identifiers ($<param>0, $2, $1.type);
		}
	;

def_list
	: def_list ',' { $<spec>$ = $<spec>0; } def_item
	| def_item
	;

def_item
	: identifier opt_initializer
		{
			initialize_def ($1, $<spec>0.type, $2, current_symtab->space,
							$<spec>0.storage);
		}
	;

function_def_list
	: func_def_list comma func_def_list_term
	| func_def_list_term
	;

func_def_list
	: func_def_list comma func_def			{ $$ = $2; }
	| func_def								{ $$ = $<spec>0; }
	;

/*	This rule is used only to get an action before both func_def and
	func_def_list_term so that the function spec can be inherited.
*/
comma: ','		{ $$ = $<spec>0; }

func_def_list_term
	: non_code_funcion ';'
	| code_function ';'
	| code_function %prec IFX
	;

func_def
	: non_code_funcion
	| code_function
	;

code_function
	: overloaded_identifier code_func
	;

non_code_funcion
	: overloaded_identifier non_code_func
	;

overloaded_identifier
	: identifier
		{
			$$ = $1;
			$$->params = current_params;
			$$->type = $<spec>0.type;
			$$ = function_symbol ($$, $<spec>0.is_overload, 1);
		}
	;

non_code_func
	: '=' '#' fexpr
		{
			build_builtin_function ($<symbol>0, $3);
		}
	| /* emtpy */
		{
			make_function ($<symbol>0, 0, current_storage);
		}
	;

code_func
	: '='
		{
			$<symtab>$ = current_symtab;
			current_func = begin_function ($<symbol>0, 0, current_symtab);
			current_symtab = current_func->symtab;
		}
	  optional_state_expr compound_statement
		{
			build_code_function ($<symbol>0, $3, $4);
			current_symtab = $<symtab>2;
		}
	;

opt_initializer
	: /*empty*/									{ $$ = 0; }
	| var_initializer							{ $$ = $1; }
	;

var_initializer
	: '=' fexpr									{ $$ = $2; }
	| '=' '{' element_list optional_comma '}'	{ $$ = $3; }
	;

optional_state_expr
	: /* emtpy */						{ $$ = 0; }
	| '[' fexpr ',' think opt_step ']'	{ $$ = build_state_expr ($2, $4, $5); }
	;

think
	: identifier
		{
			internal_error (0, "FIXME");
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
	: element
		{
			$$ = new_block_expr ();
			append_expr ($$, $1);
		}
	| element_list ',' element
		{
			append_expr ($$, $3);
		}
	;

element
	: '{' element_list optional_comma '}'		{ $$ = $2; }
	| fexpr									{ $$ = $1; }
	;

optional_comma
	: /* empty */
	| ','
	;

compound_statement
	: '{'
		{
			if (!options.traditional) {
				current_symtab = new_symtab (current_symtab, stab_local);
				current_symtab->space = current_symtab->parent->space;
			}
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
	| compound_statement		{ $$ = $1; }
	| local_storage_class type
		{
			$<spec>$ = $2;
			local_expr = new_block_expr ();
		}
	  def_list ';'
		{
			$$ = local_expr;
			local_expr = 0;
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
	| SWITCH break_label switch_block '(' fexpr ')' compound_statement
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
	| SIZEOF '(' abstract_decl ')'	%prec HYPERUNARY
		{
			$$ = sizeof_expr (0, $3->type);
		}
	;

primary
	: NAME      				{ $$ = new_symbol_expr (check_undefined ($1)); }
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
	| '(' abstract_decl ')' cast_expr %prec UNARY
		{
			$$ = cast_expr ($2->type, $4);
		}
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
			for (e = $2->e.block.head; e; e = e->next) {
				get_class (e->e.symbol, 1);
				if (!e->e.symbol->table)
					symtab_addsymbol (current_symtab, e->e.symbol);
			}
		}
	;

class_name
	: identifier %prec CLASS_NOT_CATEGORY
		{
			$1 = check_undefined ($1);
			if (!$1->type || !is_class ($1->type)) {
				error (0, "`%s' is not a class %p", $1->name, $1->type);
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
				$$ = get_class ($1, 1);
			}
			current_class = &$$->class_type;
			if (!$1->table)
				symtab_addsymbol (current_symtab, $1);
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
	: /* empty */
		{
			symtab_t   *tab, *ivars;
			ivars = class_new_ivars ($<class>0);
			for (tab = ivars; tab->parent; tab = tab->parent)
				;
			$<symtab>$ = tab;
			tab->parent = current_symtab;
			current_symtab = ivars;

			current_visibility = vis_protected;
		}
	  ivar_decl_list_2
		{
			$$ = current_symtab;
			current_symtab = $<symtab>1->parent;
			$<symtab>1->parent = 0;

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
	: type { $<spec>$ = $1; } ivars
	| type_name_or_class_name
		{
			$<spec>$ = make_spec ($1->type, 0, 0, 0);
		}
	  ivars									{}
	;

ivars
	: struct_decl
	| ivars ',' { $<spec>$ = $<spec>0; } struct_decl
	;

methoddef
	: ci methoddecl optional_state_expr
		{
			method_t   *method = $2;

			method->instance = $1;
			$2 = method = class_find_method (current_class, method);
			$<symbol>$ = method_symbol (current_class, method);
		}
		{
			method_t   *method = $2;
			const char *nicename = method_name (method);
			symbol_t   *sym = $<symbol>4;
			$<symtab>$ = current_symtab;
			current_func = begin_function (sym, nicename, current_symtab);
			method->def = sym->s.func->def;
			current_symtab = current_func->symtab;
		}
	  compound_statement
		{
			build_code_function ($<symbol>4, $3, $6);
			current_symtab = $<symtab>5;
		}
	| ci methoddecl '=' '#' const ';'
		{
			symbol_t   *sym;
			method_t   *method = $2;

			method->instance = $1;
			method = class_find_method (current_class, method);
			sym = method_symbol (current_class, method);
			build_builtin_function (sym, $5);
			method->def = sym->s.func->def;
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
	: '(' abstract_decl ')' unaryselector
		{ $$ = new_method ($2->type, $4, 0); }
	| unaryselector
		{ $$ = new_method (&type_id, $1, 0); }
	| '(' abstract_decl ')' keywordselector optional_param_list
		{ $$ = new_method ($2->type, $4, $5); }
	| keywordselector optional_param_list
		{ $$ = new_method (&type_id, $1, $2); }
	;

optional_param_list
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
	: selector ':' '(' abstract_decl ')' identifier
		{ $$ = new_param ($1->name, $4->type, $6->name); }
	| selector ':' identifier
		{ $$ = new_param ($1->name, &type_id, $3->name); }
	| ':' '(' abstract_decl ')' identifier
		{ $$ = new_param ("", $3->type, $5->name); }
	| ':' identifier
		{ $$ = new_param ("", &type_id, $2->name); }
	;

obj_expr
	: obj_messageexpr
	| SELECTOR '(' selectorarg ')'	{ $$ = selector_expr ($3); }
	| PROTOCOL '(' identifier ')'	{ $$ = protocol_expr ($3->name); }
	| ENCODE '(' abstract_decl ')'	{ $$ = encode_expr ($3->type); }
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
