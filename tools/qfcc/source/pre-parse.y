/*
	pre-parse.y

	parser for the preprocessor

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2023/10/12

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
%define api.prefix {pre_yy}
%define api.pure full
%define api.push-pull push
%define api.token.prefix {PRE_}
%locations
%parse-param {void *scanner}

%{
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#include "QF/dstring.h"

#define PRE_YYDEBUG 1
#define PRE_YYERROR_VERBOSE 1
#undef PRE_YYERROR_VERBOSE

#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/pragma.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/type.h"

#include "tools/qfcc/source/pre-parse.h"

#define pre_yytext qc_yyget_text (scanner)
char *qc_yyget_text (void *scanner);

static void
yyerror (YYLTYPE *yylloc, void *scanner, const char *s)
{
	const char *text = quote_string (pre_yytext);
#ifdef YYERROR_VERBOSE
	error (0, "%d:%s before '%s'\n", yylloc->column, s, text);
#else
	error (0, "%s before '%s'", s, text);
#endif
}
#if 0
static void
parse_error (void *scanner)
{
	const char *text = pre_yytext;
	error (0, "parse error before %s", text);
}

#define PARSE_ERROR do { parse_error (scanner); YYERROR; } while (0)
#endif

#define first_line line
#define first_column column
%}

%code requires {
#include "tools/qfcc/include/rua-lang.h"
}
%define api.value.type {rua_val_t}
%define api.location.type {rua_loc_t}

%left LOW
%nonassoc IFX
%nonassoc ELSE
%nonassoc BREAK_PRIMARY
%nonassoc ';'
%nonassoc CLASS_NOT_CATEGORY
%nonassoc STORAGEX

%left	        COMMA
%right          <op> '=' ASX
%right          '?' ':'
%left           OR
%left           AND
%left           '|'
%left           '^'
%left           '&'
%left           EQ NE
%left	        LT GT GE LE
%token	        NAND NOR XNOR
// end of tokens common between qc and qp

%left           SHL SHR
%left           '+' '-'
%left           '*' '/' '%' MOD SCALE GEOMETRIC
%left           HADAMARD CROSS DOT WEDGE REGRESSIVE
%right          SIZEOF UNARY INCOP REVERSE STAR DUAL
%left           HYPERUNARY
%left           '.' '(' '['

%token <expr>   VALUE STRING
%token <t>      TOKEN
// end of tokens comment between qc and preprocessor

%token <t.text> QSTRING HSTRING

%token          INCLUDE EMBED
%token          DEFINE UNDEF
%token <t.text> TEXT ID IDp
%token          ERROR WARNING NOTICE
%token          PRAGMA LINE
%token          IF IFDEF IFNDEF ELSE ELIF ELIFDEF ELIFNDEF ENDIF
%token          DEFINED EOD
%token          CONCAT ARGS

%type <t.text>  string
%type <macro>   params opt_params body arg arg_list arg_clist
%type <dstr>    text text_text
%type <expr>    unary_expr expr id defined defined_id line_expr
%type <t>		body_token

%{
#define TEXPR(c,t,f) new_long_expr (expr_long (c) ? expr_long (t) \
												  : expr_long (f), false)
#if 0
#define BEXPR(a,op,b) \
	({\
		printf ("\n%ld %s %ld\n", expr_long(a), #op, expr_long(b));\
		new_long_expr (expr_long (a) op expr_long (b), false);\
	})
#define UEXPR(op,a)\
	({\
		printf ("\n%s %ld\n", #op, expr_long(a));\
		new_long_expr (op expr_long (a), false);\
	})
#else
#define BEXPR(a,op,b) new_long_expr (expr_long (a) op expr_long (b), false)
#define UEXPR(op,a)   new_long_expr (op expr_long (a), false)
#endif

static const expr_t *
get_long (const expr_t *expr, const char *text, int defl)
{
	auto type = expr ? get_type (expr) : 0;
	if (!type || !is_long (type)) {
		if (type && is_double (type)) {
			error (0, "floating constant in preprocessor expression");
			expr = new_long_expr (expr_double (expr), false);
		} else {
			error (0, "token \"%s\" is not valid in preprocessor"
				   " expressions", text);
			expr= new_long_expr (defl, false);
		}
	}
	return expr;
}

%}

%%

start
	: directive_list
	| ARGS
		<macro>{
			$$ = rua_macro_arg (&$<t>1, scanner);
		}
	  args ')' { YYACCEPT; }
	;

directive_list
	: /*empty*/
	| directive_list directive
	;

