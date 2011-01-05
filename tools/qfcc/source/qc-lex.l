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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

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

#include "qfcc.h"
#include "expr.h"
#include "class.h"
#include "immediate.h"
#include "options.h"
#include "struct.h"
#include "type.h"
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


int type_or_name (char *token);
int do_grab (char *token);
void add_frame_macro (char *token);
const char *make_string (char *token, char **end);
void push_source_file (void);
void pop_source_file (void);
void line_info (char *text);

extern YYSTYPE yylval;

%}

DIGIT	[0-9]
XDIGIT	[0-9a-fA-F]
ID		[a-zA-Z_][a-zA-Z_0-9]*
FLOAT	{DIGIT}+"."{DIGIT}*
NUM		({DIGIT}+("."{DIGIT}*)?)
s		[ \t]
m		([\-+]?)
FRAMEID {ID}(\.{ID})*

%x		grab_frame grab_other comment

%%

"/*"				{ BEGIN (comment); }
<comment>"/*"		{ warning (0, "nested /* in comment"); }
<comment>"*/"		{ BEGIN (INITIAL); }
<comment>\r*\n		{ pr.source_line++; }
<comment>.			/* nothing to do */
<comment><<EOF>>	{ error (0, "EOF in comment"); return 0; }
"//".*				/* nothing to do */

^#{s}+{DIGIT}+{s}+\"(\.|[^"\n])*\".*$ { line_info (yytext + 1); }
^#line{s}+{DIGIT}+{s}+\"(\.|[^"\n])*\".*$ { line_info (yytext + 5); }

^{s}*#{s}*pragma.*$	/* skip */

{DIGIT}+[uU]?		{
						const char *c = yytext + yyleng - 1;
						int         uint = 0;
						yylval.integer_val = atoi (yytext);
						if (*c == 'u' || *c == 'U')
							uint = 1;
						return uint ? UINT_VAL : INT_VAL;
					}

0[xX]{XDIGIT}+[uU]?	{
						const char *c = yytext + 2;
						int         uint = 0;
						yylval.uinteger_val = 0;
						while (*c) {
							if (*c == 'u' || *c == 'U') {
								uint = 1;
								break;
							}
							yylval.uinteger_val *= 16;
							yylval.uinteger_val += *c - '0';
							if (*c > '9')
								yylval.uinteger_val -= 'A' - '9' - 1;
							if (*c > 'F')
								yylval.uinteger_val -= 'a' - 'A';
							c++;
						}
						return uint ? UINT_VAL : INT_VAL;
					}

{FLOAT}*			{
						yylval.float_val = atof (yytext);
						return FLOAT_VAL;
					}

{ID}				return type_or_name(yytext);
@{ID}				{
						int         tok = type_or_name(yytext);
						if (tok == '@')
							REJECT;
						return tok;
					}
@					return '@';

\"(\\.|[^"\\])*\"	{
						yylval.string_val = make_string (yytext, 0);
						return STRING_VAL;
					}

'{s}*{m}{NUM}{s}+{m}{NUM}{s}+{m}{NUM}{s}*'	{
						sscanf (yytext, "' %f %f %f '",
								&yylval.vector_val[0], &yylval.vector_val[1],
								&yylval.vector_val[2]);
						return VECTOR_VAL;
					}

'{s}*{m}{NUM}{s}+{m}{NUM}{s}+{m}{NUM}{s}+{m}{NUM}{s}*'	{
						sscanf (yytext, "' %f %f %f %f '",
								&yylval.quaternion_val[0],
								&yylval.quaternion_val[1],
								&yylval.quaternion_val[2],
								&yylval.quaternion_val[3]);
						return QUATERNION_VAL;
					}

'(\\[^xX0-7\r\n]|[^'\r\n]|\\[xX][0-9A-Fa-f]+|\\[0-7]+)*'	{
						const char *str = make_string (yytext, 0);

						if (str[1])
							warning (0, "multibyte char constant");
						yylval.integer_val = *str;
						return INT_VAL;
					}

[+\-*/&|^%]=		{
						yylval.op = yytext[0];
						return ASX;
					}

"<<="				{
						yylval.op = SHL;
						return ASX;
					}

">>="				{
						yylval.op = SHR;
						return ASX;
					}

[!(){}.*/&|^~+\-=\[\];,#%?:] {
						return yytext[0];
					}

"..."	return ELLIPSIS;

"<<"	return SHL;
">>"	return SHR;

