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
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/switch.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#define YYDEBUG 1
#define YYERROR_VERBOSE 1
#undef YYERROR_VERBOSE

extern char *qc_yytext;

static void
yyerror (const char *s)
{
#ifdef YYERROR_VERBOSE
	error (0, "%s %s\n", qc_yytext, s);
#else
	error (0, "%s before %s", s, qc_yytext);
#endif
}

static void
parse_error (void)
{
	error (0, "parse error before %s", qc_yytext);
}

#define PARSE_ERROR do { parse_error (); YYERROR; } while (0)

int yylex (void);

%}

%union {
	int			op;
	int         size;
	specifier_t spec;
	void       *pointer;			// for ensuring pointer values are null
	struct type_s	*type;
	struct expr_s	*expr;
	struct element_s *element;
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
	struct attribute_s *attribute;
}

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
%left			'*' '/' '%' MOD SCALE
%left           CROSS DOT
%right	<op>	SIZEOF UNARY INCOP
%left			HYPERUNARY
%left			'.' '(' '['

%token	<symbol>	CLASS_NAME NAME
%token	<expr>		VALUE STRING

%token				LOCAL WHILE DO IF ELSE FOR BREAK CONTINUE
%token				RETURN AT_RETURN ELLIPSIS
%token				NIL GOTO SWITCH CASE DEFAULT ENUM
%token				ARGS TYPEDEF EXTERN STATIC SYSTEM OVERLOAD NOT ATTRIBUTE
%token				UNSIGNED SIGNED LONG SHORT
%token	<op>		STRUCT
%token	<type>		TYPE
%token	<symbol>	OBJECT TYPE_NAME
%token				CLASS DEFS ENCODE END IMPLEMENTATION INTERFACE PRIVATE
%token				PROTECTED PROTOCOL PUBLIC SELECTOR REFERENCE SELF THIS

%type	<spec>		optional_specifiers specifiers local_specifiers
%type	<spec>		storage_class save_storage set_spec_storage
%type	<spec>		type_specifier type_specifier_or_storage_class
%type	<spec>		type

%type	<attribute>	attribute_list attribute

%type	<param>		function_params var_list param_declaration
%type	<param>		qc_func_params qc_var_list qc_param_decl
%type	<symbol>	new_name
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

%type	<spec>		ivar_decl decl decl_list
%type	<spec>		ivars
%type	<param>		param param_list
%type	<symbol>	methoddef
%type	<expr>		opt_initializer var_initializer local_def

%type	<expr>		opt_init opt_expr comma_expr expr
%type	<expr>		compound_init element_list
%type	<element>	element
%type	<expr>		optional_state_expr texpr vector_expr
%type	<expr>		statement statements compound_statement
%type	<expr>		else bool_label break_label continue_label
%type	<expr>		unary_expr ident_expr cast_expr expr_list
%type	<expr>		opt_arg_list arg_list arg_expr
%type	<expr>		init_var_decl_list init_var_decl
%type	<switch_block> switch_block
%type	<symbol>	identifier label

%type	<symbol>	overloaded_identifier

%type	<expr>		identifier_list
%type	<symbol>	protocol_name_list selector reserved_word
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
%type	<op>		ci not

