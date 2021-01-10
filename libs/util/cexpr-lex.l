/*
	cexpr-lex.l

	Config expression parser. Or concurrent.

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

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
%option bison-bridge
%option reentrant
%option prefix="cexpr_yy"
%option noyywrap

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
#include <ctype.h>

#include "QF/cmem.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/qfplist.h"
#include "QF/sys.h"

#include "QF/cexpr.h"
#include "libs/util/cexpr-parse.h"

#define YY_NO_INPUT
#define YY_NO_UNPUT

#define YYSTYPE CEXPR_YYSTYPE
#define YY_EXTRA_TYPE exprctx_t *

exprctx_t *cexpr_yyget_extra (yyscan_t yyscanner) __attribute__((pure));
int cexpr_yyget_lineno (yyscan_t yyscanner) __attribute__((pure));
int cexpr_yyget_column (yyscan_t yyscanner) __attribute__((pure));
YYSTYPE *cexpr_yyget_lval (yyscan_t yyscanner) __attribute__((pure));
int cexpr_yyget_debug (yyscan_t yyscanner) __attribute__((pure));
char *cexpr_yyget_text (yyscan_t yyscanner) __attribute__((pure));
int cexpr_yyget_leng (yyscan_t yyscanner) __attribute__((pure));
FILE *cexpr_yyget_out (yyscan_t yyscanner) __attribute__((pure));
FILE *cexpr_yyget_in (yyscan_t yyscanner) __attribute__((pure));

static exprval_t *parse_int (const char *str, exprctx_t *context);
static exprval_t *parse_uint (const char *str, exprctx_t *context);
static exprval_t *parse_float (const char *str, exprctx_t *context);
static exprval_t *parse_double (const char *str, exprctx_t *context);
static exprsym_t *parse_name (const char *str, exprctx_t *context);
static exprval_t *parse_variable (const char *str, exprctx_t *context);

VISIBLE void
cexpr_error(exprctx_t *ctx, const char *fmt, ...)
{
	va_list     args;
	dstring_t  *string;

	ctx->errors++;

	string = dstring_new ();

	va_start (args, fmt);
	dvsprintf (string, fmt, args);
	va_end (args);

	if (ctx->messages) {
		PL_Message (ctx->messages, ctx->item, "%s", string->str);
	} else {
		Sys_Printf ("%s\n", string->str);
	}
	dstring_delete (string);
}

%}

s			[ \t]
m			[\-+]
D			[0-9]
B			[01]
X			[0-9a-fA-F]
ID			[a-zA-Z_][a-zA-Z_0-9]*
FLOAT		({D}+|{D}*\.{D}+|{D}+\.{D}*)([eE]{m}?{D}+)?
FLOATf		{FLOAT}[fF]
FLOATd		{FLOAT}[dD]
INT			({D}+|0[xX]{X}+|0[bB]{B})
STRING		\"(\\.|[^"\\])*\"

%%
%{
	__auto_type context = yyget_extra (yyscanner);
%}

{INT}+				{
						yylval->value = parse_int (yytext, context);
						return VALUE;
					}

{INT}+[uU]			{
						yylval->value = parse_uint (yytext, context);
						return VALUE;
					}

{FLOAT}				{
						yylval->value = parse_double (yytext, context);
						return VALUE;
					}
{FLOATf}			{
						yylval->value = parse_float (yytext, context);
						return VALUE;
					}
{FLOATd}			{
						yylval->value = parse_double (yytext, context);
						return VALUE;
					}

{ID}				{
						yylval->symbol = parse_name (yytext, context);
						return NAME;
					}

\${ID}				{
						yylval->value = parse_variable (yytext + 1, context);
						return VALUE;
					}

{STRING}			{
					}
@					return '@';

'(\\[^xX0-7\r\n]|[^'\r\n]|\\[xX][0-9A-Fa-f]+|\\[0-7]+)*'	{
					}

[+\-*/&|^%]=		{
					}

"%%="				{
					}

"<<="				{
					}

">>="				{
					}

[!(){}.*/&|^~+\-=\[\];,#%?:] {
						return yytext[0];
					}

"%%"				{
					}

"<<"				return SHL;
">>"				return SHR;

"&&"				return AND;
"||"				return OR;
"=="				return EQ;
"!="				return NE;
"<="				return LE;
">="				return GE;
"<"					return LT;
">"					return GT;

"++"				{
					}

"--"				{
					}

<*>\r*\n			{
					}

<*>{s}*				/* skip */

<*>.				cexpr_error (context, "all your typo are belong to us");

%%

VISIBLE int
cexpr_eval_string (const char *str, exprctx_t *context)
{
	int         status;
	yyscan_t    scanner;
	cexpr_yypstate *ps = cexpr_yypstate_new ();

	yylex_init_extra (context, &scanner);
	yy_scan_string (str, scanner);

	context->errors = 0;
	do {
		CEXPR_YYSTYPE lval;
		int         token = yylex (&lval, scanner);
		status = cexpr_yypush_parse (ps, token, &lval, scanner, context);
	} while (status == YYPUSH_MORE);

	yylex_destroy (scanner);
	cexpr_yypstate_delete (ps);
	return status || context->errors;
}

static exprval_t *parse_int (const char *str, exprctx_t *context)
{
	exprval_t  *val = cexpr_value (&cexpr_int, context);
	*(int *) val->value = strtoimax (str, 0, 0);
	return val;
}

static exprval_t *parse_uint (const char *str, exprctx_t *context)
{
	exprval_t  *val = cexpr_value (&cexpr_uint, context);
	*(unsigned *) val->value = strtoumax (str, 0, 0);
	return val;
}

static exprval_t *parse_float (const char *str, exprctx_t *context)
{
	exprval_t  *val = cexpr_value (&cexpr_float, context);
	*(float *) val->value = strtof (str, 0);
	return val;
}

static exprval_t *parse_double (const char *str, exprctx_t *context)
{
	exprval_t  *val = cexpr_value (&cexpr_double, context);
	*(double *) val->value = strtod (str, 0);
	return val;
}

static exprsym_t *
parse_name (const char *name, exprctx_t *context)
{
	exprtab_t  *symtab = context->symtab;

	if (!symtab) {
		return 0;
	}
	__auto_type sym = (exprsym_t *) Hash_Find (symtab->tab, name);
	return sym;
}

static exprval_t *
parse_variable (const char *name, exprctx_t *context)
{
	exprval_t  *val = 0;
	if (strcmp (name, "cvars") == 0) {
		val = cexpr_cvar_struct (context);
	} else {
		exprtab_t  *symtab = context->external_variables;
		__auto_type sym = (exprsym_t *) Hash_Find (symtab->tab, name);
		if (sym) {
			val = cexpr_value_reference (sym->type, sym->value, context);
		} else {
			//val = cexpr_cvar (name, context);
		}
	}
	if (!val) {
		cexpr_error (context, "undefined variable %s", name);
	}
	return val;
}