"&&"	return AND;
"||"	return OR;
"=="	return EQ;
"!="	return NE;
"<="	return LE;
">="	return GE;
"<"		return LT;
">"		return GT;

"++"				{
						yylval.op = '+';
						return INCOP;
					}

"--"				{
						yylval.op = '-';
						return INCOP;
					}

"$"{s}*{FRAMEID}	{
						int ret = do_grab (yytext);
						if (ret > 0)
							return ret;
						else
							BEGIN (-ret);
					}

<grab_frame>{FRAMEID}	add_frame_macro (yytext);
<grab_other>[^\r\n]*	/* skip */

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
	const char	*name;
	int			value;
	type_t		*type;
	unsigned int traditional;
	unsigned int version;
} keyword_t;

static keyword_t keywords[] = {
	{"void",		TYPE,	&type_void,			1, PROG_ID_VERSION},
	{"float",		TYPE,	&type_float,		1, PROG_ID_VERSION},
	{"string",		TYPE,	&type_string,		1, PROG_ID_VERSION},
	{"vector",		TYPE,	&type_vector,		1, PROG_ID_VERSION},
	{"entity",		TYPE,	&type_entity,		1, PROG_ID_VERSION},
	{"quaternion",	TYPE,	&type_quaternion,	0, PROG_VERSION},
	{"integer",		TYPE,	&type_integer,		0, PROG_VERSION},
	{"unsigned",	TYPE,	&type_uinteger,		0, PROG_VERSION},
	{"function",	TYPE,	&type_function,		0, PROG_VERSION},
	{"id",			TYPE,	&type_id,			0, PROG_VERSION},
	{"Class",		TYPE,	&type_Class,		0, PROG_VERSION},
//	{"Protocol",	TYPE,	&type_Protocol,		0, PROG_VERSION},
	{"Method",		TYPE,	&type_Method,		0, PROG_VERSION},
	{"Super",		TYPE,	&type_Super,		0, PROG_VERSION},
	{"SEL",			TYPE,	&type_SEL,			0, PROG_VERSION},
	{"IMP",			TYPE,	&type_IMP,			0, PROG_VERSION},
	{"local",		LOCAL,	0,					1, PROG_ID_VERSION},
	{"return",		RETURN,	0,					1, PROG_ID_VERSION},
	{"while",		WHILE,	0,					1, PROG_ID_VERSION},
	{"do",			DO,		0,					1, PROG_ID_VERSION},
	{"if",			IF,		0,					1, PROG_ID_VERSION},
	{"else",		ELSE,	0,					1, PROG_ID_VERSION},
	{"for",			FOR,	0,					0, PROG_ID_VERSION},
	{"break",		BREAK,	0,					1, PROG_ID_VERSION},
	{"continue",	CONTINUE, 0,				0, PROG_ID_VERSION},
	{"switch",		SWITCH,	0,					0, PROG_ID_VERSION},
	{"case",		CASE,	0,					0, PROG_ID_VERSION},
	{"default",		DEFAULT, 0,					0, PROG_ID_VERSION},
	{"NIL",			NIL,	0,					0, PROG_ID_VERSION},
	{"struct",		STRUCT, 0,					0, PROG_VERSION},
	{"union",		UNION,	0,					0, PROG_VERSION},
	{"enum",		ENUM,	0,					0, PROG_ID_VERSION},
	{"typedef",		TYPEDEF, 0,					0, PROG_ID_VERSION},
	{"super",		SUPER,	0,					0, PROG_VERSION},

	{"@class",		CLASS,	0,					0, PROG_VERSION},
	{"@defs",		DEFS,	0,					0, PROG_VERSION},
	{"@encode",		ENCODE,	0,					0, PROG_VERSION},
	{"@end",		END,	0,					0, PROG_VERSION},
	{"@implementation", IMPLEMENTATION, 0,		0, PROG_VERSION},
	{"@interface",	INTERFACE, 0,				0, PROG_VERSION},
	{"@private",	PRIVATE, 0,					0, PROG_VERSION},
	{"@protected",	PROTECTED, 0,				0, PROG_VERSION},
	{"@protocol",	PROTOCOL, 0,				0, PROG_VERSION},
	{"@public",		PUBLIC,	0,					0, PROG_VERSION},
	{"@reference",	REFERENCE, 0,				0, PROG_VERSION},
	{"@selector",	SELECTOR, 0,				0, PROG_VERSION},
	{"@self",		SELF,	0,					0, PROG_VERSION},
	{"@this",		THIS,	0,					0, PROG_VERSION},
	{"@args",		ARGS,	0,					0, PROG_VERSION},
	{"@va_list",	TYPE,	&type_va_list,		0, PROG_VERSION},
	{"@param",		TYPE,	&type_param,		0, PROG_VERSION},
	{"@extern",		EXTERN,	0,					1, PROG_ID_VERSION},
	{"@static",		STATIC,	0,					1, PROG_ID_VERSION},
	{"@system",		SYSTEM,	0,					1, PROG_ID_VERSION},
	{"@sizeof",		SIZEOF,	0,					0, PROG_VERSION},
	{"@overload",	OVERLOAD, 0,				0, PROG_VERSION},
};

