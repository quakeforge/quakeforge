%{
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#include "QF/hash.h"

#include "debug.h"
#include "expr.h"
#include "grab.h"
#include "qfcc.h"
#include "type.h"

#include "qp-parse.h"

#ifndef YY_PROTO
# define YY_PROTO(x) x
#else
# define YY_FLEX_REALLOC_HACK
#endif
int yyget_lineno  (void);
FILE *yyget_in  (void);
FILE *yyget_out  (void);
int yyget_leng  (void);
char *yyget_text  (void);
void yyset_lineno (int  line_number);
void yyset_in (FILE *  in_str);
void yyset_out (FILE *  out_str);
int yyget_debug  (void);
void yyset_debug (int  bdebug);
int yylex_destroy  (void);

#define YY_NO_UNPUT
#define YY_DECL int yylex YY_PROTO(( void ))
YY_DECL;


static int keyword_or_id (const char *token);

%}

s			[ \t]
m			[\-+]
D			[0-9]
X			[0-9a-fA-F]
ID			[a-zA-Z_][0-9a-zA-Z_]*
FLOAT		({D}+|{D}*\.{D}+|{D}+\.{D}*)([eE]{m}?{D}+)?
INTEGER		({D}+|0[xX]{X}+)
RELOP		(=|<>|[<>]=?)
ADDOP		[\-+]
MULOP		[*/]
ASSIGNOP	:=
RANGE		\.\.
FRAMEID		{ID}(\.{ID})*

%x			GRAB_FRAME GRAB_OTHER COMMENT

%%
					grab_frame = GRAB_FRAME;
					grab_other = GRAB_OTHER;

"{"					{ BEGIN (COMMENT); }
<COMMENT>"{"		{ warning (0, "nested { in comment"); }
<COMMENT>"}"		{ BEGIN (INITIAL); }
<COMMENT>\r*\n		{ pr.source_line++; }
<COMMENT>.			/* nothing to do, with people like you */
<COMMENT><<EOF>>	{ error (0, "EOF in comment"); return 0; }

^#{s}+{D}+{s}+\"(\.|[^"\n])*\".*$		{ line_info (yytext + 1); }
^#line{s}+{D}+{s}+\"(\.|[^"\n])*\".*$	{ line_info (yytext + 1); }

{INTEGER}			{
						yylval.integer_val = strtol (yytext, 0, 0);
						return INT_VAL;
					}

{FLOAT}				{
						yylval.float_val = strtof (yytext, 0);
						return FLOAT_VAL;
					}

{ID}				return keyword_or_id (yytext);

\"(\\.|[^"\\])*\"	{
						yylval.string_val = make_string (yytext, 0);
						return STRING_VAL;
					}

'{s}*{m}{FLOAT}{s}+{m}{FLOAT}{s}+{m}{FLOAT}{s}*'  {
						sscanf (yytext, "' %f %f %f '",
								&yylval.vector_val[0], &yylval.vector_val[1],
								&yylval.vector_val[2]);
						return VECTOR_VAL;
					}

'{s}*{m}{FLOAT}{s}+{m}{FLOAT}{s}+{m}{FLOAT}{s}+{m}{FLOAT}{s}*'  {
						sscanf (yytext, "' %f %f %f %f '",
								&yylval.quaternion_val[0],
								&yylval.quaternion_val[1],
								&yylval.quaternion_val[2],
								&yylval.quaternion_val[3]);
						return QUATERNION_VAL;
					}

{RELOP}				{
						yylval.string_val = save_string (yytext);
						return RELOP;
					}
{ADDOP}				{
						yylval.integer_val = yytext[0];
						return ADDOP;
					}
{MULOP}				{
						yylval.integer_val = yytext[0];
						return MULOP;
					}

{ASSIGNOP}			return ASSIGNOP;

{RANGE}				return RANGE;

[!(){}.&|^~\[\];,#%?:]	return yytext[0];

"$"{s}*{FRAMEID}	{
						int ret = do_grab (yytext);
						if (ret >= 0) {
							yylval.integer_val = ret;
							return INT_VAL;
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

int
yywrap (void)
{
	return 1;
}

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
	{"integer",		TYPE,		&type_integer},

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
};

static const char *
keyword_get_key (void *kw, void *unused)
{
	return ((keyword_t *) kw)->name;
}

static int
keyword_or_id (const char *token)
{
	static hashtab_t *keyword_tab;
	keyword_t  *keyword;

	if (!keyword_tab) {
		size_t      i;
		keyword_tab = Hash_NewTable (1021, keyword_get_key, 0, 0);
		for (i = 0; i < sizeof (keywords) / sizeof (keywords[0]); i++)
			Hash_Add (keyword_tab, &keywords[i]);
	}
	keyword = Hash_Find (keyword_tab, token);
	if (keyword) {
		if (keyword->value == ADDOP || keyword->value == MULOP) {
			yylval.op = yytext[0];
		} else {
			yylval.type = keyword->type;
		}
		return keyword->value;
	}
	yylval.string_val = save_string (token);
	return ID;
}

#ifdef YY_FLEX_REALLOC_HACK
static __attribute__ ((used)) void *(*const yy_flex_realloc_hack)(void *,yy_size_t) = yy_flex_realloc;
#else
static __attribute__ ((used)) void (*yyunput_hack)(int, char*) = yyunput;
static __attribute__ ((used)) int (*input_hack)(void) = input;
#endif

