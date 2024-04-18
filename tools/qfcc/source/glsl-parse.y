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

#define first_line line
#define first_column column

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
%left			'*' '/' '%' MOD SCALE GEOMETRIC
%left           HADAMARD CROSS DOT WEDGE REGRESSIVE
%right	<op>	SIZEOF UNARY INCOP REVERSE STAR DUAL
%left			HYPERUNARY
%left			'.' '(' '['

%token	<expr>		VALUE STRING TOKEN
// end of tokens common between qc and qp

%token	<symbol>	CLASS_NAME NAME

%token				LOCAL WHILE DO IF ELSE FOR BREAK CONTINUE
%token				RETURN AT_RETURN ELLIPSIS
%token				NIL GOTO SWITCH CASE DEFAULT ENUM ALGEBRA
%token				ARGS TYPEDEF EXTERN STATIC SYSTEM OVERLOAD NOT ATTRIBUTE
%token	<op>		STRUCT
%token				HANDLE
%token	<spec>		TYPE_SPEC TYPE_NAME TYPE_QUAL
%token	<spec>		OBJECT_NAME
%token				CLASS DEFS ENCODE END IMPLEMENTATION INTERFACE PRIVATE
%token				PROTECTED PROTOCOL PUBLIC SELECTOR REFERENCE SELF THIS

%type	<spec>		storage_class save_storage
%type	<spec>		typespec typespec_reserved typespec_nonreserved
%type	<spec>		handle
%type	<spec>		declspecs declspecs_nosc declspecs_nots
%type	<spec>		declspecs_ts
%type	<spec>		declspecs_nosc_ts declspecs_nosc_nots
%type	<spec>		declspecs_sc_ts declspecs_sc_nots defspecs
%type	<spec>		declarator notype_declarator after_type_declarator
%type	<spec>		param_declarator param_declarator_starttypename
%type	<spec>		param_declarator_nostarttypename
%type	<spec>		absdecl absdecl1 direct_absdecl typename ptr_spec copy_spec

%type	<attribute>	attribute_list attribute

%type	<param>		function_params

%type	<symbol>	tag
%type	<spec>		struct_specifier struct_list
%type	<spec>		enum_specifier algebra_specifier
%type	<symbol>	optional_enum_list enum_list enumerator_list enumerator
%type	<symbol>	enum_init
%type	<size>		array_decl

%type	<expr>		const string

%type   <expr>		decl
%type	<param>		param_list parameter_list parameter
%type	<expr>		var_initializer local_def

%type	<mut_expr>	opt_init_semi opt_expr comma_expr
%type   <expr>		expr
%type	<expr>		compound_init
%type   <mut_expr>	element_list expr_list
%type	<designator> designator designator_spec
%type	<element>	element
%type	<expr>		texpr vector_expr
%type	<expr>		statement
%type	<mut_expr>	statements compound_statement
%type	<expr>		else bool_label break_label continue_label
%type	<expr>		unary_expr ident_expr cast_expr
%type	<mut_expr>	opt_arg_list arg_list
%type   <expr>		arg_expr
%type	<switch_block> switch_block
%type	<symbol>	identifier

