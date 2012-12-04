%{
/*
	qc-lex.l

	lexer for quakec

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
#include <ctype.h>

#include <QF/dstring.h>
#include <QF/hash.h>
#include <QF/sys.h>

#include "class.h"
#include "debug.h"
#include "diagnostic.h"
#include "expr.h"
#include "grab.h"
#include "options.h"
#include "pragma.h"
#include "qfcc.h"
#include "shared.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"
#include "value.h"

#include "qc-parse.h"

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


static int keyword_or_id (char *token);

extern QC_YYSTYPE qc_yylval;

%}

s			[ \t]
m			[\-+]
D			[0-9]
X			[0-9a-fA-F]
ID			[a-zA-Z_][a-zA-Z_0-9]*
FLOAT		({D}+|{D}*\.{D}+|{D}+\.{D}*)([eE]{m}?{D}+)?
INT			({D}+|0[xX]{X}+)
RANGE		\.\.
ELLIPSIS	\.\.\.
FRAMEID		{ID}(\.{ID})*
PRAGMAID	{ID}(-{ID})*
STRING		\"(\\.|[^"\\])*\"

%x		GRAB_FRAME GRAB_OTHER COMMENT PRAGMA

%%
					grab_frame = GRAB_FRAME;
					grab_other = GRAB_OTHER;

"/*"				{ BEGIN (COMMENT); }
<COMMENT>"/*"		{ warning (0, "nested /* in comment"); }
<COMMENT>"*/"		{ BEGIN (INITIAL); }
<COMMENT>\r*\n		{ pr.source_line++; }
<COMMENT>.			/* nothing to do */
<COMMENT><<EOF>>	{ error (0, "EOF in comment"); return 0; }
"//".*				/* nothing to do */

^#{s}+{D}+{s}+\"(\.|[^"\n])*\".*$ { line_info (yytext + 1); }
^#line{s}+{D}+{s}+\"(\.|[^"\n])*\".*$ { line_info (yytext + 5); }

^{s}*#{s}*pragma{s}+	{ BEGIN (PRAGMA); }

{INT}+[uU]?			{
						const char *c = yytext + yyleng - 1;
						int         i = strtol (yytext, 0, 0);
						if (*c == 'u' || *c == 'U')
							qc_yylval.expr = new_integer_expr (i);//FIXME
						else
							qc_yylval.expr = new_integer_expr (i);
						return CONST;
					}

{FLOAT}				{
						float       f = strtof (yytext, 0);
						qc_yylval.expr = new_float_expr (f);
						return CONST;
					}

{ID}				{
						int         tok = keyword_or_id(yytext);
						return tok;
					}
@{ID}				{
						int         tok = keyword_or_id(yytext);
						if (tok == '@')
							REJECT;
						return tok;
					}

{STRING}			{
						const char *s = make_string (yytext, 0);
						qc_yylval.expr = new_string_expr (s);
						return STRING;
					}
@					return '@';

'{s}*{m}?{FLOAT}{s}+{m}?{FLOAT}{s}+{m}?{FLOAT}{s}*'	{
						vec3_t      v;
						sscanf (yytext, "' %f %f %f '",
								&v[0], &v[1], &v[2]);
						qc_yylval.expr = new_vector_expr (v);
						return CONST;
					}

'{s}*{m}?{FLOAT}{s}+{m}?{FLOAT}{s}+{m}?{FLOAT}{s}+{m}?{FLOAT}{s}*'	{
						quat_t      q;
						sscanf (yytext, "' %f %f %f %f'",
								&q[0], &q[1], &q[2], &q[3]);
						qc_yylval.expr = new_quaternion_expr (q);
						return CONST;
					}

'(\\[^xX0-7\r\n]|[^'\r\n]|\\[xX][0-9A-Fa-f]+|\\[0-7]+)*'	{
						const char *str = make_string (yytext, 0);

						if (str[1])
							warning (0, "multibyte char constant");
						qc_yylval.expr = new_integer_expr (*str);
						return CONST;
					}

[+\-*/&|^%]=		{
						qc_yylval.op = yytext[0];
						return ASX;
					}