eod : EOD { rua_end_directive (scanner); } ;

directive
	: INCLUDE incexp string extra_warn { rua_include_file ($3, scanner); }
	| EMBED incexp string extra_ignore { rua_embed_file ($3, scanner); }
	| DEFINE ID		<macro> { $$ = rua_start_macro ($2, false, scanner); }
	  body					{ rua_macro_finish ($body, scanner); }
	  eod
	| DEFINE IDp	<macro> { $$ = rua_start_macro ($2, true, scanner); }
	  opt_params ')' <macro>{ $$ = rua_end_params ($3, scanner); }
	  body					{ rua_macro_finish ($body, scanner); }
	  eod
	| UNDEF ID				{ rua_undefine ($2, scanner); }
	  extra_warn
	| ERROR text { error (0, "%s", $text->str); dstring_delete ($text); }
	  eod
	| WARNING text { warning (0, "%s", $text->str); dstring_delete ($text); }
	  eod
	| NOTICE text { notice (0, "%s", $text->str); dstring_delete ($text); }
	  eod
	| PRAGMA expand { rua_start_pragma (scanner); }
	  pragma_params { pragma_process (); }
	  eod
	| LINE expand expr extra_warn
		{ rua_line_info ($3, 0, 0, scanner); }
	| LINE expand expr string line_expr extra_warn
		{ rua_line_info ($3, $4, $5, scanner); }
	| IF					{ rua_start_if (true, scanner); }
	  expr					{ rua_if (expr_long ($3), scanner); }
	  eod
	| IFDEF					{ rua_start_if (false, scanner); }
	  ID					{ rua_if (rua_defined ($3, scanner), scanner); }
	  extra_warn
	| IFNDEF				{ rua_start_if (false, scanner); }
	  ID					{ rua_if (!rua_defined ($3, scanner), scanner); }
	  extra_warn
	| ELSE					{ rua_else (true, "else", scanner); }
	  extra_warn
	| ELIF					{ rua_start_else (true, scanner); }
	  expr					{ rua_else (expr_long ($3), "elif", scanner); }
	  eod
	| ELIFDEF				{ rua_start_else (false, scanner); }
	  ID		{ rua_else (rua_defined ($3, scanner), "elifdef", scanner); }
	  extra_warn
	| ELIFNDEF				{ rua_start_else (false, scanner); }
	  ID		{ rua_else (!rua_defined ($3, scanner), "elifndef", scanner); }
	  extra_warn
	| ENDIF					{ rua_endif (scanner); }
	  extra_warn
	;

extra_warn
	: {} eod
	;

extra_ignore
	: {} eod
	;

text: { rua_start_text (scanner); $<dstr>$ = dstring_new (); } text_text
	  { $text = $text_text; }
	;
text_text
	: /* empty */		{ dstring_clearstr ($<dstr>0); $$ = $<dstr>0; }
	| text_text TEXT	{ dstring_appendstr ($1, $2); $$ = $1; }
	;

body: /* empty */		{ $$ = $<macro>0; }
	| body body_token	{ $$ = rua_macro_append ($1, &$2, scanner); }
	;

body_token
	: TOKEN
	| ','				{ $$ = $<t>1; }
	| '('				{ $$ = $<t>1; }
	| ')'				{ $$ = $<t>1; }
	;

incexp
	: { rua_start_include (scanner); }
	;

expand
	: { rua_start_expr (scanner); }
	;

pragma_params
	: ID						{ pragma_add_arg ($1); }
	| pragma_params ID			{ pragma_add_arg ($2); }
	;

string
	: HSTRING			{ $$ = save_string ($1); }
	| QSTRING			{ $$ = save_string ($1); }
	;

opt_params
	: /*empty*/			{ $$ = $<macro>0; }
	| params
	;

params
	: TOKEN				{ $$ = rua_macro_param ($<macro>0, &$1, scanner); }
	| params ',' TOKEN	{ $$ = rua_macro_param ($1, &$3, scanner); }
	;