%{

static switch_block_t *switch_block;
static const expr_t *break_label;
static const expr_t *continue_label;

static specifier_t
make_spec (const type_t *type, storage_class_t storage, int is_typedef,
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
parse_attributes (attribute_t *attr_list)
{
	specifier_t spec = {};
	for (attribute_t *attr = attr_list; attr; attr = attr->next) {
		if (!strcmp (attr->name, "no_va_list")) {
			spec.no_va_list = 1;
		} else if (!strcmp (attr->name, "nosave")) {
			spec.nosave = 1;
		} else if (!strcmp (attr->name, "void_return")) {
			spec.void_return = 1;
		} else {
			warning (0, "skipping unknown attribute '%s'", attr->name);
		}
	}
	return spec;
}

static int
storage_auto (specifier_t spec)
{
	return spec.storage == sc_global || spec.storage == sc_local;
}

static specifier_t
spec_merge (specifier_t spec, specifier_t new)
{
	if (new.type) {
		// deal with "type <type_name>"
		if (!spec.type || new.sym) {
			spec.sym = new.sym;
			if (!spec.type) {
				spec.type = new.type;
			}
		} else if (!spec.multi_type) {
			error (0, "two or more data types in declaration specifiers");
			spec.multi_type = 1;
		}
	}
	if (new.is_typedef || !storage_auto (new)) {
		if ((spec.is_typedef || !storage_auto (spec)) && !spec.multi_store) {
			error (0, "multiple storage classes in declaration specifiers");
			spec.multi_store = 1;
		}
		spec.storage = new.storage;
		spec.is_typedef = new.is_typedef;
	}
	if ((new.is_unsigned && spec.is_signed)
		|| (new.is_signed && spec.is_unsigned)) {
		if (!spec.multi_type) {
			error (0, "both signed and unsigned in declaration specifiers");
			spec.multi_type = 1;
		}
	}
	if ((new.is_long && spec.is_short) || (new.is_short && spec.is_long)) {
		if (!spec.multi_store) {
			error (0, "both long and short in declaration specifiers");
			spec.multi_store = 1;
		}
	}
	spec.sym = new.sym;
	spec.spec_bits |= new.spec_bits;
	return spec;
}

static specifier_t
typename_spec (specifier_t spec)
{
	spec = default_type (spec, 0);
	spec.sym->type = find_type (append_type (spec.sym->type, spec.type));
	spec.type = spec.sym->type;
	return spec;
}

static specifier_t
function_spec (specifier_t spec, param_t *params)
{
	// empty param list in an abstract decle does not create a symbol
	if (!spec.sym) {
		spec.sym = new_symbol (0);
	}
	spec = default_type (spec, spec.sym);
	spec.sym->params = params;
	spec.sym->type = append_type (spec.sym->type, parse_params (0, params));
	spec.is_function = 1; //FIXME do proper void(*)() -> ev_func
	return spec;
}

static specifier_t
array_spec (specifier_t spec, unsigned size)
{
	spec = default_type (spec, spec.sym);
	spec.sym->type = append_type (spec.sym->type, array_type (0, size));
	return spec;
}

static specifier_t
pointer_spec (specifier_t quals, specifier_t spec)
{
	spec.sym->type = append_type (spec.sym->type, pointer_type (0));
	return spec;
}

static symbol_t *
funtion_sym_type (specifier_t spec, symbol_t *sym)
{
	sym->type = append_type (spec.sym->type, spec.type);
	set_func_type_attrs (sym->type, spec);
	sym->type = find_type (sym->type);
	return sym;
}

static param_t *
make_ellipsis (void)
{
	return new_param (0, 0, 0);
}

static param_t *
make_param (specifier_t spec)
{
	spec = default_type (spec, spec.sym);
	spec.type = find_type (append_type (spec.sym->type, spec.type));
	param_t    *param = new_param (0, spec.type, spec.sym->name);
	return param;
}

static int
is_anonymous_struct (specifier_t spec)
{
	if (spec.sym) {
		return 0;
	}
	if (!is_struct (spec.type) && !is_union (spec.type)) {
		return 0;
	}
	if (!spec.type->t.symtab || spec.type->t.symtab->parent) {
		return 0;
	}
	// struct and union type names always begin with "tag ". Untagged s/u
	// are "tag .<filename>.<id>".
	if (spec.type->name[4] != '.') {
		return 0;
	}
	return 1;
}

static int
is_null_spec (specifier_t spec)
{
	static specifier_t null_spec;
	return memcmp (&spec, &null_spec, sizeof (spec)) == 0;
}

static int
use_type_name (specifier_t spec)
{
	spec.sym = new_symbol (spec.sym->name);
	spec.sym->type = spec.type;
	spec.sym->sy_type = sy_var;
	symbol_t   *s = symtab_addsymbol (current_symtab, spec.sym);
	// a different symbol being returned means that this is a redefinition
	// of that symbol in the same scope. However, typedefs to the same type
	// are allowed.
	if (s != spec.sym && spec.is_typedef && s->sy_type == sy_type
		&& type_same (s->type, spec.type)) {
		spec.sym = s;
	}
	return !!spec.sym->table;
}

static void __attribute__((used))
check_specifiers (specifier_t spec)
{
	if (!is_null_spec (spec)) {
		if (!spec.type && !spec.sym) {
			warning (0, "useless specifiers");
		} else if (spec.type && !spec.sym) {
			if (is_anonymous_struct (spec)){
				warning (0, "unnamed struct/union that defines "
						 "no instances");
			} else if (!is_enum (spec.type)
					   && !is_struct (spec.type) && !is_union (spec.type)) {
				warning (0, "useless type name in empty declaration");
			}
		} else if (!spec.type && spec.sym) {
			bug (0, "wha? %p %p", spec.type, spec.sym);
		} else {
			// a type name (id, typedef, etc) was used as a variable name.
			// this is allowed in C, so long as it's in a different scope,
			// or the types are the same
			if (!use_type_name (spec)) {
				error (0, "%s redeclared as different kind of symbol",
					   spec.sym->name);
			}
		}
	}
}

%}

%expect 0

%%

program
	: external_def_list
		{
			if (current_class) {
				warning (0, "‘@end’ missing in implementation context");
				class_finish (current_class);
				current_class = 0;
			}
		}

external_def_list
	: /* empty */
		{
			current_symtab = pr.symtab;
		}
	| external_def_list external_def
	;

external_def
	: fndef
	| datadef
	| storage_class '{' save_storage
		{
			current_storage = $1.storage;
		}
	  external_def_list '}' ';'
		{
			current_storage = $3.storage;
		}
	;

fndef
	: declspecs_ts declarator function_body
	| declspecs_nots notype_declarator function_body
	| defspecs notype_declarator function_body
	;

datadef
	: defspecs notype_initdecls ';'
	| declspecs_nots notype_initdecls ';'
	| declspecs_ts initdecls ';'
	| declspecs ';'
	| error ';'
	| error '}'
	| ';'
	;

declarator
	: after_type_declarator
	| notype_declarator
	;

after_type_declarator
	: '(' copy_spec after_type_declarator ')' { $$ = $3; }
	| after_type_declarator function_params
		{
			$$ = function_spec ($1, $2);
		}
	| after_type_declarator array_decl
		{
			$$ = array_spec ($1, $2);
		}
	| '*' ptr_spec after_type_declarator
		{
			$$ = pointer_spec ($2, $3);
		}
	| TYPE_NAME
		{
			$$ = $<spec>0;
			$$.sym = new_symbol ($1.sym->name);
		}
	| OBJECT_NAME
		{
			$$ = $<spec>0;
			$$.sym = new_symbol ($1.sym->name);
		}
	;

copy_spec
	: /* empty */
		{
			$<spec>$ = $<spec>-1;
		}
	;

ptr_spec
	: copy_spec	// for when no qualifiers are present
	;

notype_declarator
	: '(' copy_spec notype_declarator ')' { $$ = $3; }
	| notype_declarator function_params
		{
			$$ = function_spec ($1, $2);
		}
	| notype_declarator array_decl
		{
			$$ = array_spec ($1, $2);
		}
	| '*' ptr_spec notype_declarator
		{
			$$ = pointer_spec ($2, $3);
		}
	| NAME
		{
			$$ = $<spec>0;
			$$.sym = new_symbol ($1->name);
		}
	;

initdecls
	: initdecl
	| initdecls ',' { $<spec>$ = $<spec>0; } initdecl
	;

initdecl
	: declarator '=' var_initializer
		{ declare_symbol ($1, $3, current_symtab); }
	| declarator
		{ declare_symbol ($1, 0, current_symtab); }
	;

notype_initdecls
	: notype_initdecl
	| notype_initdecls ',' { $<spec>$ = $<spec>0; } notype_initdecl
	;

notype_initdecl
	: notype_declarator '=' var_initializer
		{ declare_symbol ($1, $3, current_symtab); }
	| notype_declarator
		{ declare_symbol ($1, 0, current_symtab); }
	;

/* various lists of type specifiers, storage class etc */
declspecs_ts
	: declspecs_nosc_ts
	| declspecs_sc_ts
	;

declspecs_nots
	: declspecs_nosc_nots
	| declspecs_sc_nots
	;

declspecs_nosc
	: declspecs_nosc_nots
	| declspecs_nosc_ts
	;

declspecs
	: declspecs_nosc_nots
	| declspecs_nosc_ts
	| declspecs_sc_nots
	| declspecs_sc_ts
	;

declspecs_nosc_nots
	: TYPE_QUAL
	| declspecs_nosc_nots TYPE_QUAL
		{
			$$ = spec_merge ($1, $2);
		}
	;

declspecs_nosc_ts
	: typespec
	| declspecs_nosc_ts TYPE_QUAL
		{
			$$ = spec_merge ($1, $2);
		}
	| declspecs_nosc_ts typespec_reserved
		{
			$$ = spec_merge ($1, $2);
		}
	| declspecs_nosc_nots typespec
		{
			$$ = spec_merge ($1, $2);
		}
	;

declspecs_sc_nots
	: storage_class
	| declspecs_sc_nots TYPE_QUAL
		{
			$$ = spec_merge ($1, $2);
		}
	| declspecs_nosc_nots storage_class
		{
			$$ = spec_merge ($1, $2);
		}
	| declspecs_sc_nots storage_class
		{
			$$ = spec_merge ($1, $2);
		}
	;

declspecs_sc_ts
	: declspecs_sc_ts TYPE_QUAL
		{
			$$ = spec_merge ($1, $2);
		}
	| declspecs_sc_ts typespec_reserved
		{
			$$ = spec_merge ($1, $2);
		}
	| declspecs_sc_nots typespec
		{
			$$ = spec_merge ($1, $2);
		}
	| declspecs_nosc_ts storage_class
		{
			$$ = spec_merge ($1, $2);
		}
	| declspecs_sc_ts storage_class
		{
			$$ = spec_merge ($1, $2);
		}
	;

typespec
	: typespec_reserved
		{
			$$ = $1;
			if (!$$.storage) {
				$$.storage = current_storage;
			}
		}
	| typespec_nonreserved
		{
			$$ = $1;
			if (!$$.storage) {
				$$.storage = current_storage;
			}
		}
	;

typespec_reserved
	: TYPE_SPEC
	| algebra_specifier %prec LOW
	| algebra_specifier '.' attribute
		{
			$$ = make_spec (algebra_subtype ($1.type, $3), 0, 0, 0);
		}
	| enum_specifier
	| struct_specifier
	// NOTE: fields don't parse the way they should. This is not a problem
	// for basic types, but functions need special treatment
	| '.' typespec_reserved
		{
			// avoid find_type()
			$$ = make_spec (field_type (0), 0, 0, 0);
			$$.type = append_type ($$.type, $2.type);
		}
	;

typespec_nonreserved
	: TYPE_NAME %prec LOW
	| TYPE_NAME '.' attribute
		{
			if (!is_algebra ($1.type)) {
				error (0, "%s does not have any subtypes",
					   get_type_string ($1.type));
				$$ = $1;
			} else {
				$$ = make_spec (algebra_subtype ($1.type, $3), 0, 0, 0);
			}
		}
	// NOTE: fields don't parse the way they should. This is not a problem
	// for basic types, but functions need special treatment
	| '.' typespec_nonreserved
		{
			// avoid find_type()
			$$ = make_spec (field_type (0), 0, 0, 0);
			$$.type = append_type ($$.type, $2.type);
		}
	;

defspecs
	: /* empty */
		{
			$$ = (specifier_t) {};
		}
	;

save_storage
	: /* emtpy */
		{
			$$.storage = current_storage;
		}
	;

function_body
	:
		{
			specifier_t spec = default_type ($<spec>0, $<spec>0.sym);
			symbol_t   *sym = funtion_sym_type (spec, spec.sym);
			$<symbol>$ = function_symbol (sym, spec.is_overload, 1);
		}
	  save_storage
		{
			$<symtab>$ = current_symtab;
			current_func = begin_function ($<symbol>1, 0, current_symtab, 0,
										   $<spec>-1.storage);
			current_symtab = current_func->locals;
			current_storage = sc_local;
		}
	  compound_statement
		{
			build_code_function ($<symbol>1, 0, $4);
			current_symtab = $<symtab>3;
			current_storage = $2.storage;
			current_func = 0;
		}
	| '=' '#' expr ';'
		{
			specifier_t spec = default_type ($<spec>0, $<spec>0.sym);
			symbol_t   *sym = funtion_sym_type (spec, spec.sym);
			sym = function_symbol (sym, spec.is_overload, 1);
			build_builtin_function (sym, $3, 0, spec.storage);
		}
	;

storage_class
	: EXTERN					{ $$ = make_spec (0, sc_extern, 0, 0); }
	| STATIC					{ $$ = make_spec (0, sc_static, 0, 0); }
	| SYSTEM					{ $$ = make_spec (0, sc_system, 0, 0); }
	| TYPEDEF					{ $$ = make_spec (0, sc_global, 1, 0); }
	| OVERLOAD					{ $$ = make_spec (0, current_storage, 0, 1); }
	| ATTRIBUTE '(' attribute_list ')'
		{
			$$ = parse_attributes ($3);
		}
	;

attribute_list
	: attribute
	| attribute_list ',' attribute
		{
			if ($3) {
				$3->next = $1;
				$$ = $3;
			} else {
				$$ = $1;
			}
		}
	;

attribute
	: NAME %prec LOW			{ $$ = new_attribute ($1->name, 0); }
	| NAME '(' expr_list ')'	{ $$ = new_attribute ($1->name, $3); }
	;

tag : NAME ;

algebra_specifier
	: ALGEBRA '(' TYPE_SPEC '(' expr_list ')' ')'
		{
			auto spec = make_spec (algebra_type ($3.type, $5), 0, 0, 0);
			$$ = spec;
		}
	| ALGEBRA '(' TYPE_SPEC ')'
		{
			auto spec = make_spec (algebra_type ($3.type, 0), 0, 0, 0);
			$$ = spec;
		}
	;

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
	: enum_list
	| /* empty */				{ $$ = find_enum ($<symbol>0); }
	;

enum_list
	: '{' enum_init enumerator_list optional_comma '}'
		{
			current_symtab = current_symtab->parent;
			$$ = finish_enum ($3);
		}
	;

enum_init
	: /* empty */
		{
			$$ = find_enum ($<symbol>-1);
			start_enum ($$);
			current_symtab = $$->type->t.symtab;
		}
	;

enumerator_list
	: enumerator							{ $$ = $<symbol>0; }
	| enumerator_list ','					{ $<symbol>$ = $<symbol>0; }
	  enumerator
		{
			$$ = $<symbol>0;
		}
	;

enumerator
	: identifier				{ add_enum ($<symbol>0, $1, 0); }
	| identifier '=' expr		{ add_enum ($<symbol>0, $1, $3); }
	;

struct_specifier
	: STRUCT tag struct_list { $$ = $3; }
	| STRUCT {$<symbol>$ = 0;} struct_list { $$ = $3; }
	| STRUCT tag
		{
			symbol_t   *sym;

			sym = find_struct ($1, $2, 0);
			sym->type = find_type (sym->type);
			$$ = make_spec (sym->type, 0, 0, 0);
			if (!sym->table) {
				symtab_t   *tab = current_symtab;
				while (tab->parent && tab->type == stab_struct) {
					tab = tab->parent;
				}
				symtab_addsymbol (tab, sym);
			}
		}
	| handle tag
		{
			specifier_t spec = default_type ($1, 0);
			symbol_t   *sym = find_handle ($2, spec.type);
			sym->type = find_type (sym->type);
			$$ = make_spec (sym->type, 0, 0, 0);
			if (!sym->table) {
				symtab_t   *tab = current_symtab;
				while (tab->parent && tab->type == stab_struct) {
					tab = tab->parent;
				}
				symtab_addsymbol (tab, sym);
			}
		}
	;

handle
	: HANDLE					{ $$ = make_spec (&type_int, 0, 0, 0); }
	| HANDLE '(' TYPE_SPEC ')'	{ $$ = $3; }
	;

struct_list
	: '{'
		{
			int         op = $<op>-1;
			symbol_t   *sym = $<symbol>0;
			current_symtab = start_struct (&op, sym, current_symtab);
			$<op>1 = op;
			$<symbol>$ = sym;
		}
	  struct_defs '}'
		{
			symbol_t   *sym;
			symtab_t   *symtab = current_symtab;
			current_symtab = symtab->parent;

			if ($<op>1) {
				sym = $<symbol>2;
				sym = build_struct ($<op>1, sym, symtab, 0, 0);
				$$ = make_spec (sym->type, 0, 0, 0);
				if (!sym->table)
					symtab_addsymbol (current_symtab, sym);
			}
		}
	;

struct_defs
	: component_decl_list
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

component_decl_list
	: component_decl_list2
	| component_decl_list2 component_decl
		{
			warning (0, "no semicolon at end of struct or union");
		}
	;

component_decl_list2
	: /* empty */
	| component_decl_list2 component_decl ';'
	| component_decl_list2 ';'
	;

component_decl
	: declspecs_nosc_ts components
	| declspecs_nosc_ts
		{
			if (is_anonymous_struct ($1)) {
				// type->name always begins with "tag "
				$1.sym = new_symbol (va (0, ".anonymous.%s",
										 $1.type->name + 4));
				$1.sym->type = $1.type;
				$1.sym->sy_type = sy_var;
				$1.sym->visibility = vis_anonymous;
				symtab_addsymbol (current_symtab, $1.sym);
				if (!$1.sym->table) {
					error (0, "duplicate field `%s'", $1.sym->name);
				}
			}
		}
	| declspecs_nosc_nots components_notype
	| declspecs_nosc_nots { internal_error (0, "not implemented"); }
	;

components
	: component_declarator
	| components ',' { $<spec>$ = $<spec>0; } component_declarator
	;

component_declarator
	: declarator
		{
			declare_field ($1, current_symtab);
		}
	| declarator ':' expr
	| ':' expr
	;

components_notype
	: component_notype_declarator
	| components_notype ',' component_notype_declarator
	;

component_notype_declarator
	: notype_declarator { internal_error (0, "not implemented"); }
	| notype_declarator ':' expr
	| ':' expr
	;

function_params
	: '(' copy_spec param_list ')'			{ $$ = check_params ($3); }
	;

param_list
	: /* empty */							{ $$ =  0; }
	| parameter_list
	| parameter_list ',' ELLIPSIS
		{
			$$ = param_append_identifiers ($1, 0, 0);
		}
	| ELLIPSIS
		{
			$$ = make_ellipsis ();
		}
	;

parameter_list
	: parameter
	| parameter_list ',' parameter
		{
			$$ = append_params ($1, $3);
		}
	;

parameter
	: declspecs_ts param_declarator
		{
			$$ = make_param ($2);
		}
	| declspecs_ts notype_declarator
		{
			$$ = make_param ($2);
		}
	| declspecs_ts absdecl
		{
			$$ = make_param ($2);
		}
	| declspecs_nosc_nots notype_declarator
		{
			$$ = make_param ($2);
		}
	| declspecs_nosc_nots absdecl
		{
			$$ = make_param ($2);
		}
	;

absdecl
	: /* empty */
		{
			$$ = $<spec>0;
			$$.sym = new_symbol (0);
		}
	| absdecl1
	;

absdecl1
	: direct_absdecl
	| '*' ptr_spec absdecl
		{
			$$ = pointer_spec ($2, $3);
		}
	;

direct_absdecl
	: '(' copy_spec absdecl1 ')' { $$ = $3; }
	| direct_absdecl function_params
		{
			$$ = function_spec ($1, $2);
		}
	| direct_absdecl array_decl
		{
			$$ = array_spec ($1, $2);
		}
	| function_params
		{
			$$ = function_spec ($<spec>0, $1);
		}
	| array_decl
		{
			$$ = array_spec ($<spec>0, $1);
		}
	;

param_declarator
	: param_declarator_starttypename
	| param_declarator_nostarttypename
	;

param_declarator_starttypename
	: param_declarator_starttypename function_params
		{ $$ = $1; internal_error (0, "not implemented"); }
	| param_declarator_starttypename array_decl
		{ $$ = $1; internal_error (0, "not implemented"); }
	| TYPE_NAME
		{ $$ = $1; internal_error (0, "not implemented"); }
	| OBJECT_NAME
		{ $$ = $1; internal_error (0, "not implemented"); }
	;

param_declarator_nostarttypename
	: param_declarator_nostarttypename function_params
		{ $$ = $1; internal_error (0, "not implemented"); }
	| param_declarator_nostarttypename array_decl
		{ $$ = $1; internal_error (0, "not implemented"); }
	| '*' ptr_spec param_declarator_starttypename
		{ $$ = $3; internal_error (0, "not implemented"); }
	| '*' ptr_spec param_declarator_nostarttypename
		{ $$ = $3; internal_error (0, "not implemented"); }
	| '(' copy_spec param_declarator_nostarttypename ')'	{ $$ = $3; }
	;

typename
	: declspecs_nosc absdecl
		{
			$$ = typename_spec ($2);
		}
	;

array_decl
	: '[' expr ']'
		{
			if (is_int_val ($2) && expr_int ($2) > 0) {
				$$ = expr_int ($2);
			} else if (is_uint_val ($2) && expr_uint ($2) > 0) {
				$$ = expr_uint ($2);
			} else {
				error (0, "invalid array size");
				$$ = 0;
			}
		}
	| '[' ']'					{ $$ = 0; }
	;

decl
	: declspecs_ts local_expr initdecls seq_semi
		{
			$$ = local_expr;
			local_expr = 0;
		}
	| declspecs_nots local_expr notype_initdecls seq_semi
		{
			$$ = local_expr;
			local_expr = 0;
		}
	| declspecs ';' { $$ = 0; }
	;

local_expr
	: /* emtpy */
		{
			$<spec>$ = $<spec>0;
			local_expr = new_block_expr (0);
		}
	;

var_initializer
	: expr									{ $$ = $1; }
	| compound_init
		{
			if (!$1 && is_scalar ($<spec>-1.type)) {
				error (0, "empty scalar initializer");
			}
			$$ = $1 ? $1 : new_nil_expr ();
		}
	;

compound_init
	: '{' element_list optional_comma '}'		{ $$ = $2; }
	| '{' '}'									{ $$ = 0; }
	;

element_list
	: element
		{
			$$ = new_compound_init ();
			append_element ($$, $1);
		}
	| element_list ','  element
		{
			append_element ($$, $3);
		}
	;

element
	: designator '=' compound_init				{ $$ = new_element ($3, $1); }
	| designator '=' expr 						{ $$ = new_element ($3, $1); }
	| compound_init								{ $$ = new_element ($1, 0); }
	| expr										{ $$ = new_element ($1, 0); }
	;

designator
	: designator_spec
	| designator designator_spec
		{
			designator_t *des = $1;
			while (des->next) {
				des = des->next;
			}
			des->next = $2;
			$$ = $1;
		}
	;

designator_spec
	: '.' ident_expr	{ $$ = new_designator ($2, 0); }
	| '.' NAME			{ $$ = new_designator (new_symbol_expr ($2), 0); }
	| '[' expr ']'      { $$ = new_designator (0, $2); }
	;

optional_comma
	: /* empty */
	| ','
	;

push_scope
	: /* empty */
		{
			if (!options.traditional) {
				current_symtab = new_symtab (current_symtab, stab_local);
				current_symtab->space = current_symtab->parent->space;
			}
		}
	;

pop_scope
	: /* empty */
		{
			if (!options.traditional)
				current_symtab = current_symtab->parent;
		}
	;

flush_dag
	: /* empty */
		{
			if (!no_flush_dag) {
				edag_flush ();
			}
		}
	;

seq_semi
	: ';' flush_dag
	;

compound_statement
	: '{' push_scope flush_dag statements '}' pop_scope flush_dag { $$ = $4; }
	;

statements
	: /*empty*/
		{
			$$ = new_block_expr (0);
		}
	| statements flush_dag statement
		{
			$$ = append_expr ($1, $3);
		}
	;

local_def
	: LOCAL decl { $$ = $2; }
	| decl
	;

statement
	: ';'						{ $$ = 0; }
	| error ';'					{ $$ = 0; yyerrok; }
	| compound_statement		{ $$ = $1; }
	| local_def					{ $$ = $1; }
	| RETURN opt_expr ';'		{ $$ = return_expr (current_func, $2); }
	| RETURN compound_init ';'	{ $$ = return_expr (current_func, $2); }
	| BREAK ';'
		{
			$$ = 0;
			if (break_label)
				$$ = goto_expr (break_label);
			else
				error (0, "break outside of loop or switch");
		}
	| CONTINUE ';'
		{
			$$ = 0;
			if (continue_label)
				$$ = goto_expr (continue_label);
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
	| SWITCH break_label '(' comma_expr switch_block ')' compound_statement
		{
			$$ = switch_expr (switch_block, break_label, $7);
			switch_block = $5;
			break_label = $2;
		}
	| IF '(' texpr ')' flush_dag statement %prec IFX
		{
			$$ = build_if_statement (0, $3, $6, 0, 0);
		}
	| IF '(' texpr ')' flush_dag statement else statement
		{
			$$ = build_if_statement (0, $3, $6, $7, $8);
		}
	| FOR push_scope break_label continue_label
			'(' opt_init_semi opt_expr seq_semi opt_expr ')' flush_dag statement pop_scope
		{
			if ($6) {
				$6 = build_block_expr ($6, false);
			}
			$$ = build_for_statement ($6, $7, $9, $12,
									  break_label, continue_label);
			break_label = $3;
			continue_label = $4;
		}
	| WHILE break_label continue_label '(' texpr ')' flush_dag statement
		{
			$$ = build_while_statement (0, $5, $8, break_label,
										continue_label);
			break_label = $2;
			continue_label = $3;
		}
	| DO break_label continue_label statement WHILE '(' texpr ')' ';'
		{
			$$ = build_do_while_statement ($4, 0, $7,
										   break_label, continue_label);
			break_label = $2;
			continue_label = $3;
		}
	| comma_expr ';'
		{
			$$ = $1;
		}
	;

else
	: ELSE flush_dag
		{
			// this is only to get the the file and line number info
			$$ = new_nil_expr ();
		}
	;

bool_label
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
			switch_block->test = $<expr>0;
		}
	;

opt_init_semi
	: comma_expr seq_semi
	| decl /* contains ; */		{ $$ = (expr_t *) $1; }
	| ';'
		{
			$$ = 0;
		}
	;

opt_expr
	: comma_expr
	| /* empty */
		{
			$$ = 0;
		}
	;

unary_expr
	: NAME      				{ $$ = new_symbol_expr ($1); }
	| ARGS						{ $$ = new_name_expr (".args"); }
	| SELF						{ $$ = new_self_expr (); }
	| THIS						{ $$ = new_this_expr (); }
	| const						{ $$ = $1; }
	| '(' expr ')'				{ $$ = $2; ((expr_t *) $$)->paren = 1; }
	| unary_expr '(' opt_arg_list ')' { $$ = function_expr ($1, $3); }
	| unary_expr '[' expr ']'		{ $$ = array_expr ($1, $3); }
	| unary_expr '.' ident_expr		{ $$ = field_expr ($1, $3); }
	| unary_expr '.' unary_expr		{ $$ = field_expr ($1, $3); }
	| INCOP unary_expr				{ $$ = incop_expr ($1, $2, 0); }
	| unary_expr INCOP				{ $$ = incop_expr ($2, $1, 1); }
	| unary_expr REVERSE			{ $$ = unary_expr (GLSL_REVERSE, $1); }
	| DUAL cast_expr %prec UNARY	{ $$ = unary_expr (GLSL_DUAL, $2); }
	| '+' cast_expr %prec UNARY	{ $$ = $2; }
	| '-' cast_expr %prec UNARY	{ $$ = unary_expr ('-', $2); }
	| '!' cast_expr %prec UNARY	{ $$ = unary_expr ('!', $2); }
	| '~' cast_expr %prec UNARY	{ $$ = unary_expr ('~', $2); }
	| '&' cast_expr %prec UNARY	{ $$ = address_expr ($2, 0); }
	| '*' cast_expr %prec UNARY	{ $$ = deref_pointer_expr ($2); }
	| SIZEOF unary_expr	%prec UNARY	{ $$ = sizeof_expr ($2, 0); }
	| SIZEOF '(' typename ')'	%prec HYPERUNARY
		{
			$$ = sizeof_expr (0, $3.type);
		}
	| vector_expr				{ $$ = new_vector_list ($1); }
	;

ident_expr
	: OBJECT_NAME				{ $$ = new_symbol_expr ($1.sym); }
	| CLASS_NAME				{ $$ = new_symbol_expr ($1); }
	| TYPE_NAME					{ $$ = new_symbol_expr ($1.sym); }
	;

vector_expr
	: '[' expr ',' { no_flush_dag = true; } expr_list ']'
		{
			$$ = expr_prepend_expr ($5, $2);
			no_flush_dag = false;
		}
	;

cast_expr
	: '(' typename ')' cast_expr
		{
			$$ = cast_expr (find_type ($2.type), $4);
		}
	| unary_expr %prec LOW
	;

expr
	: cast_expr
	| expr '=' expr				{ $$ = assign_expr ($1, $3); }
	| expr '=' compound_init	{ $$ = assign_expr ($1, $3); }
	| expr ASX expr				{ $$ = asx_expr ($2, $1, $3); }
	| expr '?' flush_dag expr ':' expr 	{ $$ = conditional_expr ($1, $4, $6); }
	| expr AND flush_dag bool_label expr{ $$ = bool_expr (GLSL_AND, $4, $1, $5); }
	| expr OR flush_dag bool_label expr	{ $$ = bool_expr (GLSL_OR,  $4, $1, $5); }
	| expr EQ expr				{ $$ = binary_expr (GLSL_EQ,  $1, $3); }
	| expr NE expr				{ $$ = binary_expr (GLSL_NE,  $1, $3); }
	| expr LE expr				{ $$ = binary_expr (GLSL_LE,  $1, $3); }
	| expr GE expr				{ $$ = binary_expr (GLSL_GE,  $1, $3); }
	| expr LT expr				{ $$ = binary_expr (GLSL_LT,  $1, $3); }
	| expr GT expr				{ $$ = binary_expr (GLSL_GT,  $1, $3); }
	| expr SHL expr				{ $$ = binary_expr (GLSL_SHL, $1, $3); }
	| expr SHR expr				{ $$ = binary_expr (GLSL_SHR, $1, $3); }
	| expr '+' expr				{ $$ = binary_expr ('+', $1, $3); }
	| expr '-' expr				{ $$ = binary_expr ('-', $1, $3); }
	| expr '*' expr				{ $$ = binary_expr ('*', $1, $3); }
	| expr '/' expr				{ $$ = binary_expr ('/', $1, $3); }
	| expr '&' expr				{ $$ = binary_expr ('&', $1, $3); }
	| expr '|' expr				{ $$ = binary_expr ('|', $1, $3); }
	| expr '^' expr				{ $$ = binary_expr ('^', $1, $3); }
	| expr '%' expr				{ $$ = binary_expr ('%', $1, $3); }
	| expr MOD expr				{ $$ = binary_expr (GLSL_MOD, $1, $3); }
	| expr GEOMETRIC expr		{ $$ = binary_expr (GLSL_GEOMETRIC, $1, $3); }
	| expr HADAMARD expr		{ $$ = binary_expr (GLSL_HADAMARD, $1, $3); }
	| expr CROSS expr			{ $$ = binary_expr (GLSL_CROSS, $1, $3); }
	| expr DOT expr				{ $$ = binary_expr (GLSL_DOT, $1, $3); }
	| expr WEDGE expr			{ $$ = binary_expr (GLSL_WEDGE, $1, $3); }
	| expr REGRESSIVE expr		{ $$ = binary_expr (GLSL_REGRESSIVE, $1, $3); }
	;

texpr
	: expr						{ $$ = convert_bool ($1, 1); }
	;

comma_expr
	: expr_list
		{
			if ($1->list.head->next) {
				$$ = build_block_expr ($1, true);
			} else {
				$$ = (expr_t *) $1->list.head->expr;
			}
		}
	;

expr_list
	: expr							{ $$ = new_list_expr ($1); }
	| expr_list ',' flush_dag expr	{ $$ = expr_append_expr ($1, $4); }
	;

opt_arg_list
	: /* emtpy */				{ $$ = 0; }
	| arg_list					{ $$ = $1; }
	;

arg_list
	: arg_expr					{ $$ = new_list_expr ($1); }
	| arg_list ',' arg_expr		{ $$ = expr_prepend_expr ($1, $3); }
	;

arg_expr
	: expr
	| compound_init
	;

const
	: VALUE
	| NIL						{ $$ = new_nil_expr (); }
	| string
	;

string
	: STRING
	| string STRING				{ $$ = binary_expr ('+', $1, $2); }
	;

identifier
	: NAME
	| OBJECT_NAME { $$ = $1.sym; }
	| CLASS_NAME
	| TYPE_NAME { $$ = $1.sym; }
	;

%%


static __attribute__((used)) keyword_t glsl_keywords[] = {
	{"const",                   0},
	{"uniform",                 0},
	{"buffer",                  0},
	{"shared",                  0},
	{"attribute",               0},
	{"varying",                 0},
	{"coherent",                0},
	{"volatile",                0},
	{"restrict",                0},
	{"readonly",                0},
	{"writeonly",               0},
	{"atomic_uint",             0},
	{"layout",                  0},
	{"centroid",                0},
	{"flat",                    0},
	{"smooth",                  0},
	{"noperspective",           0},
	{"patch",                   0},
	{"sample",                  0},
	{"invariant",               0},
	{"precise",                 0},
	{"break",                   QC_BREAK},
	{"continue",                QC_CONTINUE},
	{"do",                      QC_DO},
	{"for",                     QC_FOR},
	{"while",                   QC_WHILE},
	{"switch",                  QC_SWITCH},
	{"case",                    QC_CASE},
	{"default",                 QC_DEFAULT},
	{"if",                      QC_IF},
	{"else",                    QC_ELSE},
	{"subroutine",              0},
	{"in",                      0},
	{"out",                     0},
	{"inout",                   0},
	{"int",                     0},
	{"void",                    QC_TYPE_SPEC, .spec = {.type = &type_void}},
	{"bool",                    QC_TYPE_SPEC, .spec = {.type = &type_bool}},
	{"true",                    0},
	{"false",                   0},
	{"float",                   QC_TYPE_SPEC, .spec = {.type = &type_float}},
	{"double",                  QC_TYPE_SPEC, .spec = {.type = &type_double}},
	{"discard",                 0},
	{"return",                  0},
	{"vec2",                    QC_TYPE_SPEC, .spec = {.type = &type_vec2}},
	{"vec3",                    QC_TYPE_SPEC, .spec = {.type = &type_vec3}},
	{"vec4",                    QC_TYPE_SPEC, .spec = {.type = &type_vec4}},
	{"ivec2",                   QC_TYPE_SPEC, .spec = {.type = &type_ivec2}},
	{"ivec3",                   QC_TYPE_SPEC, .spec = {.type = &type_ivec3}},
	{"ivec4",                   QC_TYPE_SPEC, .spec = {.type = &type_ivec4}},
	{"bvec2",                   QC_TYPE_SPEC, .spec = {.type = &type_bvec2}},
	{"bvec3",                   QC_TYPE_SPEC, .spec = {.type = &type_bvec3}},
	{"bvec4",                   QC_TYPE_SPEC, .spec = {.type = &type_bvec4}},
	{"uint",                    QC_TYPE_SPEC, .spec = {.type = &type_uint}},
	{"uvec2",                   QC_TYPE_SPEC, .spec = {.type = &type_uivec2}},
	{"uvec3",                   QC_TYPE_SPEC, .spec = {.type = &type_uivec3}},
	{"uvec4",                   QC_TYPE_SPEC, .spec = {.type = &type_uivec4}},
	{"dvec2",                   QC_TYPE_SPEC, .spec = {.type = &type_dvec2}},
	{"dvec3",                   QC_TYPE_SPEC, .spec = {.type = &type_dvec3}},
	{"dvec4",                   QC_TYPE_SPEC, .spec = {.type = &type_dvec4}},
	{"mat2",                    QC_TYPE_SPEC, .spec = {.type = &type_mat2x2}},
	{"mat3",                    QC_TYPE_SPEC, .spec = {.type = &type_mat3x3}},
	{"mat4",                    QC_TYPE_SPEC, .spec = {.type = &type_mat4x4}},
	{"mat2x2",                  QC_TYPE_SPEC, .spec = {.type = &type_mat2x2}},
	{"mat2x3",                  QC_TYPE_SPEC, .spec = {.type = &type_mat2x3}},
	{"mat2x4",                  QC_TYPE_SPEC, .spec = {.type = &type_mat2x4}},
	{"mat3x2",                  QC_TYPE_SPEC, .spec = {.type = &type_mat3x2}},
	{"mat3x3",                  QC_TYPE_SPEC, .spec = {.type = &type_mat3x3}},
	{"mat3x4",                  QC_TYPE_SPEC, .spec = {.type = &type_mat3x4}},
	{"mat4x2",                  QC_TYPE_SPEC, .spec = {.type = &type_mat4x2}},
	{"mat4x3",                  QC_TYPE_SPEC, .spec = {.type = &type_mat4x3}},
	{"mat4x4",                  QC_TYPE_SPEC, .spec = {.type = &type_mat4x4}},
	{"dmat2",                   QC_TYPE_SPEC, .spec = {.type = &type_dmat2x2}},
	{"dmat3",                   QC_TYPE_SPEC, .spec = {.type = &type_dmat3x3}},
	{"dmat4",                   QC_TYPE_SPEC, .spec = {.type = &type_dmat4x4}},
	{"dmat2x2",                 QC_TYPE_SPEC, .spec = {.type = &type_dmat2x2}},
	{"dmat2x3",                 QC_TYPE_SPEC, .spec = {.type = &type_dmat2x3}},
	{"dmat2x4",                 QC_TYPE_SPEC, .spec = {.type = &type_dmat2x4}},
	{"dmat3x2",                 QC_TYPE_SPEC, .spec = {.type = &type_dmat3x2}},
	{"dmat3x3",                 QC_TYPE_SPEC, .spec = {.type = &type_dmat3x3}},
	{"dmat3x4",                 QC_TYPE_SPEC, .spec = {.type = &type_dmat3x4}},
	{"dmat4x2",                 QC_TYPE_SPEC, .spec = {.type = &type_dmat4x2}},
	{"dmat4x3",                 QC_TYPE_SPEC, .spec = {.type = &type_dmat4x3}},
	{"dmat4x4",                 QC_TYPE_SPEC, .spec = {.type = &type_dmat4x4}},
	{"lowp",                    0},
	{"mediump",                 0},
	{"highp",                   0},
	{"precision",               0},
	{"sampler1D",               0},
	{"sampler1DShadow",         0},
	{"sampler1DArray",          0},
	{"sampler1DArrayShadow",    0},
	{"isampler1D",              0},
	{"isampler1DArray",         0},
	{"usampler1D",              0},
	{"usampler1DArray",         0},
	{"sampler2D",               0},
	{"sampler2DShadow",         0},
	{"sampler2DArray",          0},
	{"sampler2DArrayShadow",    0},
	{"isampler2D",              0},
	{"isampler2DArray",         0},
	{"usampler2D",              0},
	{"usampler2DArray",         0},
	{"sampler2DRect",           0},
	{"sampler2DRectShadow",     0},
	{"isampler2DRect",          0},
	{"usampler2DRect",          0},
	{"sampler2DMS",             0},
	{"isampler2DMS",            0},
	{"usampler2DMS",            0},
	{"sampler2DMSArray",        0},
	{"isampler2DMSArray",       0},
	{"usampler2DMSArray",       0},
	{"sampler3D",               0},
	{"isampler3D",              0},
	{"usampler3D",              0},
	{"samplerCube",             0},
	{"samplerCubeShadow",       0},
	{"isamplerCube",            0},
	{"usamplerCube",            0},
	{"samplerCubeArray",        0},
	{"samplerCubeArrayShadow",  0},
	{"isamplerCubeArray",       0},
	{"usamplerCubeArray",       0},
	{"samplerBuffer",           0},
	{"isamplerBuffer",          0},
	{"usamplerBuffer",          0},
	{"image1D",                 0},
	{"iimage1D",                0},
	{"uimage1D",                0},
	{"image1DArray",            0},
	{"iimage1DArray",           0},
	{"uimage1DArray",           0},
	{"image2D",                 0},
	{"iimage2D",                0},
	{"uimage2D",                0},
	{"image2DArray",            0},
	{"iimage2DArray",           0},
	{"uimage2DArray",           0},
	{"image2DRect",             0},
	{"iimage2DRect",            0},
	{"uimage2DRect",            0},
	{"image2DMS",               0},
	{"iimage2DMS",              0},
	{"uimage2DMS",              0},
	{"image2DMSArray",          0},
	{"iimage2DMSArray",         0},
	{"uimage2DMSArray",         0},
	{"image3D",                 0},
	{"iimage3D",                0},
	{"uimage3D",                0},
	{"imageCube",               0},
	{"iimageCube",              0},
	{"uimageCube",              0},
	{"imageCubeArray",          0},
	{"iimageCubeArray",         0},
	{"uimageCubeArray",         0},
	{"imageBuffer",             0},
	{"iimageBuffer",            0},
	{"uimageBuffer",            0},
	{"struct",                  0},

	//vulkan
	{"texture1D",               0},
	{"texture1DArray",          0},
	{"itexture1D",              0},
	{"itexture1DArray",         0},
	{"utexture1D",              0},
	{"utexture1DArray",         0},
	{"texture2D",               0},
	{"texture2DArray",          0},
	{"itexture2D",              0},
	{"itexture2DArray",         0},
	{"utexture2D",              0},
	{"utexture2DArray",         0},
	{"texture2DRect",           0},
	{"itexture2DRect",          0},
	{"utexture2DRect",          0},
	{"texture2DMS",             0},
	{"itexture2DMS",            0},
	{"utexture2DMS",            0},
	{"texture2DMSArray",        0},
	{"itexture2DMSArray",       0},
	{"utexture2DMSArray",       0},
	{"texture3D",               0},
	{"itexture3D",              0},
	{"utexture3D",              0},
	{"textureCube",             0},
	{"itextureCube",            0},
	{"utextureCube",            0},
	{"textureCubeArray",        0},
	{"itextureCubeArray",       0},
	{"utextureCubeArray",       0},
	{"textureBuffer",           0},
	{"itextureBuffer",          0},
	{"utextureBuffer",          0},
	{"sampler",                 0},
	{"samplerShadow",           0},
	{"subpassInput",            0},
	{"isubpassInput",           0},
	{"usubpassInput",           0},
	{"subpassInputMS",          0},
	{"isubpassInputMS",         0},
	{"usubpassInputMS",         0},

	//reserved
	{"common",                  0},
	{"partition",               0},
	{"active",                  0},
	{"asm",                     0},
	{"class",                   0},
	{"union",                   0},
	{"enum",                    0},
	{"typedef",                 0},
	{"template",                0},
	{"this",                    0},
	{"resource",                0},
	{"goto",                    0},
	{"inline",                  0},
	{"noinline",                0},
	{"public",                  0},
	{"static",                  0},
	{"extern",                  0},
	{"external",                0},
	{"interface",               0},
	{"long",                    0},
	{"short",                   0},
	{"half",                    0},
	{"fixed",                   0},
	{"unsigned",                0},
	{"superp",                  0},
	{"input",                   0},
	{"output",                  0},
	{"hvec2",                   0},
	{"hvec3",                   0},
	{"hvec4",                   0},
	{"fvec2",                   0},
	{"fvec3",                   0},
	{"fvec4",                   0},
	{"filter",                  0},
	{"sizeof",                  0},
	{"cast",                    0},
	{"namespace",               0},
	{"using",                   0},
	{"sampler3DRect",           0},
};
