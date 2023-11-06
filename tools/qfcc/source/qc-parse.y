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
%define api.prefix {qc_yy}
%define api.pure full
%define api.push-pull push
%define api.token.prefix {QC_}
%locations
%parse-param {void *scanner}

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

#define QC_YYDEBUG 1
#define QC_YYERROR_VERBOSE 1
#undef QC_YYERROR_VERBOSE

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

#include "tools/qfcc/source/qc-parse.h"

#define qc_yytext qc_yyget_text (scanner)
char *qc_yyget_text (void *scanner);

static void
yyerror (YYLTYPE *yylloc, void *scanner, const char *s)
{
#ifdef QC_YYERROR_VERBOSE
	error (0, "%s %s\n", qc_yytext, s);
#else
	error (0, "%s before %s", s, qc_yytext);
#endif
}

static void
parse_error (void *scanner)
{
	error (0, "parse error before %s", qc_yytext);
}

#define PARSE_ERROR do { parse_error (scanner); YYERROR; } while (0)

#define first_line line
#define first_column column

int yylex (YYSTYPE *yylval, YYLTYPE *yylloc);

%}

%code requires { #include "tools/qfcc/include/type.h" }

%define api.location.type {struct rua_loc_s}

%union {
	int			op;
	unsigned    size;
	specifier_t spec;
	void       *pointer;			// for ensuring pointer values are null
	struct type_s	*type;
	const struct expr_s	*expr;
	struct expr_s *mut_expr;
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
	struct designator_s *designator;
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
%type	<spec>		qc_comma

%type	<attribute>	attribute_list attribute

%type	<param>		function_params
%type   <param>		qc_func_params qc_param_list qc_first_param qc_param

%type	<symbol>	tag
%type	<spec>		struct_specifier struct_list
%type	<spec>		enum_specifier algebra_specifier
%type	<symbol>	optional_enum_list enum_list enumerator_list enumerator
%type	<symbol>	enum_init
%type	<size>		array_decl

%type	<expr>		const string

%type	<spec>		ivar_decl
%type   <expr>		decl
%type	<spec>		ivars
%type	<param>		param_list parameter_list parameter
%type	<symbol>	methoddef
%type	<expr>		var_initializer local_def

%type	<mut_expr>	opt_init_semi opt_expr comma_expr
%type   <expr>		expr
%type	<expr>		compound_init
%type   <mut_expr>	element_list expr_list
%type	<designator> designator designator_spec
%type	<element>	element
%type	<expr>		method_optional_state_expr optional_state_expr
%type	<expr>		texpr vector_expr
%type	<expr>		statement
%type	<mut_expr>	statements compound_statement
%type	<expr>		else bool_label break_label continue_label
%type	<expr>		unary_expr ident_expr cast_expr
%type	<mut_expr>	opt_arg_list arg_list
%type   <expr>		arg_expr
%type	<switch_block> switch_block
%type	<symbol>	identifier label

%type	<mut_expr>	identifier_list
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
static const expr_t *break_label;
static const expr_t *continue_label;

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

static specifier_t
parse_qc_params (specifier_t spec, param_t *params)
{
	type_t    **type;
	// .float () foo; is a field holding a function variable rather
	// than a function that returns a float field.
	for (type = &spec.type; *type && is_field (*type);
		 type = &(*type)->t.fldptr.type) {
	}
	type_t     *ret_type = *type;
	*type = 0;

	spec.sym = new_symbol (0);
	spec.sym->type = spec.type;
	spec.type = ret_type;

	spec = function_spec (spec, params);
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

static symbol_t *
qc_nocode_symbol (specifier_t spec, symbol_t *sym)
{
	sym->params = spec.sym->params;
	sym = funtion_sym_type (spec, sym);
	return sym;
}

static symbol_t *
qc_function_symbol (specifier_t spec, symbol_t *sym)
{
	sym = qc_nocode_symbol (spec, sym);
	sym = function_symbol (sym, spec.is_overload, 1);
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

static param_t *
make_selector (const char *selector, struct type_s *type, const char *name)
{
	param_t    *param = new_param (selector, type, name);
	return param;
}

static param_t *
make_qc_param (specifier_t spec, symbol_t *sym)
{
	spec = default_type (spec, sym);
	sym->type = spec.type;
	param_t   *param = new_param (0, sym->type, sym->name);
	return param;
}

static param_t *
make_qc_func_param (specifier_t spec, param_t *params, symbol_t *sym)
{
	spec = parse_qc_params (spec, params);
	sym->type = append_type (spec.sym->type, spec.type);
	param_t   *param = new_param (0, sym->type, sym->name);
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

%expect 2

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
	| declspecs_ts qc_func_params
		{
			$<spec>$ = parse_qc_params ($1, $2);
		}
	  qc_func_decls
	| declspecs ';'
	| error ';'
	| error '}'
	| ';'
	;

qc_func_params
	: '(' copy_spec qc_param_list ')'	{ $$ = check_params ($3); }
	| '(' copy_spec typespec_reserved ')'
		{
			if (!is_void ($3.type)) {
				PARSE_ERROR;
			}
			$$ = 0;
		}
	| '(' copy_spec ')'					{ $$ = 0; }
	;

qc_param_list
	: qc_first_param
	| qc_param_list ',' qc_param
		{
			$$ = append_params ($1, $3);
		}
	;
// quakec function parameters cannot use a typedef as the return type
// in the first parameter (really, they should't use any as standard quakec
// doesn't support typedef, but that seems overly restrictive), howevery,
// they can stil use a typedef first parameter. This is due to grammar issues
qc_first_param
	: typespec identifier
		{
			$$ = make_qc_param ($1, $2);
		}
	| typespec_reserved qc_func_params identifier
		{
			$$ = make_qc_func_param ($1, $2, $3);
		}
	| ELLIPSIS	{ $$ = make_ellipsis (); }
	;

qc_param
	: typespec identifier
		{
			$$ = make_qc_param ($1, $2);
		}
	| typespec qc_func_params identifier
		{
			$$ = make_qc_func_param ($1, $2, $3);
		}
	| ELLIPSIS	{ $$ = make_ellipsis (); }
	;

/*	This rule is used only to get an action before both qc_func_decl and
	qc_func_decl_term so that the function spec can be inherited.
*/
qc_comma
	: ',' { $$ = $<spec>0; }
	;

qc_func_decls
	: qc_func_decl_list qc_comma qc_func_decl_term
	| qc_func_decl_term
	;

qc_func_decl_term
	: qc_nocode_func ';'
	| qc_code_func ';'
	| qc_code_func %prec IFX
	;

qc_func_decl_list
	: qc_func_decl_list qc_comma qc_func_decl
	| qc_func_decl
	;

qc_func_decl
	: qc_nocode_func
	| qc_code_func
	;

qc_nocode_func
	: identifier '=' '#' expr
		{
			specifier_t spec = $<spec>0;
			symbol_t   *sym = $1;
			const expr_t *expr = $4;
			sym = qc_function_symbol (spec, sym);
			build_builtin_function (sym, expr, 0, spec.storage);
		}
	| identifier '=' expr
		{
			specifier_t spec = $<spec>0;
			spec.sym = $1;
			spec.sym->params = spec.params;
			spec.sym->type = find_type (spec.type);
			spec.is_function = 0;
			declare_symbol (spec, $3, current_symtab);
		}
	| identifier
		{
			specifier_t spec = $<spec>0;
			symbol_t   *sym = $1;
			if (!local_expr && !is_field (spec.sym->type)) {
				sym = qc_function_symbol (spec, sym);
			} else {
				sym = qc_nocode_symbol (spec, sym);
			}
			if (!local_expr && !is_field (sym->type)) {
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

qc_code_func
	: identifier '=' optional_state_expr
	  save_storage
		{
			$<symtab>$ = current_symtab;
			specifier_t spec = $<spec>0;
			symbol_t   *sym = $1;
			sym = qc_function_symbol (spec, sym);
			current_func = begin_function (sym, 0, current_symtab, 0,
										   spec.storage);
			current_symtab = current_func->locals;
			current_storage = sc_local;
			$1 = sym;
		}
	  compound_statement
		{
			build_code_function ($1, $3, $6);
			current_symtab = $<symtab>5;
			current_storage = $4.storage;
			current_func = 0;
		}
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
	| OBJECT_NAME protocolrefs
		{
			if ($2) {
				type_t      type = *type_id.t.fldptr.type;
				type.next = 0;
				type.protos = $2;
				$$ = make_spec (pointer_type (find_type (&type)), 0, 0, 0);
			} else {
				$$ = make_spec (&type_id, 0, 0, 0);
			}
			$$.sym = $1.sym;
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
	: method_optional_state_expr
		{
			specifier_t spec = default_type ($<spec>0, $<spec>0.sym);
			symbol_t   *sym = funtion_sym_type (spec, spec.sym);
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
	| declspecs_ts local_expr qc_func_params
		{
			$<spec>$ = parse_qc_params ($1, $3);
		}
	  qc_func_decls
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

method_optional_state_expr
	: /* emtpy */						{ $$ = 0; }
	| SHR vector_expr					{ $$ = build_state_expr ($2); }
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
	| SWITCH break_label '(' comma_expr switch_block ')' compound_statement
		{
			$$ = switch_expr (switch_block, break_label, $7);
			switch_block = $5;
			break_label = $2;
		}
	| GOTO NAME
		{
			const expr_t *label = named_label_expr ($2);
			$$ = goto_expr (label);
		}
	| IF not '(' texpr ')' flush_dag statement %prec IFX
		{
			$$ = build_if_statement ($2, $4, $7, 0, 0);
		}
	| IF not '(' texpr ')' flush_dag statement else statement
		{
			$$ = build_if_statement ($2, $4, $7, $8, $9);
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
	| WHILE break_label continue_label not '(' texpr ')' flush_dag statement
		{
			$$ = build_while_statement ($4, $6, $9, break_label,
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
	| ALGEBRA '(' TYPE_SPEC '(' expr_list ')' ')'
		{
			auto algebra = algebra_type ($3.type, $5);
			current_symtab = algebra_scope (algebra, current_symtab);
		}
	  compound_statement
		{
			current_symtab = current_symtab->parent;
			$$ = $9;
		}
	| ALGEBRA '(' TYPE_SPEC ')'
		{
			auto algebra = algebra_type ($3.type, 0);
			current_symtab = algebra_scope (algebra, current_symtab);
		}
	  compound_statement
		{
			current_symtab = current_symtab->parent;
			$$ = $6;
		}
	| ALGEBRA '(' TYPE_NAME ')'
		{
			auto algebra = $3.type;
			current_symtab = algebra_scope (algebra, current_symtab);
		}
	  compound_statement
		{
			current_symtab = current_symtab->parent;
			$$ = $6;
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
	: ELSE flush_dag
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
	| unary_expr REVERSE			{ $$ = unary_expr (QC_REVERSE, $1); }
	| DUAL cast_expr %prec UNARY	{ $$ = unary_expr (QC_DUAL, $2); }
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
	| obj_expr					{ $$ = $1; }
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
	| expr AND flush_dag bool_label expr{ $$ = bool_expr (QC_AND, $4, $1, $5); }
	| expr OR flush_dag bool_label expr	{ $$ = bool_expr (QC_OR,  $4, $1, $5); }
	| expr EQ expr				{ $$ = binary_expr (QC_EQ,  $1, $3); }
	| expr NE expr				{ $$ = binary_expr (QC_NE,  $1, $3); }
	| expr LE expr				{ $$ = binary_expr (QC_LE,  $1, $3); }
	| expr GE expr				{ $$ = binary_expr (QC_GE,  $1, $3); }
	| expr LT expr				{ $$ = binary_expr (QC_LT,  $1, $3); }
	| expr GT expr				{ $$ = binary_expr (QC_GT,  $1, $3); }
	| expr SHL expr				{ $$ = binary_expr (QC_SHL, $1, $3); }
	| expr SHR expr				{ $$ = binary_expr (QC_SHR, $1, $3); }
	| expr '+' expr				{ $$ = binary_expr ('+', $1, $3); }
	| expr '-' expr				{ $$ = binary_expr ('-', $1, $3); }
	| expr '*' expr				{ $$ = binary_expr ('*', $1, $3); }
	| expr '/' expr				{ $$ = binary_expr ('/', $1, $3); }
	| expr '&' expr				{ $$ = binary_expr ('&', $1, $3); }
	| expr '|' expr				{ $$ = binary_expr ('|', $1, $3); }
	| expr '^' expr				{ $$ = binary_expr ('^', $1, $3); }
	| expr '%' expr				{ $$ = binary_expr ('%', $1, $3); }
	| expr MOD expr				{ $$ = binary_expr (QC_MOD, $1, $3); }
	| expr GEOMETRIC expr		{ $$ = binary_expr (QC_GEOMETRIC, $1, $3); }
	| expr HADAMARD expr		{ $$ = binary_expr (QC_HADAMARD, $1, $3); }
	| expr CROSS expr			{ $$ = binary_expr (QC_CROSS, $1, $3); }
	| expr DOT expr				{ $$ = binary_expr (QC_DOT, $1, $3); }
	| expr WEDGE expr			{ $$ = binary_expr (QC_WEDGE, $1, $3); }
	| expr REGRESSIVE expr		{ $$ = binary_expr (QC_REGRESSIVE, $1, $3); }
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
			$$ = append_expr (new_block_expr (0), new_symbol_expr ($1));
		}
	| identifier_list ',' identifier
		{
			$$ = append_expr ($1, new_symbol_expr ($3));
		}
	;

classdecl
	: CLASS identifier_list ';'
		{
			for (auto li = $2->block.head; li; li = li->next) {
				auto e = li->expr;
				get_class (e->symbol, 1);
				if (!e->symbol->table)
					symtab_addsymbol (current_symtab, e->symbol);
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
		}
	| IMPLEMENTATION class_name			{ class_begin (&$2->class_type); }
	  '{'								{ $<class>$ = $2; }
	  ivar_decl_list '}'
		{
			class_check_ivars ($2, $6);
		}
	| IMPLEMENTATION class_name			{ class_begin (&$2->class_type); }
	| IMPLEMENTATION class_with_super	{ class_begin (&$2->class_type); }
	  '{'								{ $<class>$ = $2; }
	  ivar_decl_list '}'
		{
			class_check_ivars ($2, $6);
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
		}
	;

protocol
	: PROTOCOL					{ $<class_type>$ = current_class; }
	;

protocolrefs
	: /* emtpy */				{ $$ = 0; }
	| LT 						{ $<protocol_list>$ = new_protocol_list (); }
	  protocol_list GT			{ $$ = $3; }
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
			int         base = 0;
			if ($<class>0->super_class) {
				base = type_size ($<class>0->super_class->type);
			}
			build_struct ('s', 0, $$, 0, base);
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
	: declspecs_nosc_ts ivars
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
	| declspecs_nosc_nots notype_ivars
	;

ivars
	: ivar_declarator { }
	| ivars ',' { $<spec>$ = $<spec>0; } ivar_declarator
	;

notype_ivars
	: notype_ivar_declarator { }
	| notype_ivars ',' { $<spec>$ = $<spec>0; }
	  notype_ivar_declarator
	;

ivar_declarator
	: declarator
		{
			declare_field ($1, current_symtab);
		}
	| declarator ':' expr
	| ':' expr
	;

notype_ivar_declarator
	: notype_declarator { internal_error (0, "not implemented"); }
	| notype_declarator ':' expr
	| ':' expr
	;

methoddef
	: ci methoddecl method_optional_state_expr
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
		{ $$ = new_method (&type_id, make_selector ("", 0, 0), 0); }
	| '+' error ';'
		{ $$ = new_method (&type_id, make_selector ("", 0, 0), 0); }
	| '-' methoddecl ';'
		{
			$2->instance = 1;
			$$ = $2;
		}
	;

methoddecl
	: '(' typename ')' unaryselector
		{ $$ = new_method ($2.type, $4, 0); }
	| unaryselector
		{ $$ = new_method (&type_id, $1, 0); }
	| '(' typename ')' keywordselector optional_param_list
		{ $$ = new_method ($2.type, $4, $5); }
	| keywordselector optional_param_list
		{ $$ = new_method (&type_id, $1, $2); }
	;

optional_param_list
	: /* empty */				{ $$ = 0; }
	| ',' param_list			{ $$ = $2; }
	;

unaryselector
	: selector					{ $$ = make_selector ($1->name, 0, 0); }
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
	| OBJECT_NAME				{ $$ = new_symbol (qc_yytext); }
	| TYPE_SPEC					{ $$ = new_symbol (qc_yytext); }
	| TYPE_NAME					{ $$ = $1.sym; }
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
	: selector ':' '(' typename ')' identifier
		{ $$ = make_selector ($1->name, $4.type, $6->name); }
	| selector ':' identifier
		{ $$ = make_selector ($1->name, &type_id, $3->name); }
	| ':' '(' typename ')' identifier
		{ $$ = make_selector ("", $3.type, $5->name); }
	| ':' identifier
		{ $$ = make_selector ("", &type_id, $2->name); }
	;

obj_expr
	: obj_messageexpr
	| SELECTOR '(' selectorarg ')'	{ $$ = selector_expr ($3); }
	| PROTOCOL '(' identifier ')'	{ $$ = protocol_expr ($3->name); }
	| ENCODE '(' typename ')'		{ $$ = encode_expr ($3.type); }
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
