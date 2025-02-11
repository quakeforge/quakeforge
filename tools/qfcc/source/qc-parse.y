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
#include "tools/qfcc/include/grab.h"
#include "tools/qfcc/include/image.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/switch.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

#define qc_yytext qc_yyget_text (ctx->scanner)
char *qc_yyget_text (rua_ctx_t *ctx);

static void
yyerror (YYLTYPE *yylloc, rua_ctx_t *ctx, const char *s)
{
#ifdef QC_YYERROR_VERBOSE
	error (0, "%s %s\n", qc_yytext, s);
#else
	error (0, "%s before %s", s, qc_yytext);
#endif
}

static void
parse_error (rua_ctx_t *ctx)
{
	error (0, "parse error before %s", qc_yytext);
}

#define PARSE_ERROR do { parse_error (ctx); YYERROR; } while (0)

#define YYLLOC_DEFAULT(Current, Rhs, N) RUA_LOC_DEFAULT(Current, Rhs, N)
#define YYLOCATION_PRINT rua_print_location

int yylex (YYSTYPE *yylval, YYLTYPE *yylloc);

%}

%code requires { #define qc_yypstate rua_yypstate }

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
%left           HADAMARD CROSS DOT OUTER WEDGE REGRESSIVE
%right	<op>	SIZEOF UNARY INCOP REVERSE STAR DUAL UNDUAL
%left			HYPERUNARY
%left			'.' '(' '['

%token	<expr>		VALUE STRING TOKEN
%token              TRUE FALSE
%token				ELLIPSIS
%token				RESERVED
// end of common tokens

%token	<symbol>	CLASS_NAME NAME

%token				LOCAL WHILE DO IF ELSE FOR BREAK CONTINUE
%token				RETURN AT_RETURN
%token				NIL GOTO SWITCH CASE DEFAULT ENUM ALGEBRA IMAGE
%token				ARGS TYPEDEF EXTERN STATIC SYSTEM OVERLOAD NOT ATTRIBUTE
%token	<op>		STRUCT
%token				HANDLE INTRINSIC
%token	<spec>		TYPE_SPEC TYPE_NAME TYPE_QUAL
%token	<spec>		OBJECT_NAME
%token				CLASS DEFS ENCODE END IMPLEMENTATION INTERFACE PRIVATE
%token				PROTECTED PROTOCOL PUBLIC SELECTOR REFERENCE SELF THIS

%token				GENERIC CONSTRUCT
%token				AT_FUNCTION AT_FIELD AT_POINTER AT_ARRAY
%token				AT_BASE AT_WIDTH AT_VECTOR AT_ROWS AT_COLS AT_MATRIX
%token				AT_INT AT_UINT AT_BOOL AT_FLOAT

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
%type	<symbol>	generic_param
%type	<op>		type_func type_op
%type	<expr>		type_function type_param type_ref
%type	<spec>		type_ref_spec
%type	<expr>		generic_type
%type	<mut_expr>	type_list type_param_list
%type	<expr>		initdecl notype_initdecl
%type	<mut_expr>	initdecls notype_initdecls

%type	<attribute>	attribute_list attribute attrfunc

%type	<param>		function_params
%type   <param>		qc_func_params qc_param_list qc_first_param qc_param

%type	<symbol>	tag
%type	<spec>		struct_specifier struct_list
%type	<spec>		enum_specifier algebra_specifier image_specifier
%type	<symbol>	optional_enum_list enum_list enumerator_list enumerator
%type	<symbol>	enum_init
%type	<expr>		array_decl

%type	<expr>		const string

%type	<spec>		ivar_decl
%type   <expr>		decl
%type	<spec>		ivars
%type	<param>		param_list parameter_list parameter
%type	<symbol>	methoddef
%type	<expr>		var_initializer local_def

%type	<expr>		opt_init_semi opt_expr comma_expr
%type   <expr>		expr
%type	<expr>		compound_init
%type	<expr>		opt_cast
%type   <mut_expr>	element_list expr_list
%type	<designator> designator designator_spec
%type	<element>	element
%type	<expr>		method_optional_state_expr optional_state_expr
%type	<expr>		vector_expr
%type	<expr>		intrinsic
%type	<expr>		statement
%type	<mut_expr>	statement_list compound_statement compound_statement_ns
%type	<mut_expr>	new_block new_scope
%type	<expr>		else line break_label continue_label
%type	<expr>		unary_expr ident_expr cast_expr
%type	<mut_expr>	arg_list
%type   <expr>		opt_arg_list arg_expr
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

static const expr_t *break_label;
static const expr_t *continue_label;
static bool generic_scope, generic_block;
static symtab_t *generic_symtab;
static expr_t *local_expr;

static void
end_generic_scope (void)
{
	if (generic_symtab != current_symtab || !generic_symtab->parent) {
		internal_error (0, "scope stack tangled?");
	}
	current_symtab = generic_symtab->parent;
	generic_symtab = nullptr;
	generic_scope = false;
}

static void
restore_storage (specifier_t st)
{
	if (generic_block && !st.is_generic) {
		end_generic_scope ();
	}
	generic_block = st.is_generic;
	current_storage = st.storage;
}

static specifier_t
type_spec (const type_t *type)
{
	specifier_t spec = {
		.type = type,
	};
	return spec;
}

static const expr_t *
type_property (specifier_t spec, const attribute_t *property)
{
	auto type_expr = spec.type_expr;
	if (!type_expr) {
		type_expr = new_type_expr (spec.type);
	}
	auto params = new_list_expr (type_expr);
	auto prop = new_type_expr (nullptr);
	prop->typ.property = property;
	expr_append_expr (params, prop);
	return new_type_function (QC_ATTRIBUTE, params);
}

static specifier_t
storage_spec (storage_class_t storage)
{
	specifier_t spec = {
		.storage = storage,
	};
	return spec;
}

static specifier_t
typedef_spec (void)
{
	specifier_t spec = {
		.storage = sc_global,
		.is_typedef = true,
	};
	return spec;
}

static specifier_t
overload_spec (void)
{
	specifier_t spec = {
		.storage = current_storage,
		.is_overload = true,
	};
	return spec;
}

static specifier_t
generic_spec (rua_ctx_t *ctx)
{
	specifier_t spec = {
		.storage = current_storage,
		.symtab = new_symtab (current_symtab, stab_local),
		.is_generic = true,
	};
	generic_symtab = spec.symtab;
	if (current_target.setup_intrinsic_symtab) {
		current_target.setup_intrinsic_symtab (generic_symtab);
	}
	return spec;
}

static int
storage_auto (specifier_t spec)
{
	return spec.storage == sc_global || spec.storage == sc_local;
}

static bool
spec_type (specifier_t spec)
{
	return spec.type || spec.type_expr;
}

static specifier_t
spec_merge (specifier_t spec, specifier_t new)
{
	if (spec_type (new)) {
		// deal with "type <type_name>"
		if (!spec_type (spec) || new.sym) {
			spec.sym = new.sym;
			if (!spec_type (spec)) {
				spec.type = new.type;
				spec.type_expr = new.type_expr;
			}
		} else if (!spec.multi_type) {
			error (0, "two or more data types in declaration specifiers");
			spec.multi_type = true;
		}
	}
	spec.type_list = new.type_list;
	if (new.is_typedef || !storage_auto (new)) {
		if ((spec.is_typedef || !storage_auto (spec)) && !spec.multi_store) {
			error (0, "multiple storage classes in declaration specifiers");
			spec.multi_store = true;
		}
		spec.storage = new.storage;
		spec.is_typedef = new.is_typedef;
	}
	if (new.is_generic) {
		if (spec.is_generic && !spec.multi_generic) {
			error (0, "multiple @generic in declaration");
			spec.multi_generic = true;
		}
	}
	if ((new.is_unsigned && spec.is_signed)
		|| (new.is_signed && spec.is_unsigned)) {
		if (!spec.multi_type) {
			error (0, "both signed and unsigned in declaration specifiers");
			spec.multi_type = true;
		}
	}
	if ((new.is_long && spec.is_short) || (new.is_short && spec.is_long)) {
		if (!spec.multi_store) {
			error (0, "both long and short in declaration specifiers");
			spec.multi_store = true;
		}
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

static specifier_t
resolve_type_spec (specifier_t spec, rua_ctx_t *ctx)
{
	spec = spec_process (spec, ctx);
	auto type = spec.type;
	if (spec.type_expr) {
		type = resolve_type (spec.type_expr, ctx);
		spec.type_expr = nullptr;
	}
	spec.type = find_type (type);
	return spec;
}

static specifier_t
typename_spec (specifier_t spec, rua_ctx_t *ctx)
{
	spec = default_type (spec, 0);
	return spec;
}

static specifier_t
function_spec (specifier_t spec, param_t *parameters)
{
	// return type will be filled in when building the final type
	// FIXME not sure I like this setup for @function
	auto params = new_type_expr (parse_params (0, parameters));
	auto type_expr = new_type_function (QC_AT_FUNCTION, params);
	if (spec.type_list) {
		expr_append_expr (spec.type_list, type_expr);
	} else {
		spec.is_function = true;
		spec.type_list = new_list_expr (type_expr);
	}

	spec.params = parameters;
	spec.is_generic = generic_scope;
	spec.is_generic_block = generic_block;
	spec.symtab = generic_symtab;
	return spec;
}

static specifier_t
array_spec (specifier_t spec, const expr_t *size)
{
	// element type will be filled in when building the final type
	auto params = new_list_expr (size);
	auto type_expr = new_type_function (QC_AT_ARRAY, params);
	if (spec.type_list) {
		expr_append_expr (spec.type_list, type_expr);
	} else {
		spec.type_list = new_list_expr (type_expr);
	}
	return spec;
}

static specifier_t
pointer_spec (specifier_t quals, specifier_t spec)
{
	// referenced type will be filled in when building the final type
	auto type_expr = new_type_function (QC_AT_POINTER, nullptr);
	if (spec.type_list) {
		expr_append_expr (spec.type_list, type_expr);
	} else {
		spec.type_list = new_list_expr (type_expr);
	}
	return spec;
}

static specifier_t
field_spec (specifier_t spec)
{
	// referenced type will be filled in when building the final type
	auto type_expr = new_type_function (QC_AT_FIELD, nullptr);
	if (spec.type_list) {
		expr_prepend_expr (spec.type_list, type_expr);
	} else {
		spec.type_list = new_list_expr (type_expr);
	}
	return spec;
}

static specifier_t
qc_function_spec (specifier_t spec, param_t *params)
{
#if 0
	//FIXME this is borked
	// .float () foo; is a field holding a function variable rather
	// than a function that returns a float field.
	// FIXME I think this breaks fields holding functions that return fields
	// but that would require some messy ()s to get parsing anyway, and it can
	// wait until such is needed (if ever).
	const type_t *field_chain = nullptr;
	const type_t *ret_type = spec.type;
	while (ret_type && is_field (ret_type)) {
		field_chain = field_type (field_chain);
		ret_type = ret_type->fldptr.type;
	}

	// qc-style functions are known to be functions before the symbol is seen,
	// so provide an unnamed symbol to hold any field type information
	spec.sym = new_symbol (0);
	spec.sym->type = field_chain;
	spec.type = ret_type;
#endif

	spec = function_spec (spec, params);
	return spec;
}

static specifier_t
qc_set_symbol (specifier_t spec, symbol_t *sym, rua_ctx_t *ctx)
{
	spec = spec_process (spec, ctx);
	sym->type = spec.type;
	spec.sym = sym;
	return spec;
}

static param_t *
make_ellipsis (void)
{
	return new_param (0, 0, 0);
}

static param_t *
make_param (specifier_t spec, rua_ctx_t *ctx)
{
	//FIXME should not be sc_global
	//FIXME if (spec.storage == sc_global || spec.storage == sc_extern) {
	if (spec.storage < sc_inout) {
		spec.storage = sc_param;
	}

	param_t *param;
	if (generic_block && spec.type_expr) {
		param = new_generic_param (spec.type_expr, spec.sym->name);
	} else if (spec.sym) {
		spec = spec_process (spec, ctx);
		spec.type = find_type (append_type (spec.sym->type, spec.type));
		param = new_param (nullptr, spec.type, spec.sym->name);
	} else {
		spec = spec_process (spec, ctx);
		param = new_param (nullptr, spec.type, nullptr);
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

static param_t *
make_selector (const char *selector, const type_t *type, const char *name)
{
	param_t    *param = new_param (selector, type, name);
	return param;
}

static param_t *
make_qc_param (specifier_t spec, symbol_t *sym, rua_ctx_t *ctx)
{
	sym->type = nullptr;
	spec.sym = sym;
	return make_param (spec, ctx);
}

static param_t *
make_qc_func_param (specifier_t spec, param_t *params, symbol_t *sym,
					rua_ctx_t *ctx)
{
	spec = qc_function_spec (spec, params);
	spec = spec_process (spec, ctx);
	param_t   *param = new_param (0, spec.type, sym->name);
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
	if (!spec.type->symtab || spec.type->symtab->parent) {
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
use_type_name (specifier_t spec, rua_ctx_t *ctx)
{
	spec.sym = new_symbol (spec.sym->name);
	spec.sym->type = spec.type;
	spec.sym->sy_type = sy_name;
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
check_specifiers (specifier_t spec, rua_ctx_t *ctx)
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
			if (!use_type_name (spec, ctx)) {
				error (0, "%s redeclared as different kind of symbol",
					   spec.sym->name);
			}
		}
	}
}

static const expr_t *
decl_expr (specifier_t spec, const expr_t *init, rua_ctx_t *ctx)
{
	auto sym = spec.sym;
	spec.sym = nullptr;
	auto decl = new_decl_expr (spec, current_symtab);
	return append_decl (decl, sym, init);
}

static const expr_t *
forward_decl_expr (symbol_t *tag, int sueh, const type_t *base_type)
{
	symbol_t *sym;

	if (sueh == 's' || sueh == 'u') {
		sym = find_struct (sueh, tag, nullptr);
		sym->type = find_type (sym->type);
	} else if (sueh == 'e') {
		sym = find_enum (tag);
	} else if (sueh == 'h') {
		sym = find_handle (tag, base_type);
		sym->type = find_type (sym->type);
	} else {
		internal_error (0, "invalude decl thing");
	}
	return new_symbol_expr (sym);
}

static symtab_t *
pop_scope (symtab_t *current)
{
	auto parent = current->parent;
	if (!parent) {
		internal_error (0, "scope stack underflow");
	}
	if (parent->type == stab_bypass) {
		if (!parent->parent) {
			internal_error (0, "bypass scope with no parent");
		}
		// reconnect the current scope to the parent of the bypassed scope
		current->parent = parent->parent;
	}
	return parent;
}

static const expr_t *
number_as_symbol (const rua_tok_t *tok, rua_ctx_t *ctx)
{
	return new_name_expr (tok->text);
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
			if (!current_symtab) {
				current_symtab = pr.symtab;
			}
		}
	| external_def_list external_def
		{
			if (generic_scope && !generic_block) {
				end_generic_scope ();
			}
		}
	| external_def_list obj_def
	;

external_def
	: fndef
	| datadef
	| storage_class '{' save_storage
		{
			current_storage = $1.storage;
			generic_block = generic_scope;
		}
	  external_def_list '}' ';'
		{
			restore_storage ($3);
		}
	;

fndef
	: declspecs_ts declarator function_body
	| declspecs_nots notype_declarator function_body
	| defspecs notype_declarator function_body
	;

datadef
	: defspecs notype_initdecls ';'			{ decl_process ($2, ctx); }
	| declspecs_nots notype_initdecls ';'	{ decl_process ($2, ctx); }
	| declspecs_ts initdecls ';'			{ decl_process ($2, ctx); }
	| declspecs_ts qc_func_params
		{
			$<spec>$ = qc_function_spec ($1, $2);
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
// doesn't support typedef, but that seems overly restrictive), however,
// they can stil use a typedef first parameter. This is due to grammar issues
qc_first_param
	: typespec identifier
		{
			$$ = make_qc_param ($1, $2, ctx);
		}
	| typespec_reserved qc_func_params identifier
		{
			$$ = make_qc_func_param ($1, $2, $3, ctx);
		}
	| ELLIPSIS
		{
			if (options.code.no_vararg) {
				PARSE_ERROR;
			}
			$$ = make_ellipsis ();
		}
	;

qc_param
	: typespec identifier
		{
			$$ = make_qc_param ($1, $2, ctx);
		}
	| typespec qc_func_params identifier
		{
			$$ = make_qc_func_param ($1, $2, $3, ctx);
		}
	| ELLIPSIS
		{
			if (options.code.no_vararg) {
				PARSE_ERROR;
			}
			$$ = make_ellipsis ();
		}
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
			specifier_t spec = qc_set_symbol ($<spec>0, $1, ctx);
			const expr_t *bi_val = expr_process ($4, ctx);

			spec.is_overload |= ctx->language->always_overload;
			spec.sym = function_symbol (spec, ctx);
			build_builtin_function (spec, nullptr, bi_val);
		}
	| identifier '=' intrinsic
		{
			specifier_t spec = qc_set_symbol ($<spec>0, $1, ctx);
			build_intrinsic_function (spec, $3, ctx);
		}
	| identifier '=' expr
		{
			specifier_t spec = qc_set_symbol ($<spec>0, $1, ctx);
			const expr_t *expr = $3;

			declare_symbol (spec, expr, current_symtab, local_expr, ctx);
		}
	| identifier
		{
			specifier_t spec = qc_set_symbol ($<spec>0, $1, ctx);
			declare_symbol (spec, nullptr, current_symtab, local_expr, ctx);
		}
	;

qc_code_func
	: identifier '=' optional_state_expr
	  save_storage
		{
			specifier_t spec = qc_set_symbol ($<spec>0, $1, ctx);
			auto fs = (funcstate_t) {
				.function = current_func,
			};
			spec.is_overload |= ctx->language->always_overload;
			spec.sym = function_symbol (spec, ctx);
			current_func = begin_function (spec, nullptr, current_symtab, ctx);
			current_symtab = current_func->locals;
			current_storage = sc_local;
			fs.spec = spec;
			$<funcstate>$ = fs;
		}
	  compound_statement_ns
		{
			auto fs = $<funcstate>5;
			build_code_function (fs.spec, $3, $6, ctx);
			current_symtab = pop_scope (current_func->parameters);
			current_func = fs.function;
			restore_storage ($4);
		}
	;

declarator
	: after_type_declarator
	| notype_declarator
	;

// reuses typedef or class name
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

// does not reuse a typedef or class name
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
			auto spec = $<spec>0;
			spec.sym = new_symbol ($1->name);
			$$ = spec;
		}
	| NOT
		{
			$$ = $<spec>0;
			$$.sym = new_symbol ("not");
		}
	;

initdecls
	: initdecl								{ $$ = new_list_expr ($1); }
	| initdecls ',' { $<spec>$ = $<spec>0; } initdecl
		{
			$$ = expr_append_expr ($1, $4);
		}
	;

initdecl
	: declarator '=' var_initializer	{ $$ = decl_expr ($1, $3, ctx); }
	| declarator						{ $$ = decl_expr ($1, nullptr, ctx); }
	;

notype_initdecls
	: notype_initdecl						{ $$ = new_list_expr ($1); }
	| notype_initdecls ',' { $<spec>$ = $<spec>0; } notype_initdecl
		{
			$$ = expr_append_expr ($1, $4);
		}
	;

notype_initdecl
	: notype_declarator '=' var_initializer	{ $$ = decl_expr ($1, $3, ctx); }
	| notype_declarator					{ $$ = decl_expr ($1, nullptr, ctx); }
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
	| type_function
		{
			if (generic_scope) {
				$$ = (specifier_t) {
					.type_expr = $1,
				};
			} else {
				auto type = resolve_type ($1, ctx);
				$$ = type_spec (type);
			}
		}
	| algebra_specifier %prec LOW
	| algebra_specifier '.' attribute
		{
			$$ = type_spec (algebra_subtype ($1.type, $3));
		}
	| image_specifier
	| enum_specifier
	| struct_specifier
	// NOTE: fields don't parse the way they should. This is not a problem
	// for basic types, but functions need special treatment
	| '.' typespec_reserved
		{
			$$ = field_spec ($2);
		}
	;

typespec_nonreserved
	: TYPE_NAME %prec LOW
	| TYPE_NAME[spec] '.' attribute
		{
			auto te = type_property ($spec, $attribute);
			$$ = (specifier_t) { .type_expr = te };
		}
	| OBJECT_NAME protocolrefs
		{
			if ($2) {
				type_t      type = *type_id.fldptr.type;
				type.next = 0;
				type.protos = $2;
				$$ = type_spec (pointer_type (find_type (&type)));
			} else {
				$$ = type_spec (&type_id);
			}
		}
	| CLASS_NAME protocolrefs
		{
			if ($2) {
				type_t      type = *$1->type;
				type.next = 0;
				type.protos = $2;
				$$ = type_spec (find_type (&type));
			} else {
				$$ = type_spec ($1->type);
			}
		}
	// NOTE: fields don't parse the way they should. This is not a problem
	// for basic types, but functions need special treatment
	| '.' typespec_nonreserved
		{
			$$ = field_spec ($2);
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
			$$.is_generic = generic_block;
		}
	;

function_body
	: method_optional_state_expr[state]
		{
			specifier_t spec = $<spec>0;
			spec.is_overload |= ctx->language->always_overload;
			spec.sym = function_symbol (spec, ctx);
			$<spec>$ = spec;
		}
	  save_storage[storage]
		{
			$<funcstate>$ = (funcstate_t) {
				.function = current_func,
			};
			auto spec = $<spec>2;
			current_func = begin_function (spec, nullptr, current_symtab, ctx);
			current_symtab = current_func->locals;
			current_storage = sc_local;
		}
	  compound_statement_ns[body]
		{
			auto spec = $<spec>2;
			auto funcstate = $<funcstate>4;
			build_code_function (spec, $state, $body, ctx);
			current_symtab = pop_scope (current_func->parameters);
			current_func = funcstate.function;
			restore_storage ($storage);
		}
	| '=' '#' expr ';'
		{
			specifier_t spec = $<spec>0;
			const expr_t *bi_val = expr_process ($3, ctx);

			spec.is_overload |= ctx->language->always_overload;
			spec.sym = function_symbol (spec, ctx);
			build_builtin_function (spec, nullptr, bi_val);
		}
	| '=' intrinsic
		{
			specifier_t spec = $<spec>0;
			build_intrinsic_function (spec, $2, ctx);
		}
	;

intrinsic
	: INTRINSIC '(' expr_list ')'	{ $$ = new_intrinsic_expr ($3); }
	| INTRINSIC '(' expr_list ')' vector_expr
		{
			auto e = new_intrinsic_expr ($3);
			e->intrinsic.extra = $5;
			$$ = e;
		}
	;

storage_class
	: EXTERN					{ $$ = storage_spec (sc_extern); }
	| STATIC					{ $$ = storage_spec (sc_static); }
	| SYSTEM					{ $$ = storage_spec (sc_system); }
	| TYPEDEF					{ $$ = typedef_spec (); }
	| OVERLOAD					{ $$ = overload_spec (); }
	| GENERIC '('				{ $<spec>$ = generic_spec (ctx); }
	  generic_param_list ')'
		{
			$$ = $<spec>3;

			if (generic_scope) {
				error (0, "multiple @generic in declaration");
				$$.multi_generic = true;
			} else {
				generic_scope = true;
			}
			generic_symtab->type = stab_bypass;
			generic_symtab->parent = current_symtab;
			current_symtab = generic_symtab;
		}
	| ATTRIBUTE '(' attribute_list ')'
		{
			$$ = (specifier_t) { .attributes = $3 };
		}
	;

generic_param_list
	: generic_param
		{
			auto spec = $<spec>0;
			symtab_addsymbol (spec.symtab, $1);
		}
	| generic_param_list ',' generic_param
		{
			auto spec = $<spec>0;
			symtab_addsymbol (spec.symtab, $3);
		}
	;

generic_param
	: NAME						{ $$ = type_parameter ($1, nullptr); }
	| NAME '=' generic_type		{ $$ = type_parameter ($1, $3); }
	;

generic_type
	: type_function				{ $$ = $1; }
	| '[' type_list ']'			{ $$ = $2; }
	;

type_function
	: type_func '(' type_param_list ')'	{ $$ = new_type_function ($1, $3); }
	;

type_func
	: AT_FIELD					{ $$ = QC_AT_FIELD; }
	| AT_FUNCTION				{ $$ = QC_AT_FUNCTION; }
	| AT_POINTER				{ $$ = QC_AT_POINTER; }
	| REFERENCE 				{ $$ = QC_REFERENCE; }
	| AT_ARRAY					{ $$ = QC_AT_ARRAY; }
	| AT_BASE					{ $$ = QC_AT_BASE; }
	| AT_VECTOR					{ $$ = QC_AT_VECTOR; }
	| AT_MATRIX					{ $$ = QC_AT_MATRIX; }
	| AT_INT					{ $$ = QC_AT_INT; }
	| AT_UINT					{ $$ = QC_AT_UINT; }
	| AT_BOOL					{ $$ = QC_AT_BOOL; }
	| AT_FLOAT					{ $$ = QC_AT_FLOAT; }
	;

type_op
	: AT_WIDTH					{ $$ = QC_AT_WIDTH; }
	| AT_ROWS					{ $$ = QC_AT_ROWS; }
	| AT_COLS					{ $$ = QC_AT_COLS; }
	;

type_param_list
	: type_param						{ $$ = new_list_expr ($1); }
	| type_param_list ',' type_param	{ $$ = expr_append_expr ($1, $3); }
	;

type_param
	: type_function
	| type_ref
	| expr
	;

type_list
	: type_ref							{ $$ = new_list_expr ($1); }
	| type_list ',' type_ref			{ $$ = expr_append_expr ($1, $3); }
	;

type_ref
	: type_ref_spec
		{
			specifier_t spec = default_type ($1, 0);
			$$ = new_type_expr (spec.type);
		}
	| STRUCT tag				{ $$ = forward_decl_expr ($2, $1, nullptr); }
	| ENUM tag					{ $$ = forward_decl_expr ($2, 'e', nullptr); }
	| handle tag				{ $$ = forward_decl_expr ($2, 'h', $1.type); }
	| CLASS_NAME				{ $$ = new_type_expr ($1->type); }
	| TYPE_NAME
		{
			if ($1.type_expr) {
				$$ = $1.type_expr;
			} else {
				$$ = new_type_expr ($1.type);
			}
		}
	| TYPE_NAME[spec] '.' attribute
		{
			$$ = type_property ($spec, $attribute);
		}
	;

type_ref_spec
	: TYPE_SPEC
	| type_ref_spec TYPE_SPEC			{ $$ = spec_merge ($1, $2); }
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

attrfunc
	: NAME %prec LOW			{ $$ = new_attrfunc ($1->name, 0); }
	| NAME '(' expr_list ')'	{ $$ = new_attrfunc ($1->name, $3); }
	;

tag : NAME ;

algebra_specifier
	: ALGEBRA '(' TYPE_SPEC '(' expr_list ')' ')'
		{
			auto spec = type_spec (algebra_type ($3.type, $5));
			$$ = spec;
		}
	| ALGEBRA '(' TYPE_SPEC ')'
		{
			auto spec = type_spec (algebra_type ($3.type, 0));
			$$ = spec;
		}
	;

image_specifier
	: IMAGE '(' TYPE_SPEC[spec] ','
		{
			// allow 2D to be parsed as a symbol instead of 2.0 (double/decimal)
			$<pointer>$ = ctx->language->parse_number;
			ctx->language->parse_number = number_as_symbol;
		}[parse_number]
	  expr_list ')'
		{
			auto type = $spec.type;
			auto spec = type_spec (image_type (type, $expr_list));
			$$ = spec;
			ctx->language->parse_number = $<pointer>parse_number;
		}
	;

enum_specifier
	: ENUM tag optional_enum_list
		{
			$$ = type_spec ($3->type);
			if (!$3->table)
				symtab_addsymbol (current_symtab, $3);
		}
	| ENUM enum_list
		{
			$$ = type_spec ($2->type);
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
			current_symtab = pop_scope (current_symtab);
			$$ = finish_enum ($3);
		}
	;

enum_init
	: /* empty */
		{
			$$ = find_enum ($<symbol>-1);
			start_enum ($$);
			current_symtab = $$->type->symtab;
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
	: identifier
		{
			add_enum ($<symbol>0, $identifier, nullptr);
		}
	| identifier '=' expr
		{
			$expr = expr_process ($expr, ctx);
			add_enum ($<symbol>0, $identifier, $expr);
		}
	;

struct_specifier
	: STRUCT tag struct_list { $$ = $3; }
	| STRUCT {$<symbol>$ = 0;} struct_list { $$ = $3; }
	| STRUCT tag
		{
			symbol_t   *sym;

			sym = find_struct ($1, $2, 0);
			sym->type = find_type (sym->type);
			$$ = type_spec (sym->type);
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
			specifier_t spec = $1;
			symbol_t   *sym = find_handle ($2, spec.type);
			sym->type = find_type (sym->type);
			$$ = type_spec (sym->type);
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
	: HANDLE					{ $$ = type_spec (&type_int); }
	| HANDLE '(' typename ')'	{ $$ = $3; }
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
			current_symtab = pop_scope (symtab);

			if ($<op>1) {
				sym = $<symbol>2;
				sym = build_struct ($<op>1, sym, symtab, 0, 0);
				$$ = type_spec (sym->type);
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
				current_symtab = class_to_struct ($3->type->class,
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
				$1.sym->sy_type = sy_offset;
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
			declare_field ($1, current_symtab, ctx);
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
			if (options.code.no_vararg) {
				PARSE_ERROR;
			}
			$$ = param_append_identifiers ($1, 0, 0);
		}
	| ELLIPSIS
		{
			if (options.code.no_vararg) {
				PARSE_ERROR;
			}
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
			$$ = make_param ($2, ctx);
		}
	| declspecs_ts notype_declarator
		{
			$$ = make_param ($2, ctx);
		}
	| declspecs_ts absdecl
		{
			$$ = make_param ($2, ctx);
		}
	| declspecs_nosc_nots notype_declarator
		{
			$$ = make_param ($2, ctx);
		}
	| declspecs_nosc_nots absdecl
		{
			$$ = make_param ($2, ctx);
		}
	;

absdecl
	: /* empty */
		{
			$$ = $<spec>0;
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
	: declspecs_nosc absdecl	{ $$ = typename_spec ($2, ctx); }
	;

array_decl
	: '[' expr ']'				{ $$ = $expr; }
	| '[' ']'					{ $$ = 0; }
	;

decl
	: declspecs_ts local_expr initdecls seq_semi
		{
			$$ = $3;
		}
	| declspecs_nots local_expr notype_initdecls seq_semi
		{
			$$ = $3;
		}
	| declspecs_ts local_expr qc_func_params
		{
			$<spec>$ = qc_function_spec ($1, $3);
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
	: opt_cast '{' element_list optional_comma '}'
		{
			auto cast = $1;
			$3->compound.type_expr = cast;
			$$ = $3;
		}
	| opt_cast '{' '}'
		{
			auto cast = $1;
			if (cast) {
				auto elements = new_compound_init ();
				elements->compound.type_expr = cast;
				$$ = elements;
			} else {
				$$ = nullptr;
			}
		}
	;

opt_cast
	: '(' typename ')'					{ $$ = new_decl_expr ($2, nullptr); }
	| /*empty*/							{ $$ = nullptr; }
	;

method_optional_state_expr
	: /* emtpy */						{ $$ = 0; }
	| SHR vector_expr					{ $$ = build_state_expr ($2, ctx); }
	;

optional_state_expr
	: /* emtpy */						{ $$ = 0; }
	| vector_expr						{ $$ = build_state_expr ($1, ctx); }
	;

element_list
	: element
		{
			$$ = new_compound_init ();
			append_element ($$, $1);
		}
	| element_list ',' element
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

new_block
	: /* empty */
		{
			auto block = new_block_expr (nullptr);
			block->block.scope = current_symtab;
			$$ = block;
		}

new_scope
	: /* empty */
		{
			auto block = new_block_expr (nullptr);
			block->block.scope = current_symtab;
			if (!options.traditional) {
				block->block.scope = new_symtab (current_symtab, stab_local);
				block->block.scope->space = current_symtab->space;
				current_symtab = block->block.scope;
			}
			$$ = block;
		}
	;

end_scope
	: /* empty */
		{
			if (!options.traditional) {
				current_symtab = pop_scope (current_symtab);
			}
		}
	;

seq_semi
	: ';'
	;

compound_statement
	: '{' '}'										{ $$ = new_block_expr (0); }
	| '{' new_scope statement_list '}' end_scope	{ $$ = $3; }
	;

compound_statement_ns
	: '{' '}'										{ $$ = new_block_expr (0); }
	| '{' new_block statement_list '}'				{ $$ = $3; }
	;

statement_list
	: statement
		{
			auto list = $<mut_expr>0;
			$$ = append_expr (list, $1);
		}
	| statement_list statement
		{
			auto list = $1;
			$$ = append_expr (list, $2);
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
	| RETURN opt_expr ';'		{ $$ = new_return_expr ($2); }
	| RETURN compound_init ';'	{ $$ = new_return_expr ($2); }
	| AT_RETURN	expr ';'
		{
			auto at = new_return_expr ($2);
			at->retrn.at_return = true;
			$$ = at;
		}
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
			$$ = new_caselabel_expr ($2, nullptr);
		}
	| DEFAULT ':'
		{
			$$ = new_caselabel_expr (nullptr, nullptr);
		}
	| SWITCH break_label line '(' comma_expr[test] ')' compound_statement[body]
		{
			scoped_src_loc ($line);
			$$ = new_switch_expr ($test, $body, break_label);
			break_label = $break_label;
		}
	| GOTO NAME
		{
			const expr_t *label = named_label_expr ($2);
			$$ = goto_expr (label);
		}
	| IF not line '(' expr[test] ')' statement[true] %prec IFX
		{
			scoped_src_loc ($line);
			$$ = new_select_expr ($not, $test, $true, nullptr, nullptr);
		}
	| IF not line '(' expr[test]')' statement[true]
			else statement[false]
		{
			scoped_src_loc ($line);
			$$ = new_select_expr ($not, $test, $true, $else, $false);
		}
	| FOR new_scope break_label continue_label
			'(' opt_init_semi[init] opt_expr[test] ';' opt_expr[cont] ')'
			statement[body] end_scope
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
	| WHILE break_label continue_label not '(' expr[test] ')'
			statement[body]
		{
			$$ = new_loop_expr ($not, false, $test, $body,
								continue_label, nullptr, break_label);
			break_label = $break_label;
			continue_label = $continue_label;
		}
	| DO break_label continue_label statement[body]
			WHILE not '(' expr[test] ')' ';'
		{
			$$ = new_loop_expr ($not, true, $test, $body,
								continue_label, nullptr, break_label);
			break_label = $break_label;
			continue_label = $continue_label;
		}
	| ALGEBRA '(' TYPE_SPEC '(' expr_list ')' ')'
		{
			auto algebra = algebra_type ($3.type, $5);
			current_symtab = algebra_scope (algebra, current_symtab);
		}
	  compound_statement
		{
			current_symtab = pop_scope (current_symtab);
			$$ = $9;
		}
	| ALGEBRA '(' TYPE_SPEC ')'
		{
			auto algebra = algebra_type ($3.type, 0);
			current_symtab = algebra_scope (algebra, current_symtab);
		}
	  compound_statement
		{
			current_symtab = pop_scope (current_symtab);
			$$ = $6;
		}
	| ALGEBRA '(' TYPE_NAME ')'
		{
			auto algebra = $3.type;
			current_symtab = algebra_scope (algebra, current_symtab);
		}
	  compound_statement
		{
			current_symtab = pop_scope (current_symtab);
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

line
	: /* empty */
		{
			// this is only to get the the file and line number info
			$$ = new_nil_expr ();
		}

else
	: ELSE line				{ $$ = $line; }
	;

label
	: NAME ':'
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

opt_init_semi
	: comma_expr ';'
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
	| unary_expr '(' opt_arg_list ')' { $$ = new_call_expr ($1, $3, nullptr); }
	| unary_expr '[' expr ']'		{ $$ = new_array_expr ($1, $3); }
	| unary_expr '.' ident_expr		{ $$ = new_field_expr ($1, $3); }
	| unary_expr '.' unary_expr		{ $$ = new_field_expr ($1, $3); }
	| INCOP unary_expr				{ $$ = new_incop_expr ($1, $2, false); }
	| unary_expr INCOP				{ $$ = new_incop_expr ($2, $1, true); }
	| unary_expr REVERSE			{ $$ = new_unary_expr (QC_REVERSE, $1); }
	| DUAL cast_expr %prec UNARY	{ $$ = new_unary_expr (QC_DUAL, $2); }
	| UNDUAL cast_expr %prec UNARY	{ $$ = new_unary_expr (QC_UNDUAL, $2); }
	| '+' cast_expr %prec UNARY	{ $$ = $2; }
	| '-' cast_expr %prec UNARY	{ $$ = new_unary_expr ('-', $2); }
	| '!' cast_expr %prec UNARY	{ $$ = new_unary_expr ('!', $2); }
	| '~' cast_expr %prec UNARY	{ $$ = new_unary_expr ('~', $2); }
	| '&' cast_expr %prec UNARY	{ $$ = new_unary_expr ('&', $2); }
	| '*' cast_expr %prec UNARY	{ $$ = new_unary_expr ('.', $2); }
	| '=' cast_expr %prec UNARY { $$ = new_unary_expr ('=', $2); }
	| SIZEOF unary_expr	%prec UNARY	{ $$ = new_unary_expr ('S', $2); }
	| SIZEOF '(' typename ')'	%prec HYPERUNARY
		{
			auto spec = $3;
			auto type_expr = spec.type_expr;
			if (!type_expr) {
				type_expr = new_type_expr (spec.type);
			}
			$$ = new_unary_expr ('S', type_expr);
		}
	| type_op '(' type_param_list ')'	{ $$ = type_function ($1, $3); }
	| vector_expr				{ $$ = new_vector_list_expr ($1); }
	| obj_expr					{ $$ = $1; }
	;

ident_expr
	: OBJECT_NAME				{ $$ = new_symbol_expr ($1.sym); }
	| CLASS_NAME				{ $$ = new_symbol_expr ($1); }
	| TYPE_NAME					{ $$ = new_symbol_expr ($1.sym); }
	;

vector_expr
	: '[' expr ',' expr_list ']'
		{
			$$ = expr_prepend_expr ($4, $2);
		}
	;

cast_expr
	: '(' typename ')' cast_expr
		{
			auto spec = $2;
			auto decl = new_decl_expr (spec, nullptr);
			$$ = new_binary_expr ('C', decl, $4);
		}
	| CONSTRUCT '(' typename ',' expr_list[args] ')' //FIXME arg_expr instead?
		{
			auto spec = $3;
			auto args = $args;
			auto type_expr = spec.type_expr;
			if (!type_expr) {
				type_expr = new_type_expr (spec.type);
			}
			$$ = new_call_expr (type_expr, args, nullptr);
		}
	| unary_expr %prec LOW
	;

expr
	: cast_expr
	| expr '=' expr				{ $$ = new_assign_expr ($1, $3); }
	| expr '=' compound_init	{ $$ = new_assign_expr ($1, $3); }
	| expr ASX expr
		{
			scoped_src_loc ($1);
			auto expr = $3;
			expr = paren_expr (expr);
			expr = new_binary_expr ($2, $1, expr);
			$$ = new_assign_expr ($1, expr);
		}
	| expr '?' expr ':' expr 	{ $$ = new_cond_expr ($1, $3, $5); }
	| expr AND expr				{ $$ = new_binary_expr (QC_AND, $1, $3); }
	| expr OR expr				{ $$ = new_binary_expr (QC_OR,  $1, $3); }
	| expr EQ expr				{ $$ = new_binary_expr (QC_EQ,  $1, $3); }
	| expr NE expr				{ $$ = new_binary_expr (QC_NE,  $1, $3); }
	| expr LE expr				{ $$ = new_binary_expr (QC_LE,  $1, $3); }
	| expr GE expr				{ $$ = new_binary_expr (QC_GE,  $1, $3); }
	| expr LT expr				{ $$ = new_binary_expr (QC_LT,  $1, $3); }
	| expr GT expr				{ $$ = new_binary_expr (QC_GT,  $1, $3); }
	| expr SHL expr				{ $$ = new_binary_expr (QC_SHL, $1, $3); }
	| expr SHR expr				{ $$ = new_binary_expr (QC_SHR, $1, $3); }
	| expr '+' expr				{ $$ = new_binary_expr ('+', $1, $3); }
	| expr '-' expr				{ $$ = new_binary_expr ('-', $1, $3); }
	| expr '*' expr				{ $$ = new_binary_expr ('*', $1, $3); }
	| expr '/' expr				{ $$ = new_binary_expr ('/', $1, $3); }
	| expr '&' expr				{ $$ = new_binary_expr ('&', $1, $3); }
	| expr '|' expr				{ $$ = new_binary_expr ('|', $1, $3); }
	| expr '^' expr				{ $$ = new_binary_expr ('^', $1, $3); }
	| expr '%' expr				{ $$ = new_binary_expr ('%', $1, $3); }
	| expr MOD expr				{ $$ = new_binary_expr (QC_MOD, $1, $3); }
	| expr GEOMETRIC expr		{ $$ = new_binary_expr (QC_GEOMETRIC, $1, $3); }
	| expr HADAMARD expr		{ $$ = new_binary_expr (QC_HADAMARD, $1, $3); }
	| expr CROSS expr			{ $$ = new_binary_expr (QC_CROSS, $1, $3); }
	| expr DOT expr				{ $$ = new_binary_expr (QC_DOT, $1, $3); }
	| expr OUTER expr			{ $$ = new_binary_expr (QC_OUTER, $1, $3); }
	| expr WEDGE expr			{ $$ = new_binary_expr (QC_WEDGE, $1, $3); }
	| expr REGRESSIVE expr		{ $$ = new_binary_expr (QC_REGRESSIVE, $1, $3); }
	;

comma_expr
	: expr_list
		{
			if ($1->list.head->next) {
				auto b = build_block_expr ($1, true);
				b->block.scope = current_symtab;
				$$ = b;
			} else {
				$$ = (expr_t *) $1->list.head->expr;
			}
		}
	;

expr_list
	: expr						{ $$ = new_list_expr ($1); }
	| expr_list ',' expr		{ $$ = expr_append_expr ($1, $3); }
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
	| TRUE						{ $$ = new_bool_expr (true); }
	| FALSE						{ $$ = new_bool_expr (false); }
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
	| NOT { $$ = new_symbol ("not"); }
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
			for (auto li = $2->block.list.head; li; li = li->next) {
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
				$$ = $1->type->class;
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
			current_symtab = pop_scope (tab);
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
				$1.sym->sy_type = sy_offset;
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
			declare_field ($1, current_symtab, ctx);
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
			$<symbol>$ = method_symbol (current_class, method, ctx);
		}
	  save_storage
		{
			method_t   *method = $2;
			const char *nicename = method_name (method);
			symbol_t   *sym = $<symbol>4;
			symtab_t   *ivar_scope;

			auto spec = (specifier_t) {
				.sym = sym,
				.storage = sc_static,
				.is_far = true,
			};

			ivar_scope = class_ivar_scope (current_class, current_symtab);
			$<funcstate>$ = (funcstate_t) {
				.symtab = ivar_scope,
				.function = current_func,
			};
			current_func = begin_function (spec, nicename, ivar_scope, ctx);
			class_finish_ivar_scope (current_class, ivar_scope,
									 current_func->locals);
			method->func = sym->metafunc->func;
			method->def = method->func->def;
			current_symtab = current_func->locals;
			current_storage = sc_local;
		}
	  compound_statement_ns
		{
			auto fs = $<funcstate>6;
			auto fsym = $<symbol>4;
			fs.spec.sym = fsym;
			build_code_function (fs.spec, $3, $7, ctx);
			current_symtab = pop_scope (fs.symtab);
			current_func = fs.function;
			restore_storage ($5);
		}
	| ci methoddecl '=' '#' const ';'
		{
			method_t   *method = $2;
			const expr_t *bi_val = expr_process ($5, ctx);

			method->instance = $1;
			method = class_find_method (current_class, method);

			auto spec = (specifier_t) {
				.sym = method_symbol (current_class, method, ctx),
				.storage = sc_static,
				.is_far = true,
			};
			build_builtin_function (spec, nullptr, bi_val);
			method->func = spec.sym->metafunc->func;
			method->def = method->func->def;
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
	: ci methoddecl ';'
		{
			$2->instance = $1;
			$$ = $2;
		}
	| ci error ';'
		{ $$ = new_method (&type_id, make_selector ("", 0, 0), 0); }
	;

methoddecl
	: '(' typename ')' unaryselector
		{
			auto spec = resolve_type_spec ($2, ctx);
			$$ = new_method (spec.type, $4, 0);
		}
	| unaryselector
		{ $$ = new_method (&type_id, $1, 0); }
	| '(' typename ')' keywordselector optional_param_list
		{
			auto spec = resolve_type_spec ($2, ctx);
			$$ = new_method (spec.type, $4, $5);
		}
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
		{
			auto spec = resolve_type_spec ($4, ctx);
			$$ = make_selector ($1->name, spec.type, $6->name);
		}
	| selector ':' identifier
		{
			$$ = make_selector ($1->name, &type_id, $3->name);
		}
	| ':' '(' typename ')' identifier
		{
			auto spec = resolve_type_spec ($3, ctx);
			$$ = make_selector ("", spec.type, $5->name);
		}
	| ':' identifier
		{
			$$ = make_selector ("", &type_id, $2->name);
		}
	;

obj_expr
	: obj_messageexpr
	| SELECTOR '(' selectorarg ')'	{ $$ = selector_expr ($3); }
	| PROTOCOL '(' identifier ')'	{ $$ = protocol_expr ($3->name); }
	| ENCODE '(' typename ')'
		{
			auto spec = resolve_type_spec ($3, ctx);
			$$ = encode_expr (spec.type);
		}
	| obj_string /* FIXME string object? */
	;

obj_messageexpr
	: '[' receiver messageargs ']'
		{
			scoped_src_loc ($receiver);
			$$ = new_message_expr ($receiver, $messageargs);
		}
	| '[' TYPE_NAME[spec] attrfunc ']'
		{
			$$ = type_property ($spec, $attrfunc);
		}
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

static void __attribute__((used))
qc_dump_stack(yypstate *yyps)
{
	yy_stack_print (yyps->yyss, yyps->yyssp);
}

// preprocessor directives in ruamoko and quakec
static directive_t rua_directives[] = {
	{"include",  PRE_INCLUDE},
	{"embed",    PRE_EMBED},
	{"define",   PRE_DEFINE},
	{"undef",    PRE_UNDEF},
	{"error",    PRE_ERROR},
	{"warning",  PRE_WARNING},
	{"notice",   PRE_NOTICE},
	{"pragma",   PRE_PRAGMA},
	{"line",     PRE_LINE},
};

static directive_t *
qc_directive (const char *token)
{
	static hashtab_t *directive_tab;

	if (!directive_tab) {
		directive_tab = Hash_NewTable (253, rua_directive_get_key, 0, 0, 0);
		for (size_t i = 0; i < ARRCOUNT(rua_directives); i++) {
			Hash_Add (directive_tab, &rua_directives[i]);
		}
	}
	directive_t *directive = Hash_Find (directive_tab, token);
	return directive;
}

// These keywords are part of the Ruamoko language and require the QuakeForge
// Ruamoko VM.
static keyword_t rua_keywords[] = {
#define VEC_TYPE(type_name, base_type) \
	{ #type_name, QC_TYPE_SPEC, .spec = { .type = &type_##type_name } },
#include "tools/qfcc/include/vec_types.h"

	{"bvec2", QC_TYPE_SPEC, .spec = {.type = &type_bvec2}},
	{"bvec3", QC_TYPE_SPEC, .spec = {.type = &type_bvec3}},
	{"bvec4", QC_TYPE_SPEC, .spec = {.type = &type_bvec4}},

#define MAT_TYPE(type_name, base_type, cols, align_as) \
	{ #type_name, QC_TYPE_SPEC, .spec = { . type = &type_##type_name } },
#include "tools/qfcc/include/mat_types.h"
	{ "mat2", QC_TYPE_SPEC, .spec = { . type = &type_mat2x2 } },
	{ "mat3", QC_TYPE_SPEC, .spec = { . type = &type_mat3x3 } },
	{ "mat4", QC_TYPE_SPEC, .spec = { . type = &type_mat4x4 } },
};

// These keywords are all part of the Ruamoko (Objective-QC) language.
// The first time any one of them is encountered, the class system will be
// initialized.
// If not compiling for the QuakeForge VM, or if Ruamoko has been disabled,
// then they will be unavailable as keywords.
static keyword_t obj_keywords[] = {
	{"id",				QC_OBJECT_NAME, .spec = { .type = &type_id } 		},
	{"Class",			QC_TYPE_SPEC, .spec = { .type = &type_Class } 		},
	{"Method",			QC_TYPE_SPEC, .spec = { .type = &type_method } 	},
	{"Super",			QC_TYPE_SPEC, .spec = { .type = &type_super } 		},
	{"SEL",				QC_TYPE_SPEC, .spec = { .type = &type_SEL } 		},
	{"IMP",				QC_TYPE_SPEC, .spec = { .type = &type_IMP } 		},

	{"@class",			QC_CLASS					},
	{"@defs",			QC_DEFS					},
	{"@encode",			QC_ENCODE					},
	{"@end",			QC_END						},
	{"@implementation",	QC_IMPLEMENTATION			},
	{"@interface",		QC_INTERFACE				},
	{"@private",		QC_PRIVATE					},
	{"@protected",		QC_PROTECTED				},
	{"@protocol",		QC_PROTOCOL				},
	{"@public",			QC_PUBLIC					},
	{"@reference",		QC_REFERENCE				},
	{"@selector",		QC_SELECTOR				},
	{"@self",			QC_SELF					},
	{"@this",			QC_THIS					},

	// This is a hack to trigger the initialization of the class
	// sytem if it is seen before any other Objective-QC symbol. Otherwise,
	// it is just an identifier, though it does reference a built-in type
	// created by the class system.
	{"obj_module",		0						},
};

// These keywords are extensions to QC and thus available only in advanced
// or extended code. However, if they are preceeded by an @ (eg, @for), then
// they are always available. This is to prevent them from causing trouble
// for traditional code that might use these words as identifiers, but still
// make the language features available to traditional code.
static keyword_t at_keywords[] = {
	{"for",			QC_FOR		},
	{"goto",		QC_GOTO	},
	{"break",		QC_BREAK	},
	{"continue",	QC_CONTINUE},
	{"switch",		QC_SWITCH	},
	{"case",		QC_CASE	},
	{"default",		QC_DEFAULT	},
	{"nil",			QC_NIL		},
	{"struct",		QC_STRUCT	},
	{"union",		QC_STRUCT	},
	{"enum",		QC_ENUM	},
	{"typedef",		QC_TYPEDEF	},
	{"extern",		QC_EXTERN	},
	{"static",		QC_STATIC	},
	{"sizeof",		QC_SIZEOF	},
	{"not",			QC_NOT		},
	{"auto",		QC_TYPE_SPEC, .spec = { .type = &type_auto } },
	{"const",		QC_TYPE_QUAL, .spec = { .is_const = true } },
};

// These keywords require the QuakeForge VM to be of any use. ie, they cannot
// be supported (sanely) by v6 progs.
static keyword_t qf_keywords[] = {
	{"quaternion",	QC_TYPE_SPEC, .spec = { .type = &type_quaternion } },
	{"double",		QC_TYPE_SPEC, .spec = { .type = &type_double } },
	{"int",			QC_TYPE_SPEC, .spec = { .type = &type_int } 	},
	{"bool",		QC_TYPE_SPEC, .spec = { .type = &type_bool } 	},
	{"lbool",		QC_TYPE_SPEC, .spec = { .type = &type_lbool } 	},
	{"unsigned",	QC_TYPE_SPEC, .spec = { .is_unsigned = true } },
	{"signed",		QC_TYPE_SPEC, .spec = { .is_signed = true } },
	{"long",		QC_TYPE_SPEC, .spec = { .is_long = true } },
	{"short",		QC_TYPE_SPEC, .spec = { .is_short = true } },

	{"true",        QC_TRUE },
	{"false",       QC_FALSE},

	{"@args",		QC_ARGS,				},
	{"@va_list",	QC_TYPE_SPEC, .spec = { .type = &type_va_list } 	},
	{"@param",		QC_TYPE_SPEC, .spec = { .type = &type_param } 		},
	{"@return",     QC_AT_RETURN,		},
	{"@in",			QC_TYPE_QUAL, .spec = { .storage = sc_in } },
	{"@out",		QC_TYPE_QUAL, .spec = { .storage = sc_out } },
	{"@inout",		QC_TYPE_QUAL, .spec = { .storage = sc_inout } },

	{"@hadamard",	QC_HADAMARD,	},
	{"@cross",		QC_CROSS,		},
	{"@dot",		QC_DOT,			},
	{"@outer",		QC_OUTER,		},
	{"@wedge",		QC_WEDGE,		},
	{"@regressive",	QC_REGRESSIVE,	},
	{"@geometric",	QC_GEOMETRIC,	},
	{"@algebra",	QC_ALGEBRA,		},
	{"@dual",		QC_DUAL,		},
	{"@undual",		QC_UNDUAL,		},

	{"@image",		QC_IMAGE,		},

	{"@construct",	QC_CONSTRUCT,	},
	{"@generic",	QC_GENERIC,		},
	{"@function",	QC_AT_FUNCTION,	},
	{"@field",		QC_AT_FIELD,	},
	{"@pointer",	QC_AT_POINTER,	},
	{"@array",		QC_AT_ARRAY,	},
	{"@base",		QC_AT_BASE,		},
	{"@width",		QC_AT_WIDTH,	},
	{"@vector",		QC_AT_VECTOR,	},
	{"@rows",		QC_AT_ROWS,		},
	{"@cols",		QC_AT_COLS,		},
	{"@matrix",		QC_AT_MATRIX,	},
	{"@int",		QC_AT_INT,		},
	{"@uint",		QC_AT_UINT,		},
	{"@bool",		QC_AT_BOOL,		},
	{"@float",		QC_AT_FLOAT,	},
};

// These keywors are always available. Other than the @ keywords, they
// form traditional QuakeC.
static keyword_t keywords[] = {
	{"void",		QC_TYPE_SPEC, .spec = { .type = &type_void } 	},
	{"float",		QC_TYPE_SPEC, .spec = { .type = &type_float } 	},
	{"string",		QC_TYPE_SPEC, .spec = { .type = &type_string } },
	{"vector",		QC_TYPE_SPEC, .spec = { .type = &type_vector } },
	{"entity",		QC_TYPE_SPEC, .spec = { .type = &type_entity } },
	{"local",		QC_LOCAL,					},
	{"return",		QC_RETURN,					},
	{"while",		QC_WHILE,					},
	{"do",			QC_DO,						},
	{"if",			QC_IF,						},
	{"else",		QC_ELSE,					},
	{"@system",		QC_SYSTEM,					},
	{"@overload",	QC_OVERLOAD,				},
	{"@attribute",  QC_ATTRIBUTE,				},
	{"@handle",     QC_HANDLE,					},
	{"@intrinsic",  QC_INTRINSIC,				},
};

static int
qc_process_keyword (QC_YYSTYPE *lval, keyword_t *keyword, const char *token,
					rua_ctx_t *ctx)
{
	if (keyword->value == QC_STRUCT) {
		lval->op = token[0];
	} else if (keyword->value == QC_OBJECT_NAME) {
		symbol_t   *sym;

		sym = symtab_lookup (current_symtab, token);
		lval->symbol = sym;
		// the global id symbol is always just a name so attempts to redefine
		// it globally can be caught and treated as an error, but it needs to
		// be redefinable when in an enclosing scope.
		if (sym->sy_type == sy_name) {
			// this is the global id (object)
			lval->spec = (specifier_t) {
				.type = sym->type,
				.sym = sym,
			};
			return QC_OBJECT_NAME;
		} else if (sym->sy_type == sy_type_param) {
			// id has been redeclared via a generic type param
			lval->spec = (specifier_t) {
				.type_expr = new_symbol_expr (sym),
				.sym = sym,
			};
			return QC_TYPE_NAME;
		} else if (sym->sy_type == sy_type) {
			// id has been redeclared via a typedef
			lval->spec = (specifier_t) {
				.type = sym->type,
				.sym = sym,
			};
			return QC_TYPE_NAME;
		}
		// id has been redeclared as a variable (hopefully)
		return QC_NAME;
	} else {
		lval->spec = keyword->spec;
	}
	return keyword->value;
}

static int
qc_keyword_or_id (QC_YYSTYPE *lval, const char *token, rua_ctx_t *ctx)
{
	static hashtab_t *keyword_tab;
	static hashtab_t *qf_keyword_tab;
	static hashtab_t *at_keyword_tab;
	static hashtab_t *obj_keyword_tab;
	static hashtab_t *rua_keyword_tab;

	keyword_t  *keyword = 0;

	if (!keyword_tab) {
		size_t      i;

		keyword_tab = Hash_NewTable (253, rua_keyword_get_key, 0, 0, 0);
		qf_keyword_tab = Hash_NewTable (253, rua_keyword_get_key, 0, 0, 0);
		at_keyword_tab = Hash_NewTable (253, rua_keyword_get_key, 0, 0, 0);
		obj_keyword_tab = Hash_NewTable (253, rua_keyword_get_key, 0, 0, 0);
		rua_keyword_tab = Hash_NewTable (253, rua_keyword_get_key, 0, 0, 0);

		for (i = 0; i < ARRCOUNT(keywords); i++)
			Hash_Add (keyword_tab, &keywords[i]);
		for (i = 0; i < ARRCOUNT(qf_keywords); i++)
			Hash_Add (qf_keyword_tab, &qf_keywords[i]);
		for (i = 0; i < ARRCOUNT(at_keywords); i++)
			Hash_Add (at_keyword_tab, &at_keywords[i]);
		for (i = 0; i < ARRCOUNT(obj_keywords); i++)
			Hash_Add (obj_keyword_tab, &obj_keywords[i]);
		for (i = 0; i < ARRCOUNT(rua_keywords); i++)
			Hash_Add (rua_keyword_tab, &rua_keywords[i]);
	}
	if (options.traditional < 1) {
		if (options.code.progsversion == PROG_VERSION) {
			keyword = Hash_Find (rua_keyword_tab, token);
		}
		if (!keyword) {
			keyword = Hash_Find (obj_keyword_tab, token);
			if (keyword) {
				if (!obj_initialized)
					class_init ();
			}
		}
		if (!keyword)
			keyword = Hash_Find (qf_keyword_tab, token);
	}
	if (!keyword && options.traditional < 2)
		keyword = Hash_Find (at_keyword_tab, token);
	if (!keyword && token[0] == '@') {
		keyword = Hash_Find (at_keyword_tab, token + 1);
		if (keyword)
			token += 1;
	}
	if (!keyword)
		keyword = Hash_Find (keyword_tab, token);
	if (keyword && keyword->value)
		return qc_process_keyword (lval, keyword, token, ctx);
	if (token[0] == '@') {
		return '@';
	}

	symbol_t   *sym = nullptr;
	if (!sym) {
		sym = symtab_lookup (current_symtab, token);
	}
	if (!sym) {
		sym = new_symbol (token);
	}
	lval->symbol = sym;
	if (sym->sy_type == sy_type) {
		lval->spec = (specifier_t) {
			.type = sym->type,
			.sym = sym,
		};
		return QC_TYPE_NAME;
	} else if (sym->sy_type == sy_type_param) {
		lval->spec = (specifier_t) {
			.type_expr = new_symbol_expr (sym),
			.sym = sym,
		};
		return QC_TYPE_NAME;
	}
	if (sym->sy_type == sy_class)
		return QC_CLASS_NAME;
	return QC_NAME;
}

static int
qc_yyparse (FILE *in, rua_ctx_t *ctx)
{
	rua_parser_t parser = {
		.parse = qc_yypush_parse,
		.state = qc_yypstate_new (),
		.directive = qc_directive,
		.keyword_or_id = qc_keyword_or_id,
	};
	int ret = rua_parse (in, &parser, ctx);
	qc_yypstate_delete (parser.state);
	return ret;
}

int
qc_parse_string (const char *str, rua_ctx_t *ctx)
{
	rua_parser_t parser = {
		.parse = qc_yypush_parse,
		.state = qc_yypstate_new (),
		.directive = qc_directive,
		.keyword_or_id = qc_keyword_or_id,
	};
	auto qc_ctx = *ctx;
	int ret = rua_parse_string (str, &parser, &qc_ctx);
	glsl_yypstate_delete (parser.state);
	return ret;
}

static int
qc_finish (const char *file, rua_ctx_t *ctx)
{
	if (options.frames_files) {
		write_frame_macros (va (0, "%s.frame", file_basename (file, 0)));
	}
	class_finish_module (ctx);
	return pr.error_count;
}

static void
rua_init (rua_ctx_t *ctx)
{
	ctx->language->initialized = true;
	if (options.code.spirv && !pr.module) {
		//FIXME unhardcode
		spirv_add_extension (pr.module, "SPV_KHR_multiview");
		spirv_add_extinst_import (pr.module, "GLSL.std.450");
		//FIXME sufficient? phys 32/storage?
		spirv_set_addressing_model (pr.module, SpvAddressingModelLogical);
		//FIXME look into Vulkan, or even configurable
		spirv_set_memory_model (pr.module, SpvMemoryModelGLSL450);
	}
}

language_t lang_ruamoko = {
	.short_circuit = true,
	.init = rua_init,
	.parse = qc_yyparse,
	.finish = qc_finish,
	.parse_declaration = rua_parse_declaration,
};