%{

static switch_block_t *switch_block;
static expr_t *break_label;
static expr_t *continue_label;

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
	if (new.is_typedef || new.storage) {
		if ((spec.is_typedef || spec.storage) && !spec.multi_store) {
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
default_type (specifier_t spec, symbol_t *sym)
{
	if (spec.type) {
		if (is_float (spec.type) && !spec.multi_type) {
			// no modifieres allowed
			if (spec.is_unsigned) {
				spec.multi_type = 1;
				error (0, "both unsigned and float in declaration specifiers");
			} else if (spec.is_signed) {
				spec.multi_type = 1;
				error (0, "both signed and float in declaration specifiers");
			} else if (spec.is_short) {
				spec.multi_type = 1;
				error (0, "both short and float in declaration specifiers");
			} else if (spec.is_long) {
				spec.multi_type = 1;
				error (0, "both long and float in declaration specifiers");
			}
		}
		if (is_double (spec.type)) {
			// long is allowed but ignored
			if (spec.is_unsigned) {
				spec.multi_type = 1;
				error (0, "both unsigned and double in declaration specifiers");
			} else if (spec.is_signed) {
				spec.multi_type = 1;
				error (0, "both signed and double in declaration specifiers");
			} else if (spec.is_short) {
				spec.multi_type = 1;
				error (0, "both short and double in declaration specifiers");
			}
		}
		if (is_int (spec.type)) {
			// signed and short are ignored
			if (spec.is_unsigned && spec.is_long) {
				spec.type = &type_ulong;
			} else if (spec.is_long) {
				spec.type = &type_long;
			}
		}
	} else {
		if (spec.is_long) {
			if (spec.is_unsigned) {
				spec.type = type_ulong_uint;
			} else {
				spec.type = type_long_int;
			}
		} else {
			if (spec.is_unsigned) {
				spec.type = &type_uint;
			} else if (spec.is_signed || spec.is_short) {
				spec.type = &type_int;
			}
		}
	}
	if (!spec.type) {
		spec.type = type_default;
		warning (0, "type defaults to '%s' in declaration of '%s'",
				 type_default->name, sym->name);
	}
	return spec;
}

static int
is_anonymous_struct (specifier_t spec)
{
	if (spec.sym) {
		return 0;
	}
	if (!is_struct (spec.type)) {
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
	symtab_addsymbol (current_symtab, spec.sym);
	return !!spec.sym->table;
}

static void
check_specifiers (specifier_t spec)
{
	if (!is_null_spec (spec)) {
		if (!spec.type && !spec.sym) {
			warning (0, "useless specifiers");
		} else if (spec.type && !spec.sym) {
			if (is_anonymous_struct (spec)){
				warning (0, "unnamed struct/union that defines "
						 "no instances");
			} else if (!is_enum (spec.type) && !is_struct (spec.type)) {
				warning (0, "useless type name in empty declaration");
			}
		} else if (!spec.type && spec.sym) {
			bug (0, "wha? %p %p", spec.type, spec.sym);
		} else {
			// a type name (id, typedef, etc) was used as a variable name.
			// this is allowed in C, so long as it's in a different scope
			if (!use_type_name (spec)) {
				error (0, "%s redeclared as different kind of symbol",
					   spec.sym->name);
			}
		}
	}
}

static void
set_func_type_attrs (type_t *func, specifier_t spec)
{
	func->t.func.no_va_list = spec.no_va_list;
	func->t.func.void_return = spec.void_return;
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
	| external_def_list obj_def
	| error END
		{
			yyerrok;
			current_class = 0;
			current_symtab = pr.symtab;
			current_storage = sc_global;
		}
	| error ';'
		{
			yyerrok;
			current_class = 0;
			current_symtab = pr.symtab;
			current_storage = sc_global;
		}
	| error '}'
		{
			yyerrok;
			current_class = 0;
			current_symtab = pr.symtab;
			current_storage = sc_global;
		}
	;

external_def
	: optional_specifiers external_decl_list ';' { }
	| optional_specifiers ';'
		{
			check_specifiers ($1);
		}
	| optional_specifiers qc_func_params
		{
			type_t    **type;
			type_t     *ret_type;
			$<spec>$ = $1;		// copy spec bits and storage
			// .float () foo; is a field holding a function variable rather
			// than a function that returns a float field.
			for (type = &$1.type; *type && (*type)->type == ev_field;
				 type = &(*type)->t.fldptr.type) {
			}
			ret_type = *type;
			*type = 0;
			*type = parse_params (0, $2);
			set_func_type_attrs ((*type), $1);
			$<spec>$.type = find_type (append_type ($1.type, ret_type));
			if ($<spec>$.type->type != ev_field)
				$<spec>$.params = $2;
		}
	  function_def_list
		{
			(void) ($<spec>3);
		}
	| optional_specifiers function_decl function_body
	| storage_class '{' save_storage
		{
			current_storage = $1.storage;
		}
	  external_def_list '}' ';'
		{
			current_storage = $3.storage;
		}
	;

save_storage
	: /* emtpy */
		{
			$$.storage = current_storage;
		}
	;

set_spec_storage
	: /* emtpy */
		{
			$$ = $<spec>0;
			$$.storage = current_storage;
		}
	;

function_body
	: optional_state_expr
		{
			symbol_t   *sym = $<symbol>0;
			specifier_t spec = default_type ($<spec>-1, sym);

			set_func_type_attrs (sym->type, spec);
			sym->type = find_type (append_type (sym->type, spec.type));
			$<symbol>$ = function_symbol (sym, spec.is_overload, 1);
		}
	  save_storage
		{
			$<symtab>$ = current_symtab;
			current_func = begin_function ($<symbol>2, 0, current_symtab, 0,
										   $<spec>-1.storage);
			current_symtab = current_func->locals;
			current_storage = sc_local;
		}
	  compound_statement
		{
			build_code_function ($<symbol>2, $1, $5);
			current_symtab = $<symtab>4;
			current_storage = $3.storage;
			current_func = 0;
		}
	| '=' '#' expr ';'
		{
			symbol_t   *sym = $<symbol>0;
			specifier_t spec = default_type ($<spec>-1, sym);

			set_func_type_attrs (sym->type, spec);
			sym->type = find_type (append_type (sym->type, spec.type));
			sym = function_symbol (sym, spec.is_overload, 1);
			build_builtin_function (sym, $3, 0, spec.storage);
		}
	;

external_decl_list
	: external_decl
	| external_decl_list ',' { $<spec>$ = $<spec>0; }
	  external_decl { (void) ($<spec>3); }
	;

external_decl
	: var_decl
		{
			specifier_t spec = default_type ($<spec>0, $1);
			$1->type = find_type (append_type ($1->type, spec.type));
			if (spec.is_typedef) {
				$1->sy_type = sy_type;
				$1->type=find_type (alias_type ($1->type, $1->type, $1->name));
				symtab_addsymbol (current_symtab, $1);
			} else {
				initialize_def ($1, 0, current_symtab->space, spec.storage,
								current_symtab);
				if ($1->s.def)
					$1->s.def->nosave |= spec.nosave;
			}
		}
	| var_decl var_initializer
		{
			specifier_t spec = default_type ($<spec>0, $1);

			$1->type = find_type (append_type ($1->type, spec.type));
			if (spec.is_typedef) {
				error (0, "typedef %s is initialized", $1->name);
				$1->sy_type = sy_type;
				$1->type=find_type (alias_type ($1->type, $1->type, $1->name));
				symtab_addsymbol (current_symtab, $1);
			} else {
				initialize_def ($1, $2, current_symtab->space, spec.storage,
								current_symtab);
				if ($1->s.def)
					$1->s.def->nosave |= spec.nosave;
			}
		}
	| function_decl
		{
			specifier_t spec = default_type ($<spec>0, $1);
			set_func_type_attrs ($1->type, spec);
			$1->type = find_type (append_type ($1->type, spec.type));
			if (spec.is_typedef) {
				$1->sy_type = sy_type;
				$1->type=find_type (alias_type ($1->type, $1->type, $1->name));
				symtab_addsymbol (current_symtab, $1);
			} else {
				$1 = function_symbol ($1, spec.is_overload, 1);
			}
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
	: NAME						{ $$ = new_attribute ($1->name, 0); }
	| NAME '(' expr_list ')'	{ $$ = new_attribute ($1->name, $3); }
	;

optional_specifiers
	: specifiers
		{
			$$ = $1;
			if (!$$.storage)
				$$.storage = current_storage;
		}
	| /* empty */
		{
			$$ = make_spec (0, current_storage, 0, 0);
		}
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
		{
			$$ = spec_merge ($1, $2);
		}
	;

type_specifier_or_storage_class
	: type_specifier
	| storage_class
	;

type_specifier
	: TYPE
		{
			$$ = make_spec ($1, 0, 0, 0);
		}
	| UNSIGNED
		{
			$$ = make_spec (0, current_storage, 0, 0);
			$$.is_unsigned = 1;
		}
	| SIGNED
		{
			$$ = make_spec (0, current_storage, 0, 0);
			$$.is_signed = 1;
		}
	| LONG
		{
			$$ = make_spec (0, current_storage, 0, 0);
			$$.is_long = 1;
		}
	| SHORT
		{
			$$ = make_spec (0, current_storage, 0, 0);
			$$.is_short = 1;
		}
	| enum_specifier
	| struct_specifier
	| TYPE_NAME
		{
			$$ = make_spec ($1->type, 0, 0, 0);
			$$.sym = $1;
		}
	| OBJECT protocolrefs
		{
			if ($2) {
				type_t      type = *type_id.t.fldptr.type;
				type.next = 0;
				type.protos = $2;
				$$ = make_spec (pointer_type (find_type (&type)), 0, 0, 0);
			} else {
				$$ = make_spec (&type_id, 0, 0, 0);
			}
			$$.sym = $1;
		}
	| CLASS_NAME protocolrefs
		{
			if ($2) {
				type_t      type = *$1->type;
				type.next = 0;
				type.protos = $2;
				$$ = make_spec (find_type (&type), 0, 0, 0);
			} else {
				$$ = make_spec ($1->type, 0, 0, 0);
			}
			$$.sym = $1;
		}
	// NOTE: fields don't parse the way they should. This is not a problem
	// for basic types, but functions need special treatment
	| '.' type_specifier
		{
			// avoid find_type()
			$$ = make_spec (field_type (0), 0, 0, 0);
			$$.type = append_type ($$.type, $2.type);
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
			(void) ($<symbol>3);
		}
	;

enumerator
	: identifier				{ add_enum ($<symbol>0, $1, 0); }
	| identifier '=' expr		{ add_enum ($<symbol>0, $1, $3); }
	;

struct_specifier
	: STRUCT optional_tag '{'
		{
			symbol_t   *sym;
			sym = find_struct ($1, $2, 0);
			if (!sym->table) {
				symtab_addsymbol (current_symtab, sym);
			} else {
				if (!sym->type) {
					internal_error (0, "broken structure symbol?");
				}
				if (sym->type->meta == ty_enum
					|| (sym->type->meta == ty_struct && sym->type->t.symtab)) {
					error (0, "%s %s redefined",
						   $1 == 's' ? "struct" : "union", $2->name);
					$1 = 0;
				} else if (sym->type->meta != ty_struct) {
					internal_error (0, "%s is not a struct or union",
									$2->name);
				}
			}
			current_symtab = new_symtab (current_symtab, stab_local);
		}
	  struct_defs '}'
		{
			symbol_t   *sym;
			symtab_t   *symtab = current_symtab;
			current_symtab = symtab->parent;

			if ($1) {
				sym = build_struct ($1, $2, symtab, 0);
				$$ = make_spec (sym->type, 0, 0, 0);
				if (!sym->table)
					symtab_addsymbol (current_symtab, sym);
			}
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
	: struct_def_list ';'
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

struct_def_list
	: struct_def
	| struct_def_list ';' struct_def
	;

struct_def
	: type struct_decl_list
	| type
		{
			if ($1.sym && $1.sym->type != $1.type) {
				// a type name (id, typedef, etc) was used as a field name.
				// this is allowed in C
				if (!use_type_name ($1)) {
					error (0, "duplicate field `%s'", $1.sym->name);
				}
			} else if (is_anonymous_struct ($1)) {
				// anonymous struct/union
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
			} else {
				// bare type
				warning (0, "declaration does not declare anything");
			}
			$$ = $1;
		}
	;

struct_decl_list
	: struct_decl_list ',' { $<spec>$ = $<spec>0; }
	  struct_decl { (void) ($<spec>3); }
	| struct_decl
	;

struct_decl
	: function_decl
		{
			$<spec>0 = default_type ($<spec>-0, $1);
			$1->type = find_type (append_type ($1->type, $<spec>0.type));
			$1->sy_type = sy_var;
			$1->visibility = current_visibility;
			symtab_addsymbol (current_symtab, $1);
			if (!$1->table) {
				error (0, "duplicate field `%s'", $1->name);
			}
		}
	| var_decl
		{
			$<spec>0 = default_type ($<spec>-0, $1);
			$1->type = find_type (append_type ($1->type, $<spec>0.type));
			$1->sy_type = sy_var;
			$1->visibility = current_visibility;
			symtab_addsymbol (current_symtab, $1);
			if (!$1->table) {
				error (0, "duplicate field `%s'", $1->name);
			}
		}
	| var_decl ':' expr		%prec COMMA		{}
	| ':' expr				%prec COMMA		{}
	;

new_name
	: NAME					%prec COMMA
		{
			$$ = $1;
			// due to the way declarations work, we need a new symbol at all
			// times. redelcarations will be checked later
			if ($$->table)
				$$ = new_symbol ($1->name);
		}
	;

var_decl
	: new_name
		{
			$$ = $1;
			$$->type = 0;
		}
	| var_decl function_params
		{
			$$->type = append_type ($$->type, parse_params (0, $2));
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

qc_func_params
	: '(' ')'								{ $$ = 0; }
	| '(' ps qc_var_list ')'				{ $$ = check_params ($3); }
	| '(' ps TYPE ')'
		{
			if (!is_void ($3))
				PARSE_ERROR;
			$$ = 0;
		}
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

qc_var_list
	: qc_param_decl
	| qc_var_list ',' qc_param_decl
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
			$1 = default_type ($1, $2);
			$2->type = find_type (append_type ($2->type, $1.type));
			$$ = new_param (0, $2->type, $2->name);
		}
	| abstract_decl			{ $$ = new_param (0, $1->type, $1->name); }
	| ELLIPSIS				{ $$ = new_param (0, 0, 0); }
	;

abstract_decl
	: type abs_decl
		{
			// abs_decl's symbol is just a carrier for the type
			if ($2->name) {
				bug (0, "unexpected name in abs_decl");
			}
			if ($1.sym) {
				$1.sym = new_symbol ($1.sym->name);
				$1.sym->type = $2->type;
				$2 = $1.sym;
			}
			$$ = $2;
			$1 = default_type ($1, $2);
			$$->type = find_type (append_type ($$->type, $1.type));
		}
	| error		{ $$ = new_symbol (0); }
	;

qc_param_decl
	: type new_name
		{
			$2->type = find_type ($1.type);
			$$ = new_param (0, $2->type, $2->name);
		}
	| type qc_func_params new_name
		{
			type_t    **type;
			// .float () foo; is a field holding a function variable rather
			// than a function that returns a float field.
			for (type = &$1.type; *type && (*type)->type == ev_field;
				 type = &(*type)->t.fldptr.type)
				 ;
			*type = parse_params (*type, $2);
			set_func_type_attrs ((*type), $1);
			$3->type = find_type ($1.type);
			if ($3->type->type != ev_field)
				$3->params = $2;
			$$ = new_param (0, $3->type, $3->name);
		}
	| ELLIPSIS				{ $$ = new_param (0, 0, 0); }
	;

abs_decl
	: /* empty */			{ $$ = new_symbol (0); }
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
	: '[' expr ']'
		{
			if (!is_int_val ($2) || expr_int ($2) < 1) {
				error (0, "invalid array size");
				$$ = 0;
			} else {
				$$ = expr_int ($2);
			}
		}
	| '[' ']'					{ $$ = 0; }
	;

local_specifiers
	: LOCAL specifiers			{  $$ = $2; }
	| specifiers				{  $$ = $1; }
	;

param_list
	: param						{ $$ = $1; }
	| param_list ',' { $<param>$ = $<param>1; } param
		{
			$$ = $4;
			(void) ($<symbol>3);
		}
	;

param
	: type identifier
		{
			$2 = check_redefined ($2);
			$$ = param_append_identifiers ($<param>0, $2, $1.type);
		}
	;

local_decl_list
	: decl_list
	| qc_func_params
		{
			type_t    **type;
			specifier_t spec = $<spec>0;	// copy spec bits and storage
			// .float () foo; is a field holding a function variable rather
			// than a function that returns a float field.
			for (type = &spec.type; *type && (*type)->type == ev_field;
				 type = &(*type)->t.fldptr.type)
				 ;
			*type = parse_params (*type, $1);
			set_func_type_attrs ((*type), spec);
			spec.type = find_type (spec.type);
			$<spec>$ = spec;
		}
	  func_def_list
		{
			(void) ($<spec>2);
		}
	;

decl_list
	: decl_list ',' { $<spec>$ = $<spec>0; } decl { (void) ($<spec>3); }
	| decl
	;

decl
	: function_decl							{}
	| var_decl opt_initializer
		{
			specifier_t spec = default_type ($<spec>0, $1);
			storage_class_t sc = spec.storage;
			struct defspace_s *space = current_symtab->space;

			if (sc == sc_static)
				space = pr.near_data;
			$1->type = find_type (append_type ($1->type, spec.type));
			initialize_def ($1, $2, space, sc, current_symtab);
			if ($1->s.def)
				$1->s.def->nosave |= spec.nosave;
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
			$$->type = $<spec>0.type;
			if (!local_expr && $$->type->type != ev_field) {
				$$ = function_symbol ($$, $<spec>0.is_overload, 1);
				$$->params = $<spec>0.params;
			}
		}
	;

non_code_func
	: '=' '#' expr
		{
			build_builtin_function ($<symbol>0, $3, 0, $<spec>-1.storage);
		}
	| '=' expr
		{
			if (local_expr) {
				symbol_t   *sym = $<symbol>0;
				specifier_t spec = $<spec>-1;
				initialize_def (sym, $2, current_symtab->space, spec.storage,
								current_symtab);
				if (sym->s.def)
					sym->s.def->nosave |= spec.nosave;
			} else {
				if (is_int_val ($2) || is_float_val ($2)) {
					error (0, "invalid function initializer."
						   " did you forget #?");
				} else {
					error (0, "cannot create global function variables");
				}
			}
		}
	| /* emtpy */
		{
			symbol_t   *sym = $<symbol>0;
			specifier_t spec = $<spec>-1;
			if (!local_expr && sym->type->type != ev_field) {
				// things might be a confused mess from earlier errors
				if (sym->sy_type == sy_func)
					make_function (sym, 0, sym->table->space, spec.storage);
			} else {
				initialize_def (sym, 0, current_symtab->space, spec.storage,
								current_symtab);
				if (sym->s.def)
					sym->s.def->nosave |= spec.nosave;
			}
		}
	;

code_func
	: '=' optional_state_expr
	  save_storage
		{
			$<symtab>$ = current_symtab;
			current_func = begin_function ($<symbol>0, 0, current_symtab, 0,
										   $<spec>-1.storage);
			current_symtab = current_func->locals;
			current_storage = sc_local;
		}
	  compound_statement
		{
			build_code_function ($<symbol>0, $2, $5);
			current_symtab = $<symtab>4;
			current_storage = $3.storage;
			current_func = 0;
		}
	;

opt_initializer
	: /*empty*/									{ $$ = 0; }
	| var_initializer							{ $$ = $1; }
	;

var_initializer
	: '=' expr									{ $$ = $2; }
	| '=' compound_init
		{
			if (!$2 && is_scalar ($<spec>-1.type)) {
				error (0, "empty scalar initializer");
			}
			$$ = $2 ? $2 : new_nil_expr ();
		}
	;

compound_init
	: '{' element_list optional_comma '}'		{ $$ = $2; }
	| '{' '}'									{ $$ = 0; }
	;

optional_state_expr
	: /* emtpy */						{ $$ = 0; }
	| vector_expr						{ $$ = build_state_expr ($1); }
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
	: compound_init								{ $$ = new_element ($1, 0); }
	| expr										{ $$ = new_element ($1, 0); }
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

compound_statement
	: '{' push_scope statements '}' pop_scope { $$ = $3; }
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

local_def
	: local_specifiers
		{
			if (!$1.storage)
				$1.storage = sc_local;
			$<spec>$ = $1;
			local_expr = new_block_expr ();
		}
	  local_decl_list ';'
		{
			$$ = local_expr;
			local_expr = 0;
			(void) ($<spec>2);
		}
	| specifiers opt_initializer ';'
		{
			check_specifiers ($1);
			$$ = 0;
		}
	;

statement
	: ';'						{ $$ = 0; }
	| error ';'					{ $$ = 0; yyerrok; }
	| compound_statement		{ $$ = $1; }
	| local_def					{ $$ = $1; }
	| RETURN opt_expr ';'		{ $$ = return_expr (current_func, $2); }
	| RETURN compound_init ';'	{ $$ = return_expr (current_func, $2); }
	| AT_RETURN	expr ';'		{ $$ = at_return_expr (current_func, $2); }
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
	| label
		{
			$$ = named_label_expr ($1);
		}
	| CASE expr ':'
		{
			$$ = case_label_expr (switch_block, $2);
		}
	| DEFAULT ':'
		{
			$$ = case_label_expr (switch_block, 0);
		}
	| SWITCH break_label '(' expr switch_block ')' compound_statement
		{
			$$ = switch_expr (switch_block, break_label, $7);
			switch_block = $5;
			break_label = $2;
		}
	| GOTO NAME
		{
			expr_t     *label = named_label_expr ($2);
			$$ = goto_expr (label);
		}
	| IF not '(' texpr ')' statement %prec IFX
		{
			$$ = build_if_statement ($2, $4, $6, 0, 0);
		}
	| IF not '(' texpr ')' statement else statement
		{
			$$ = build_if_statement ($2, $4, $6, $7, $8);
		}
	| FOR push_scope break_label continue_label
			'(' opt_init ';' opt_expr ';' opt_expr ')' statement pop_scope
		{
			if ($6) {
				$6 = build_block_expr ($6);
			}
			$$ = build_for_statement ($6, $8, $10, $12,
									  break_label, continue_label);
			break_label = $3;
			continue_label = $4;
		}
	| WHILE break_label continue_label not '(' texpr ')' statement
		{
			$$ = build_while_statement ($4, $6, $8, break_label,
										continue_label);
			break_label = $2;
			continue_label = $3;
		}
	| DO break_label continue_label statement WHILE not '(' texpr ')' ';'
		{
			$$ = build_do_while_statement ($4, $6, $8,
										   break_label, continue_label);
			break_label = $2;
			continue_label = $3;
		}
	| comma_expr ';'
		{
			$$ = $1;
		}
	;

not
	: NOT						{ $$ = 1; }
	| /* empty */				{ $$ = 0; }
	;

else
	: ELSE
		{
			// this is only to get the the file and line number info
			$$ = new_nil_expr ();
		}
	;

label
	: NAME ':'
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

opt_init
	: comma_expr
	| type set_spec_storage init_var_decl_list { $$ = $3; }
	| /* empty */
		{
			$$ = 0;
		}
	;

init_var_decl_list
	: init_var_decl
		{
			$$ = $1;
		}
	| init_var_decl_list ',' { $<spec>$ = $<spec>0; } init_var_decl
		{
			$4->next = $1;
			$$ = $4;
		}
	;

init_var_decl
	: var_decl opt_initializer
		{
			specifier_t spec = $<spec>0;
			$1->type = find_type (append_type ($1->type, spec.type));
			$1->sy_type = sy_var;
			initialize_def ($1, 0, current_symtab->space, spec.storage,
							current_symtab);
			$$ = assign_expr (new_symbol_expr ($1), $2);
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
	| '(' expr ')'				{ $$ = $2; $$->paren = 1; }
	| unary_expr '(' opt_arg_list ')' { $$ = function_expr ($1, $3); }
	| unary_expr '[' expr ']'		{ $$ = array_expr ($1, $3); }
	| unary_expr '.' ident_expr		{ $$ = field_expr ($1, $3); }
	| unary_expr '.' unary_expr		{ $$ = field_expr ($1, $3); }
	| INCOP unary_expr				{ $$ = incop_expr ($1, $2, 0); }
	| unary_expr INCOP				{ $$ = incop_expr ($2, $1, 1); }
	| '+' cast_expr %prec UNARY	{ $$ = $2; }
	| '-' cast_expr %prec UNARY	{ $$ = unary_expr ('-', $2); }
	| '!' cast_expr %prec UNARY	{ $$ = unary_expr ('!', $2); }
	| '~' cast_expr %prec UNARY	{ $$ = unary_expr ('~', $2); }
	| '&' cast_expr %prec UNARY	{ $$ = address_expr ($2, 0); }
	| '*' cast_expr %prec UNARY	{ $$ = deref_pointer_expr ($2); }
	| SIZEOF unary_expr	%prec UNARY	{ $$ = sizeof_expr ($2, 0); }
	| SIZEOF '(' abstract_decl ')'	%prec HYPERUNARY
		{
			$$ = sizeof_expr (0, $3->type);
		}
	| vector_expr				{ $$ = new_vector_list ($1); }
	| obj_expr					{ $$ = $1; }
	;

ident_expr
	: OBJECT					{ $$ = new_symbol_expr ($1); }
	| CLASS_NAME				{ $$ = new_symbol_expr ($1); }
	| TYPE_NAME					{ $$ = new_symbol_expr ($1); }
	;

vector_expr
	: '[' expr ',' expr_list ']'
		{
			expr_t     *t = $4;
			while (t->next)
				t = t->next;
			t->next = $2;
			$$ = $4;
		}
	;

cast_expr
	: '(' abstract_decl ')' cast_expr
		{
			$$ = cast_expr ($2->type, $4);
		}
	| unary_expr %prec LOW
	;

expr
	: cast_expr
	| expr '=' expr				{ $$ = assign_expr ($1, $3); }
	| expr '=' compound_init	{ $$ = assign_expr ($1, $3); }
	| expr ASX expr				{ $$ = asx_expr ($2, $1, $3); }
	| expr '?' expr ':' expr 	{ $$ = conditional_expr ($1, $3, $5); }
	| expr AND bool_label expr	{ $$ = bool_expr (AND, $3, $1, $4); }
	| expr OR bool_label expr	{ $$ = bool_expr (OR,  $3, $1, $4); }
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
	| expr MOD expr				{ $$ = binary_expr (MOD, $1, $3); }
	| expr CROSS expr			{ $$ = binary_expr (CROSS, $1, $3); }
	| expr DOT expr				{ $$ = binary_expr (DOT, $1, $3); }
	;

texpr
	: expr						{ $$ = convert_bool ($1, 1); }
	;

comma_expr
	: expr_list
		{
			if ($1->next) {
				expr_t     *res = $1;
				$1 = build_block_expr ($1);
				$1->e.block.result = res;
			}
			$$ = $1;
		}
	;

expr_list
	: expr
	| expr_list ',' expr
		{
			$3->next = $1;
			$$ = $3;
		}
	;

opt_arg_list
	: /* emtpy */				{ $$ = 0; }
	| arg_list					{ $$ = $1; }
	;

arg_list
	: arg_expr
	| arg_list ',' arg_expr
		{
			$3->next = $1;
			$$ = $3;
		}
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
	| OBJECT
	| CLASS_NAME
	| TYPE_NAME
	;

// Objective-QC stuff

obj_def
	: classdef					{ }
	| classdecl
	| protocoldecl
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
			if (!$1->type) {
				$$ = get_class ($1, 1);
				if (!$1->table) {
					symtab_addsymbol (current_symtab, $1);
				}
			} else if (!is_class ($1->type)) {
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
			if (current_class) {
				warning (0, "‘@end’ missing in implementation context");
				class_finish (current_class);
				current_class = 0;
			}
			$$ = get_class ($1, 0);
			if (!$$) {
				$1 = check_redefined ($1);
				$$ = get_class ($1, 1);
			}
			$$->interface_declared = 1;
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
			if (!$3->interface_declared) {
				$3->interface_declared = 1;
				error (0, "cannot find interface declaration for `%s', "
					   "superclass of `%s'", $3->name, $1->name);
			}
			$1->interface_declared = 1;
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
			if (current_class) {
				warning (0, "‘@end’ missing in implementation context");
				class_finish (current_class);
				current_class = 0;
			}
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
			if ($$ && $$->methods) {
				error (0, "redefinition of protocol %s", $1->name);
				$$ = get_protocol (0, 1);
			}
			if (!$$) {
				$$ = get_protocol ($1->name, 1);
			}
			$$->methods = new_methodlist ();
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

protocoldecl
	: protocol
	  protocol_name_list ';'
		{
			while ($2) {
				get_protocol ($2->name, 1);
				$2 = $2->next;
			}
		}
	;

protocoldef
	: protocol
	  protocol_name
	  protocolrefs			{ protocol_add_protocols ($2, $3); $<class>$ = 0; }
	  methodprotolist			{ protocol_add_methods ($2, $5); }
	  END
		{
			current_class = $<class_type>1;
			(void) ($<class>4);
		}
	;

protocol
	: PROTOCOL					{ $<class_type>$ = current_class; }
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
			if (tab == current_symtab)
				internal_error (0, "ivars already linked to parent scope");
			$<symtab>$ = tab;
			tab->parent = current_symtab;
			current_symtab = ivars;

			current_visibility = vis_protected;
		}
	  ivar_decl_list_2
		{
			symtab_t   *tab = $<symtab>1;
			$$ = current_symtab;
			current_symtab = tab->parent;
			tab->parent = 0;

			tab = $$->parent;	// preserve the ivars inheritance chain
			build_struct ('s', 0, $$, 0);
			$$->parent = tab;
			current_visibility = vis_public;
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
	: type ivars
	| type
		{
			if (is_anonymous_struct ($1)) {
				// anonymous struct/union
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
			} else {
				// bare type
				warning (0, "declaration does not declare anything");
			}
		}
	;

ivars
	: struct_decl
	| ivars ',' { $<spec>$ = $<spec>0; } struct_decl { (void) ($<spec>3); }
	;

methoddef
	: ci methoddecl optional_state_expr
		{
			method_t   *method = $2;

			method->instance = $1;
			$2 = method = class_find_method (current_class, method);
			$<symbol>$ = method_symbol (current_class, method);
		}
	  save_storage
		{
			method_t   *method = $2;
			const char *nicename = method_name (method);
			symbol_t   *sym = $<symbol>4;
			symtab_t   *ivar_scope;

			$<symtab>$ = current_symtab;

			ivar_scope = class_ivar_scope (current_class, current_symtab);
			current_func = begin_function (sym, nicename, ivar_scope, 1,
										   sc_static);
			class_finish_ivar_scope (current_class, ivar_scope,
									 current_func->locals);
			method->func = sym->s.func;
			method->def = sym->s.func->def;
			current_symtab = current_func->locals;
			current_storage = sc_local;
		}
	  compound_statement
		{
			build_code_function ($<symbol>4, $3, $7);
			current_symtab = $<symtab>6;
			current_storage = $5.storage;
			current_func = 0;
		}
	| ci methoddecl '=' '#' const ';'
		{
			symbol_t   *sym;
			method_t   *method = $2;

			method->instance = $1;
			method = class_find_method (current_class, method);
			sym = method_symbol (current_class, method);
			build_builtin_function (sym, $5, 1, sc_static);
			method->func = sym->s.func;
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
	| '-' error ';'
		{ $$ = new_method (&type_id, new_param ("", 0, 0), 0); }
	| '+' error ';'
		{ $$ = new_method (&type_id, new_param ("", 0, 0), 0); }
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
			$$ = param_append_identifiers ($2, 0, 0);
		}
	;

unaryselector
	: selector					{ $$ = new_param ($1->name, 0, 0); }
	;

keywordselector
	: keyworddecl
	| keywordselector keyworddecl { $2->next = $1; $$ = $2; }
	;

protocol_name_list
	: identifier
	| protocol_name_list ',' identifier { $3->next = $1; $$ = $3; }

selector
	: NAME						{ $$ = $1; }
	| CLASS_NAME				{ $$ = $1; }
	| OBJECT					{ $$ = new_symbol (qc_yytext); }
	| TYPE						{ $$ = new_symbol (qc_yytext); }
	| TYPE_NAME					{ $$ = $1; }
	| reserved_word
	;

reserved_word
	: LOCAL						{ $$ = new_symbol (qc_yytext); }
	| RETURN					{ $$ = new_symbol (qc_yytext); }
	| WHILE						{ $$ = new_symbol (qc_yytext); }
	| DO						{ $$ = new_symbol (qc_yytext); }
	| IF						{ $$ = new_symbol (qc_yytext); }
	| ELSE						{ $$ = new_symbol (qc_yytext); }
	| FOR						{ $$ = new_symbol (qc_yytext); }
	| BREAK						{ $$ = new_symbol (qc_yytext); }
	| CONTINUE					{ $$ = new_symbol (qc_yytext); }
	| SWITCH					{ $$ = new_symbol (qc_yytext); }
	| CASE						{ $$ = new_symbol (qc_yytext); }
	| DEFAULT					{ $$ = new_symbol (qc_yytext); }
	| NIL						{ $$ = new_symbol (qc_yytext); }
	| STRUCT					{ $$ = new_symbol (qc_yytext); }
	| ENUM						{ $$ = new_symbol (qc_yytext); }
	| TYPEDEF					{ $$ = new_symbol (qc_yytext); }
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
	: expr
	| CLASS_NAME
		{
			$$ = new_symbol_expr ($1);
		}
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