static const char *
keyword_get_key (void *kw, void *unused)
{
	return ((keyword_t*)kw)->name;
}

int
type_or_name (char *token)
{
	static hashtab_t *keyword_tab;
	keyword_t  *keyword;
	typedef_t  *typename;
	class_t    *class;

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
		if (!options.traditional && token[0] == '@'
			&& !class_Class.super_class)
			class_init ();
		yylval.type = keyword->type;
		return keyword->value;
	}
	if (token[0] == '@') {
		return '@';
	}
	if ((typename = get_typedef (token))) {
		yylval.typename = typename;
		return TYPE_NAME;
	}
	if (!get_enum (token)) {
		if ((class = get_class (token, 0))) {
			yylval.string_val = save_string (token);
			return CLASS_NAME;
		}
	}
	yylval.string_val = save_string (token);
	return NAME;
}

static hashtab_t *frame_tab;
static hashtab_t *grab_tab;

typedef struct frame_s {
	struct frame_s *next;
	const char *name;
	int			num;
} frame_t;

static frame_t *free_frames;

static frame_t grab_list[] = {
	{0, "cd",		0},
	{0, "origin",	0},
	{0, "base",		0},
	{0, "flags",	0},
	{0, "scale",	0},
	{0, "skin",		0},
};

static const char *
frame_get_key (void *f, void *unused)
{
	return ((frame_t*)f)->name;
}

static void
frame_free (void *_f, void *unused)
{
	frame_t    *f = (frame_t *)_f;
	f->next = free_frames;
	free_frames = f;
}

int
do_grab (char *token)
{
	static int initialized;
	frame_t *frame;

	if (!initialized) {
		size_t      i;

		initialized = 1;
		frame_tab = Hash_NewTable (1021, frame_get_key, frame_free, 0);
		grab_tab = Hash_NewTable (1021, frame_get_key, 0, 0);
		for (i = 0; i < sizeof (grab_list) / sizeof (grab_list[0]); i++)
			Hash_Add (grab_tab, &grab_list[i]);
	}
	while (isspace ((unsigned char)*++token))
		// advance over $ and leading space
		;
	if (!strcmp (token, "frame"))
		return -grab_frame;
	if (!strcmp (token, "frame_reset")) {
		clear_frame_macros ();
		return -grab_other;
	}
	if (Hash_Find (grab_tab, token))
		return -grab_other;
	frame = Hash_Find (frame_tab, token);
	if (frame) {
		yylval.integer_val = frame->num;
		return INT_VAL;
	}
	return 0;
}

static int frame_number;

void
add_frame_macro (char *token)
{
	frame_t *frame;

	if (Hash_Find (frame_tab, token)) {
		warning (0, "duplicate frame macro `%s'", token);
		if (options.traditional)
			frame_number++;
		return;
	}
	ALLOC (1024, frame_t, frames, frame);

	frame->name = save_string (token);
	frame->num = frame_number++;
	Hash_Add (frame_tab, frame);
}

void
clear_frame_macros (void)
{
	frame_number = 0;
	if (frame_tab)
		Hash_FlushTable (frame_tab);
}

