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

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/pragma.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/type.h"

#define YYDEBUG 1
#define YYERROR_VERBOSE 1
#undef YYERROR_VERBOSE

#include "tools/qfcc/source/pre-parse.h"

#define pre_yytext qc_yyget_text (scanner)
char *qc_yyget_text (void *scanner);

static void
yyerror (YYLTYPE *yylloc, void *scanner, const char *s)
{
	const char *text = quote_string (pre_yytext);
#ifdef YYERROR_VERBOSE
	error (0, "%d:%s before '%s'\n", yylloc->first_column, s, text);
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
%}

%code requires {
#include "tools/qfcc/include/rua-lang.h"
}
%define api.value.type {rua_tok_t}
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
%right  <op>    SIZEOF UNARY INCOP REVERSE STAR DUAL
%left           HYPERUNARY
%left           '.' '(' '['

%token <value.expr> VALUE STRING
%token          TOKEN
// end of tokens comment between qc and preprocessor

%token			QSTRING HSTRING

%token          INCLUDE EMBED
%token          DEFINE UNDEF
%token <text>   TEXT ID IDp
%token          ERROR WARNING
%token          PRAGMA LINE
%token          IF IFDEF IFNDEF ELSE ELIF ELIFDEF ELIFNDEF ENDIF
%token          DEFINED EOD
%token          CONCAT ARGS

%type <macro>   params body arg arg_list
%type <dstr>    text text_text
%type <value.expr>  unary_expr expr id defined defined_id

%{
#define BEXPR(a,op,b) new_long_expr (expr_long (a) op expr_long (b), false)
#define UEXPR(op,a)   new_long_expr (op expr_long (a), false)
%}

%%

start
	: directive_list
	| ARGS
		<macro>{
			auto arg = rua_start_macro (0, false, scanner);
			$$ = rua_macro_arg (arg, scanner);
		}
	  args ')' { YYACCEPT; }
	;

directive_list
	: /*empty*/
	| directive EOD { rua_end_directive (scanner); } directive_list
	;

directive
	: INCLUDE expand string extra_warn
	| EMBED expand string extra_ignore
	| DEFINE ID		<macro> { $$ = rua_start_macro ($2, false, scanner); }
	  body					{ rua_macro_finish ($body, scanner); }
	| DEFINE IDp	<macro> { $$ = rua_start_macro ($2, true, scanner); }
	  params ')'	<macro> { $$ = rua_end_params ($3, scanner); }
	  body					{ rua_macro_finish ($body, scanner); }
	| UNDEF ID extra_warn	{ rua_undefine ($2, scanner); }
	| ERROR text { error (0, "%s", $text->str); dstring_delete ($text); }
	| WARNING text { warning (0, "%s", $text->str); dstring_delete ($text); }
	| PRAGMA expand { rua_start_pragma (scanner); }
	  pragma_params { pragma_process (); }
	| LINE expand expr QSTRING extra_warn
	| IF expand expr		{ rua_if (expr_long ($3), scanner); }
	| IFDEF ID extra_warn	{ rua_if (rua_defined ($2, scanner), scanner); }
	| IFNDEF ID extra_warn	{ rua_if (!rua_defined ($2, scanner), scanner); }
	| ELSE extra_warn		{ rua_else (true, "else", scanner); }
	| ELIF expand expr		{ rua_else (expr_long ($3), "elif", scanner); }
	| ELIFDEF ID extra_warn
		{
			rua_else (rua_defined ($2, scanner), "elifdef", scanner);
		}
	| ELIFNDEF ID extra_warn
		{
			rua_else (!rua_defined ($2, scanner), "elifndef", scanner);
		}
	| ENDIF extra_warn		{ rua_endif (scanner); }
	;

extra_warn
	: {}
	;

extra_ignore
	: {}
	;

text: { rua_start_text (scanner); $<dstr>$ = dstring_new (); } text_text
	  { $text = $text_text; }
	;
text_text
	: TEXT				{ dstring_appendstr ($<dstr>0, $1); $$ = $<dstr>0; }
	| text_text TEXT			{ dstring_appendstr ($1, $2); $$ = $1; }
	;

body: /* empty */		{ $$ = $<macro>0; }
	| body body_token	{ $$ = rua_macro_append ($1, yyvsp, scanner); }
	;

body_token : TOKEN | ',' | '(' | ')' ;

expand
	: { rua_start_expr (scanner); }
	;

pragma_params
	: ID						{ pragma_add_arg ($1); }
	| pragma_params ID			{ pragma_add_arg ($2); }
	;

string
	: HSTRING
	| QSTRING
	;

params
	: TOKEN				{ $$ = rua_macro_param ($<macro>0, yyvsp, scanner); }
	| params ',' TOKEN	{ $$ = rua_macro_param ($1, yyvsp, scanner); }
	;

args: arg_list
	| args ','
		<macro>{
			auto arg = rua_start_macro (0, false, scanner);
			$$ = rua_macro_arg (arg, scanner);
		}
	  arg_list
	;

arg_list
	: /* emtpy */	{ $$ = $<macro>0; }
	| arg_list arg	{ $$ = $2; }
	;

arg : '('	<macro>	{ $$ = rua_macro_append ($<macro>0, yyvsp, scanner); }
	  args	<macro>	{ $$ = rua_macro_append ($<macro>0, yyvsp, scanner); }
	  ')'			{ $$ = rua_macro_append ($<macro>0, yyvsp, scanner); }
	| TOKEN			{ $$ = rua_macro_append ($<macro>0, yyvsp, scanner); }
	;

id  : ID			{ $$ = 0; }
	| IDp args ')'	{ $$ = 0; }
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
	| VALUE
		{
			auto type = get_type ($1);
			if (!is_long (type)) {
				if (is_double (type)) {
					error (0, "floating constant in preprocessor expression");
					$1 = new_long_expr (expr_double ($1), false);
				} else {
					error (0, "token \"%s\" is not valid in preprocessor"
						   " expressions", $<text>1);
					$1 = new_long_expr (1, false);
				}
			}
			$$ = $1;
		}
	| QSTRING
		{
			error (0, "token \"%s\" is not valid in preprocessor"
				   " expressions", $<text>1);
			$$ = new_long_expr (1, false);
		}
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

%%