"<<="				{
						qc_yylval.op = SHL;
						return ASX;
					}

">>="				{
						qc_yylval.op = SHR;
						return ASX;
					}

[!(){}.*/&|^~+\-=\[\];,#%?:] {
						qc_yylval.pointer = 0;	// ensure pointer vals are null
						return yytext[0];
					}

{ELLIPSIS}			return ELLIPSIS;

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
						qc_yylval.op = '+';
						return INCOP;
					}

"--"				{
						qc_yylval.op = '-';
						return INCOP;
					}

"$"{s}*{FRAMEID}	{
						int ret = do_grab (yytext);
						if (ret >= 0) {
							qc_yylval.expr = new_integer_expr (ret);
							return CONST;
						} else {
							BEGIN (-ret);
						}
					}

<GRAB_FRAME>{FRAMEID}	add_frame_macro (yytext);
<GRAB_OTHER>[^\r\n]*	/* skip */
<PRAGMA>{ID}		{ pragma (yytext); }

<*>\r*\n			{
						pr.source_line++;
						BEGIN (INITIAL);
					}

<*>{s}*				/* skip */

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
	int         traditional;
	unsigned    version;
	int         objc;
} keyword_t;

static keyword_t keywords[] = {
	{"void",		TYPE,	&type_void,			2, PROG_ID_VERSION,	0},
	{"float",		TYPE,	&type_float,		2, PROG_ID_VERSION,	0},
	{"string",		TYPE,	&type_string,		2, PROG_ID_VERSION,	0},
	{"vector",		TYPE,	&type_vector,		2, PROG_ID_VERSION,	0},
	{"entity",		TYPE,	&type_entity,		2, PROG_ID_VERSION,	0},
	{"quaternion",	TYPE,	&type_quaternion,	0, PROG_VERSION,	0},
	{"int",			TYPE,	&type_integer,		0, PROG_VERSION,	0},
	{"unsigned",	TYPE,	&type_integer,		0, PROG_VERSION,	0},//FIXME
	{"function",	TYPE,	&type_function,		0, PROG_VERSION,	0},
	{"id",			TYPE,	&type_id,			0, PROG_VERSION,	1},
	{"Class",		TYPE,	&type_Class,		0, PROG_VERSION,	1},
//	{"Protocol",	TYPE,	&type_Protocol,		0, PROG_VERSION,	0},
	{"Method",		TYPE,	&type_obj_method,	0, PROG_VERSION,	1},
	{"Super",		TYPE,	&type_obj_super,	0, PROG_VERSION,	1},
	{"SEL",			TYPE,	&type_SEL,			0, PROG_VERSION,	1},
	{"IMP",			TYPE,	&type_IMP,			0, PROG_VERSION,	1},
	{"local",		LOCAL,	0,					2, PROG_ID_VERSION,	0},
	{"return",		RETURN,	0,					2, PROG_ID_VERSION,	0},
	{"while",		WHILE,	0,					2, PROG_ID_VERSION,	0},
	{"do",			DO,		0,					2, PROG_ID_VERSION,	0},
	{"if",			IF,		0,					2, PROG_ID_VERSION,	0},
	{"else",		ELSE,	0,					2, PROG_ID_VERSION,	0},
	{"for",			FOR,	0,					1, PROG_ID_VERSION,	0},
	{"break",		BREAK,	0,					2, PROG_ID_VERSION,	0},
	{"continue",	CONTINUE, 0,				1, PROG_ID_VERSION,	0},
	{"switch",		SWITCH,	0,					1, PROG_ID_VERSION,	0},
	{"case",		CASE,	0,					1, PROG_ID_VERSION,	0},
	{"default",		DEFAULT, 0,					1, PROG_ID_VERSION,	0},
	{"nil",			NIL,	0,					0, PROG_ID_VERSION,	0},
	{"@nil",		NIL,	0,					2, PROG_ID_VERSION,	0},
	{"struct",		STRUCT, 0,					0, PROG_VERSION,	0},
	{"union",		STRUCT,	0,					0, PROG_VERSION,	0},
	{"enum",		ENUM,	0,					0, PROG_ID_VERSION,	0},
	{"typedef",		TYPEDEF, 0,					0, PROG_ID_VERSION,	0},

	// this is a hack to trigger the initialization of the class
	// sytem if they are seen before any other Objective-QC symbol
	{"obj_module", 0,		0,					0, PROG_VERSION,	1},

	{"@class",		CLASS,	0,					0, PROG_VERSION,	1},
	{"@defs",		DEFS,	0,					0, PROG_VERSION,	1},
	{"@encode",		ENCODE,	0,					0, PROG_VERSION,	1},
	{"@end",		END,	0,					0, PROG_VERSION,	1},
	{"@implementation", IMPLEMENTATION, 0,		0, PROG_VERSION,	1},
	{"@interface",	INTERFACE, 0,				0, PROG_VERSION,	1},
	{"@private",	PRIVATE, 0,					0, PROG_VERSION,	1},
	{"@protected",	PROTECTED, 0,				0, PROG_VERSION,	1},
	{"@protocol",	PROTOCOL, 0,				0, PROG_VERSION,	1},
	{"@public",		PUBLIC,	0,					0, PROG_VERSION,	1},
	{"@reference",	REFERENCE, 0,				0, PROG_VERSION,	1},
	{"@selector",	SELECTOR, 0,				0, PROG_VERSION,	1},
	{"@self",		SELF,	0,					0, PROG_VERSION,	1},
	{"@this",		THIS,	0,					0, PROG_VERSION,	1},
	{"@args",		ARGS,	0,					0, PROG_VERSION,	0},
	{"@va_list",	TYPE,	&type_va_list,		0, PROG_VERSION,	0},
	{"@param",		TYPE,	&type_param,		0, PROG_VERSION,	0},
	{"@extern",		EXTERN,	0,					1, PROG_ID_VERSION,	0},
	{"@static",		STATIC,	0,					1, PROG_ID_VERSION,	0},
	{"@system",		SYSTEM,	0,					1, PROG_ID_VERSION,	0},
	{"@sizeof",		SIZEOF,	0,					0, PROG_VERSION,	0},
	{"@overload",	OVERLOAD, 0,				0, PROG_VERSION,	0},
};