const char *
make_string (char *token, char **end)
{
	char        s[2];
	int         c;
	int         i;
	int         mask;
	int         boldnext;
	int         quote;
	static dstring_t *str;

	if (!str)
		str = dstring_newstr ();
	dstring_clearstr (str);

	s[1] = 0;

	mask = 0x00;
	boldnext = 0;

	quote = *token++;
	do {
		c = *token++;
		if (!c)
			error (0, "EOF inside quote");
		if (c == '\n')
			error (0, "newline inside quote");
		if (c == '\\') {				// escape char
			c = *token++;
			if (!c)
				error (0, "EOF inside quote");
			switch (c) {
				case '\\':
					c = '\\';
					break;
				case 'n':
					c = '\n';
					break;
				case '"':
					c = '\"';
					break;
				case '\'':
					c = '\'';
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					if (!options.qccx_escapes) {
						for (i = c = 0; i < 3
							 && *token >= '0'
							 && *token <= '7'; i++, token++) {
							c *= 8;
							c += *token - '0';
						}
						if (!*token)
							error (0, "EOF inside quote");
						break;
					}
				case '8':
				case '9':
					c = 18 + c - '0';
					break;
				case 'x':
					c = 0;
					while (*token && isxdigit ((unsigned char)*token)) {
						c *= 16;
						if (*token <= '9')
							c += *token - '0';
						else if (*token <= 'F')
							c += *token - 'A' + 10;
						else
							c += *token - 'a' + 10;
						token++;
					}
					if (!*token)
						error (0, "EOF inside quote");
					break;
				case 'a':
					c = '\a';
					break;
				case 'b':
					if (options.qccx_escapes)
						mask ^= 0x80;
					else
						c = '\b';
					break;
				case 'e':
					c = '\033';
					break;
				case 'f':
					c = '\f';
					break;
				case 'r':
					c = '\r';
					break;
				case 't':
					c = '\t';
					break;
				case 'v':
					c = '\v';
					break;
				case '^':
					if (*token == '\"')
						error (0, "Unexpected end of string after \\^");
					boldnext = 1;
					continue;
				case '[':
					c = 0x90;		// gold [
					break;
				case ']':
					c = 0x91;		// gold ]
					break;
				case '.':
					c = 28;			// center dot
					break;
				case '<':
					if (options.qccx_escapes)
						c = 29;			// brown left end
					else
						mask = 0x80;
					continue;
				case '-':
					c = 30;			// brown center bit
					break;
				case '>':
					if (options.qccx_escapes)
						c = 31;			// broun right end
					else
						mask = 0x00;
					continue;
				case '(':
					c = 128;		// left slider end
					break;
				case '=':
					c = 129;		// slider center
					break;
				case ')':
					c = 130;		// right slider end
					break;
				case '{':
					c = 0;
					while (*token && *token != '}'
						   && isdigit ((unsigned char)*token)) {
						c *= 10;
						c += *token++ - '0';
					}
					if (!*token)
						error (0, "EOF inside quote");
					if (*token != '}')
						error (0, "non-digit inside \\{}");
					else
						token++;
					if (c > 255)
						warning (0, "\\{%d} > 255", c);
					break;
				default:
					error (0, "Unknown escape char");
					break;
			}
		} else if (c == quote) {
			break;
		}
		if (boldnext)
			c = c ^ 0x80;
		boldnext = 0;
		c = c ^ mask;
		s[0] = c;
		dstring_appendstr (str, s);
	} while (1);

	if (end)
		*end = token;

	return save_string (str->str);
}

static srcline_t *free_srclines;

void
push_source_file (void)
{
	srcline_t  *srcline;
	ALLOC (16, srcline_t, srclines, srcline);
	srcline->source_file = pr.source_file;
	srcline->source_line = pr.source_line;
	srcline->next = pr.srcline_stack;
	pr.srcline_stack = srcline;
}

void
pop_source_file (void)
{
	srcline_t  *tmp;

	if (!pr.srcline_stack) {
		notice (0, "unbalanced #includes. bug in preprocessor?");
		return;
	}
	tmp = pr.srcline_stack;
	pr.srcline_stack = tmp->next;
	tmp->next = free_srclines;
	free_srclines = tmp;
}

void
line_info (char *text)
{
	char *p;
	char *s;
	const char *str;
	int line;
	int flags;

	p = text;
	line = strtol (p, &s, 10);
	p = s;
	while (isspace ((unsigned char)*p))
		p++;
	if (!*p)
		error (0, "Unexpected end of file");
	str = make_string (p, &s);		// grab the filename
	p = s;
	while (isspace ((unsigned char) *p))
		p++;
	flags = strtol (p, &s, 10);
	switch (flags) {
		case 1:
			push_source_file ();
			break;
		case 2:
			pop_source_file ();
			break;
	}
	while (*p && *p != '\n')	// ignore rest
		p++;
	pr.source_line = line - 1;
	pr.source_file = ReuseString (strip_path (str));
}

#ifdef YY_FLEX_REALLOC_HACK
static __attribute__ ((used)) void *(*const yy_flex_realloc_hack)(void *,yy_size_t) = yy_flex_realloc;
#else
static __attribute__ ((used)) void (*yyunput_hack)(int, char*) = yyunput;
static __attribute__ ((used)) int (*input_hack)(void) = input;
#endif