args: arg_list
	| args ','
		<macro>{
			$$ = rua_macro_arg (&$<t>2, scanner);
		}
	  arg_list
	;

arg_list
	: /* emtpy */	{ $$ = $<macro>0; }
	| arg_list arg	{ $$ = $2; }
	;

arg : '('	<macro>	{ $$ = rua_macro_append ($<macro>0, &$<t>1, scanner); }
	  arg_clist	<macro>	{ $$ = $<macro>2; }
	  ')'			{ $$ = rua_macro_append ($<macro>4, &$<t>5, scanner); }
	| TOKEN			{ $$ = rua_macro_append ($<macro>0, &$1, scanner); }
	| VALUE			{ $$ = rua_macro_append ($<macro>0, &$<t>1, scanner); }
	| ID			{ $$ = rua_macro_append ($<macro>0, &$<t>1, scanner); }
	;

arg_clist
	: /* emtpy */	{ $$ = $<macro>0; }
	| arg_clist arg	{ $$ = $2; }
	| arg_clist ','	{ $$ = rua_macro_append ($1, &$<t>2, scanner); }
	;

id  : ID					{ $$ = new_long_expr (0, false); }
	| ID '(' args ')'		{ $$ = new_long_expr (0, false); }
	;

defined
	: defined_id			{ $$ = $1; }
	| '(' defined_id ')'	{ $$ = $2; }
	;

//the token's text is quite ephemeral when short (the usual case)
defined_id
	: ID { $$ = new_long_expr (rua_defined ($1, scanner), false); }
	;

unary_expr
	: id
	| VALUE				{ $$ = get_long ($1, $<t.text>1, 1); }
	| QSTRING			{ $$ = get_long (0, $<t.text>1, 1); }
	| '(' expr ')'		{ $$ = $2; }
	| DEFINED			{ rua_expand_off (scanner); }
	  defined			{ rua_expand_on (scanner); $$ = $3; }
	| '+' unary_expr	{ $$ = $2; }
	| '-' unary_expr	{ $$ = UEXPR (-, $2); }
	| '~' unary_expr	{ $$ = UEXPR (~, $2); }
	| '!' unary_expr	{ $$ = UEXPR (!, $2); }
	;

expr
    : unary_expr		{ $$ = $1; }
	| expr '?' expr ':' expr { $$ = TEXPR ($1, $3, $5); }
	| expr AND expr		{ $$ = BEXPR ($1, &&, $3); }
	| expr OR expr		{ $$ = BEXPR ($1, ||, $3); }
	| expr EQ expr		{ $$ = BEXPR ($1, ==, $3); }
	| expr NE expr		{ $$ = BEXPR ($1, !=, $3); }
	| expr LE expr		{ $$ = BEXPR ($1, <=, $3); }
	| expr GE expr		{ $$ = BEXPR ($1, >=, $3); }
	| expr LT expr		{ $$ = BEXPR ($1, < , $3); }
	| expr GT expr		{ $$ = BEXPR ($1, > , $3); }
	| expr SHL expr		{ $$ = BEXPR ($1, <<, $3); }
	| expr SHR expr		{ $$ = BEXPR ($1, >>, $3); }
	| expr '+' expr		{ $$ = BEXPR ($1, + , $3); }
	| expr '-' expr		{ $$ = BEXPR ($1, - , $3); }
	| expr '*' expr		{ $$ = BEXPR ($1, * , $3); }
	| expr '/' expr		{ $$ = BEXPR ($1, / , $3); }
	| expr '&' expr		{ $$ = BEXPR ($1, & , $3); }
	| expr '|' expr		{ $$ = BEXPR ($1, | , $3); }
	| expr '^' expr		{ $$ = BEXPR ($1, ^ , $3); }
	| expr '%' expr		{ $$ = BEXPR ($1, % , $3); }
	;

line_expr
	:	/* empty */		{ $$ = new_long_expr (0, false); }
	| line_expr VALUE
		{
			pr_long_t   flags = expr_long ($1);
			pr_long_t   bit = expr_long (get_long ($2, $<t.text>2, 0)) - 1;
			if (bit >= 0) {
				flags |= 1 << bit;
				$1 = new_long_expr (flags, false);
			}
			$$ = $1;
		}
	;

%%