static const char *
keyword_get_key (const void *kw, void *unused)
{
	return ((keyword_t*)kw)->name;
}

static int
keyword_or_id (char *token)
{
	static hashtab_t *keyword_tab;
	keyword_t  *keyword;
	symbol_t   *sym;

	if (!keyword_tab) {
		size_t      i;
		keyword_tab = Hash_NewTable (1021, keyword_get_key, 0, 0);
		for (i = 0; i < sizeof (keywords) / sizeof (keywords[0]); i++)
			if (keywords[i].traditional >= options.traditional
				&& keywords[i].version <= options.code.progsversion)
				Hash_Add (keyword_tab, &keywords[i]);
	}
	keyword = Hash_Find (keyword_tab, token);
	if (keyword) {
		if (!options.traditional && keyword->objc && !obj_initialized)
			class_init ();
		if (keyword->value) {
			if (keyword->value == STRUCT) {
				qc_yylval.op = token[0];
			} else {
				qc_yylval.type = 0;
				qc_yylval.type = keyword->type;
			}
			return keyword->value;
		}
	}
	if (token[0] == '@') {
		return '@';
	}
	sym = symtab_lookup (current_symtab, token);
	if (!sym)
		sym = new_symbol (token);
	qc_yylval.symbol = sym;
	if (sym->sy_type == sy_type)
		return TYPE_NAME;
	if (sym->sy_type == sy_class)
		return CLASS_NAME;
	return NAME;
}

#ifdef YY_FLEX_REALLOC_HACK
static __attribute__ ((used)) void *(*const yy_flex_realloc_hack)(void *,yy_size_t) = yy_flex_realloc;
#else
static __attribute__ ((used)) void (*yyunput_hack)(int, char*) = yyunput;
static __attribute__ ((used)) int (*input_hack)(void) = input;
#endif
