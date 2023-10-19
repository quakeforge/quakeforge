/*
	qp-lex.l

	lexer for QuakePascal

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/06

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
%option bison-locations
%option bison-bridge
%option reentrant
%option prefix="qp_yy"
%option noyywrap
%option debug

%{
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <ctype.h>

#include "QF/hash.h"

#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/grab.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

#include "tools/qfcc/source/qp-parse.h"

#ifndef YY_PROTO
# define YY_PROTO(x) x
#else
# define YY_FLEX_REALLOC_HACK
#endif

#define YY_NO_INPUT	// debian flex
#define YY_NO_UNPUT	// debian flex
#define YY_NO_YYINPUT
#define YY_NO_YYUNPUT


#define YYSTYPE QP_YYSTYPE
#define YYLTYPE QP_YYLTYPE

void *qp_yyget_extra (yyscan_t yyscanner) __attribute__((pure));
int yyget_lineno (yyscan_t yyscanner) __attribute__((pure));
int yyget_column (yyscan_t yyscanner) __attribute__((pure));
YYSTYPE *yyget_lval (yyscan_t yyscanner) __attribute__((pure));
YYLTYPE *yyget_lloc (yyscan_t yyscanner) __attribute__((pure));
int yyget_leng (yyscan_t yyscanner) __attribute__((pure));
char *yyget_text (yyscan_t yyscanner) __attribute__((pure));
int yyget_debug (yyscan_t yyscanner) __attribute__((pure));
FILE *yyget_in (yyscan_t yyscanner) __attribute__((pure));
FILE *yyget_out (yyscan_t yyscanner) __attribute__((pure));

static int keyword_or_id (YYSTYPE *lval, const char *token);
static int convert_relop (const char *relop) __attribute__((pure));

static void
update_yylloc (YYLTYPE *lloc, const char *text)
{
	lloc->first_line = lloc->last_line;
	lloc->first_column = lloc->last_column;
	for(int i = 0; text[i] != '\0'; i++) {
		if(text[i] == '\n') {
			lloc->last_line++;
			lloc->last_column = 1;
		} else {
			lloc->last_column++;
		}
	}
}

#define YY_USER_ACTION update_yylloc (yylloc, yytext);

%}

s			[ \t]
m			[\-+]
D			[0-9]
B			[01]
X			[0-9a-fA-F]
ID			[a-zA-Z_][0-9a-zA-Z_]*
FLOAT		({D}+|{D}*\.{D}+|{D}+\.{D}*)([eE]{m}?{D}+)?
INTEGER		({D}+|0[xX]{X}+)
RELOP		(=|<>|[<>]=?)
ADDOP		[\-+]
MULOP		[*/]
ASSIGNOP	:=
RANGE		\.\.
ELLIPSIS	\.\.\.
FRAMEID		{ID}(\.{ID})*

%x			GRAB_FRAME GRAB_OTHER COMMENT

%%
					grab_frame = GRAB_FRAME;
					grab_other = GRAB_OTHER;

"{"					{ BEGIN (COMMENT); }
<COMMENT>"{"		{ warning (0, "nested { in comment"); }
<COMMENT>"}"		{ BEGIN (INITIAL); }
"(*"					{ BEGIN (COMMENT); }
<COMMENT>"(*"		{ warning (0, "nested (* in comment"); }
<COMMENT>"*)"		{ BEGIN (INITIAL); }
<COMMENT>\r*\n		{ pr.source_line++; }
<COMMENT>.			/* nothing to do, with people like you */
<COMMENT><<EOF>>	{ error (0, "EOF in comment"); return 0; }
"//".*				/* nothing to do, with people like you */

^#{s}+{D}+{s}+\"(\.|[^"\n])*\".*$		{ line_info (yytext + 1); }
^#line{s}+{D}+{s}+\"(\.|[^"\n])*\".*$	{ line_info (yytext + 1); }

{INTEGER}			{
						int         i;

						if (yytext[0] == '0' && tolower (yytext[1] == 'b'))
							i = strtol (yytext + 2, 0, 2);
						else
							i = strtol (yytext, 0, 0);
						yylval->expr = new_int_expr (i, false);
						return VALUE;
					}

{FLOAT}				{
						float       f = strtof (yytext, 0);
						yylval->expr = new_float_expr (f);
						return VALUE;
					}

{ID}				return keyword_or_id (yylval, yytext);

\"(\\.|[^"\\])*\"	{
						const char *s = make_string (yytext, 0);
						yylval->expr = new_string_expr (s);
						return VALUE;
					}

'{s}*{m}{FLOAT}{s}+{m}{FLOAT}{s}+{m}{FLOAT}{s}*'  {
						vec3_t      v;
						sscanf (yytext, "' %f %f %f '",
								&v[0], &v[1], &v[2]);
						yylval->expr = new_vector_expr (v);
						return VALUE;
					}

'{s}*{m}{FLOAT}{s}+{m}{FLOAT}{s}+{m}{FLOAT}{s}+{m}{FLOAT}{s}*'  {
						quat_t      q;
						sscanf (yytext, "' %f %f %f %f'",
								&q[0], &q[1], &q[2], &q[3]);
						yylval->expr = new_quaternion_expr (q);
						return VALUE;
					}

{RELOP}				{
						yylval->op = convert_relop (yytext);
						return RELOP;
					}
{ADDOP}				{
						yylval->op = yytext[0];
						return ADDOP;
					}
{MULOP}				{
						yylval->op = yytext[0];
						return MULOP;
					}

{ASSIGNOP}			return ASSIGNOP;

{RANGE}				return RANGE;
{ELLIPSIS}			return ELLIPSIS;

[!(){}.&|^~\[\];,#%?:]	return yytext[0];

"$"{s}*{FRAMEID}	{
						int ret = do_grab (yytext);
						if (ret >= 0) {
							yylval->expr = new_int_expr (ret, false);
							return VALUE;
						} else {
							BEGIN (-ret);
						}
					}

<GRAB_FRAME>{FRAMEID}	add_frame_macro (yytext);
<GRAB_OTHER>[^\r\n]*	/* skip */

<*>\r*\n			{
						pr.source_line++;
						BEGIN (INITIAL);
					}

<*>{s}*				/* skip whitespace */

<*>.				error (0, "all your typo are belong to us");

%%

typedef struct {
	const char *name;
	int         value;
	type_t     *type;
} keyword_t;

static keyword_t keywords[] = {
	{"real",		TYPE,		&type_float},
	{"string",		TYPE,		&type_string},
	{"vector",		TYPE,		&type_vector},
	{"entity",		TYPE,		&type_entity},
	{"quaternion",	TYPE,		&type_quaternion},
	{"integer",		TYPE,		&type_int},

	{"program",		PROGRAM,	0},
	{"var",			VAR,		0},
	{"array",		ARRAY,		0},
	{"of",			OF,			0},
	{"function",	FUNCTION,	0},
	{"procedure",	PROCEDURE,	0},
	{"begin",		PBEGIN,		0},
	{"end",			END,		0},
	{"if",			IF,			0},
	{"then",		THEN,		0},
	{"else",		ELSE,		0},
	{"while",		WHILE,		0},
	{"do",			DO,			0},
	{"or",			ADDOP,		0},
	{"div",			MULOP,		0},
	{"mod",			MULOP,		0},
	{"and",			MULOP,		0},
	{"not",			NOT,		0},

	{"return",		RETURN,		0},
};

static const char *
keyword_get_key (const void *kw, void *unused)
{
	return ((keyword_t *) kw)->name;
}

static int
keyword_or_id (YYSTYPE *lval, const char *token)
{
	static hashtab_t *keyword_tab;
	keyword_t  *keyword;
	symbol_t   *sym;

	if (!keyword_tab) {
		size_t      i;
		keyword_tab = Hash_NewTable (1021, keyword_get_key, 0, 0, 0);
		for (i = 0; i < sizeof (keywords) / sizeof (keywords[0]); i++)
			Hash_Add (keyword_tab, &keywords[i]);
	}
	keyword = Hash_Find (keyword_tab, token);
	if (keyword) {
		if (keyword->value == ADDOP || keyword->value == MULOP) {
			lval->op = token[0];
		} else {
			lval->type = keyword->type;
		}
		return keyword->value;
	}
	sym = symtab_lookup (current_symtab, token);
	if (!sym)
		sym = new_symbol (token);
	if (sym->sy_type == sy_type) {
		lval->type = sym->type;
		return TYPE_NAME;
	}
	lval->symbol = sym;
	return ID;
}

static int
convert_relop (const char *relop)
{
	switch (relop[0]) {
		case '=':
			return EQ;
		case '<':
			switch (relop[1]) {
				case 0:
					return LT;
				case '>':
					return NE;
				case '=':
					return LE;
			}
			break;
		case '>':
			switch (relop[1]) {
				case 0:
					return GT;
				case '=':
					return GE;
			}
			break;
	}
	internal_error (0, "bad relop %s", relop);
}

int
qp_yyparse (FILE *in)
{
	int         status;
	yyscan_t    scanner;
	qp_yypstate *ps = qp_yypstate_new ();

	yylex_init_extra (0, &scanner);
	yyset_in (in, scanner);
	YYLTYPE lloc = { 1, 1, 1, 1 };
	do {
		YYSTYPE lval;
		int token = yylex (&lval, &lloc, scanner);
		status = qp_yypush_parse (ps, token, &lval, &lloc, scanner);
	} while (status == YYPUSH_MORE);

	yylex_destroy (scanner);
	qp_yypstate_delete (ps);
	return status;
}
