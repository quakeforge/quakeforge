%{
/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
static const char rcsid[] = 
	"$Id$";

#include <QF/hash.h>
#include <QF/sys.h>
#include "qfcc.h"
#include "class.h"
#include "struct.h"
#include "type.h"
#include "qc-parse.h"

#define YY_NO_UNPUT

int type_or_name (char *token);

int do_grab (char *token);

void add_frame_macro (char *token);

char *make_string (char *token);

extern YYSTYPE yylval;
extern int element_flag;

%}

DIGIT	[0-9]
ID		[a-zA-Z_][a-zA-Z_0-9]*
FLOAT	{DIGIT}+"."{DIGIT}*
NUM		({DIGIT}+("."{DIGIT}*)?)
s		[ \t]
m		([\-+]?)

%x		grab_frame grab_other

%%

"/*"				{
						int c;
						do {
							while ((c = input ()) != '*' && c != EOF
								   && c != '\n')
								;
							while (c == '*')
								c = input ();
							if (c == EOF)
								error (0, "EOF in comment");
							if (c == '\n')
								pr_source_line++;
						} while (c != '/' && c != EOF);
					}

"//".*	/* nothing to do */

{DIGIT}+			{
						yylval.integer_val = atoi (yytext);
						return INT_VAL;
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

\"(\\.|[^"])*\"		{
						yylval.string_val = make_string (yytext);
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
								&yylval.vector_val[0], &yylval.vector_val[1],
								&yylval.vector_val[2], &yylval.vector_val[3]);
						return QUATERNION_VAL;
					}

'(\\[^xX0-7\r\n]|[^'\r\n]|\\[xX][0-9A-Fa-f]+|\\[0-7]+)*'	{
						char *str = make_string (yytext);

						if (str[1])
							warning (0, "multibyte char constant");
						yylval.integer_val = *str;
						free (str);
						return INT_VAL;
					}

^#{s}+{DIGIT}+{s}+\"(\.|[^"\n])*\".*$ {
						char *p;
						char *s;
						int line;
						
						p = yytext + 1;
						line = strtol (p, &s, 10);
						p = s;
						while (isspace ((unsigned char)*p))
							p++;
						if (!*p)
							error (0, "Unexpected end of file");
						s = make_string (p);		// grab the filename
						while (*p && *p != '\n')	// ignore flags
							p++;
						pr_source_line = line - 1;
						s_file = ReuseString (strip_path (s));
						free (s);
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
						if (yytext[0] == '{' && element_flag) {
							element_flag = 0;
							return ELE_START;
						}
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

"$"{s}*{ID}			{
						int ret = do_grab(yytext);
						if (ret > 0)
							return ret;
						else
							BEGIN (-ret);
					}

<grab_frame>{ID}	add_frame_macro (yytext);

<grab_other>[^\r\n]*	/* skip */

<*>\r*\n			{
						pr_source_line++;
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
	int			traditional;
	int			version;
} keyword_t;

static keyword_t keywords[] = {
	{"void",		TYPE,	&type_void,			1, PROG_ID_VERSION},
	{"float",		TYPE,	&type_float,		1, PROG_ID_VERSION},
	{"string",		TYPE,	&type_string,		1, PROG_ID_VERSION},
	{"vector",		TYPE,	&type_vector,		1, PROG_ID_VERSION},
	{"entity",		TYPE,	&type_entity,		1, PROG_ID_VERSION},
	{"quaternion",	TYPE,	&type_quaternion,	0, PROG_VERSION},
	{"integer",		TYPE,	&type_integer,		0, PROG_VERSION},
	{"function",	TYPE,	&type_function,		0, PROG_VERSION},
	{"id",			TYPE,	&type_id,			0, PROG_VERSION},
	{"Class",		TYPE,	&type_Class,		0, PROG_VERSION},
	{"Protocol",	TYPE,	&type_Protocol,		0, PROG_VERSION},
	{"Method",		TYPE,	&type_Method,		0, PROG_VERSION},
	{"SEL",			TYPE,	&type_SEL,			0, PROG_VERSION},
	{"IMP",			TYPE,	&type_IMP,			0, PROG_VERSION},
	{"local",		LOCAL,	0,					1, PROG_ID_VERSION},
	{"return",		RETURN,	0,					1, PROG_ID_VERSION},
	{"while",		WHILE,	0,					1, PROG_ID_VERSION},
	{"do",			DO,		0,					1, PROG_ID_VERSION},
	{"if",			IF,		0,					1, PROG_ID_VERSION},
	{"else",		ELSE,	0,					1, PROG_ID_VERSION},
	{"for",			FOR,	0,					0, PROG_ID_VERSION},
	{"break",		BREAK,	0,					0, PROG_ID_VERSION},
	{"continue",	CONTINUE, 0,				0, PROG_ID_VERSION},
	{"switch",		SWITCH,	0,					0, PROG_ID_VERSION},
	{"case",		CASE,	0,					0, PROG_ID_VERSION},
	{"default",		DEFAULT, 0,					0, PROG_ID_VERSION},
	{"NIL",			NIL,	0,					0, PROG_ID_VERSION},
	{"struct",		STRUCT, 0,					0, PROG_VERSION},
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
	{"@selector",	SELECTOR, 0,				0, PROG_VERSION},
	{"@self",		SELF,	0,					0, PROG_VERSION},
	{"@this",		THIS,	0,					0, PROG_VERSION},
	{"@argc",		ARGC,	0,					0, PROG_VERSION},
	{"@argv",		ARGV,	0,					0, PROG_VERSION},
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
	type_t     *type;

	if (!keyword_tab) {
		int i;
		keyword_tab = Hash_NewTable (1021, keyword_get_key, 0, 0);
		for (i = 0; i < sizeof (keywords) / sizeof (keywords[0]); i++)
			if (keywords[i].traditional >= options.traditional
				&& keywords[i].version <= options.code.progsversion)
				Hash_Add (keyword_tab, &keywords[i]);
	}
	keyword = Hash_Find (keyword_tab, token);
	if (keyword) {
		if (token[0] == '@' && !class_Class.super_class)
			class_init ();
		yylval.type = keyword->type;
		return keyword->value;
	}
	if (token[0] == '@') {
		return '@';
	}
	if ((type = find_struct (token)) || (type = get_typedef (token))) {
		yylval.type = type;
		return TYPE;
	}
	yylval.string_val = strdup (token);
	return NAME;
}

static hashtab_t *frame_tab;
static hashtab_t *grab_tab;

typedef struct {
	const char *name;
	int			num;
} frame_t;

static frame_t grab_list[] = {
	{"cd",		0},
	{"origin",	0},
	{"base",	0},
	{"flags",	0},
	{"scale",	0},
	{"skin",	0},
};

static const char *
frame_get_key (void *f, void *unused)
{
	return ((frame_t*)f)->name;
}

static void
frame_free (void *f, void *unused)
{
	free ((char*)((frame_t*)f)->name);
	free (f);
}

int
do_grab (char *token)
{
	static int initialized;
	frame_t *frame;

	if (!initialized) {
		int i;

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
	frame_t *frame = malloc (sizeof (frame_t));
	SYS_CHECKMEM (frame);

	frame->name = strdup (token);
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

char *
make_string (char *token)
{
	char       *str, *s;
	int         c;
	int         i;
	int         mask;
	int         boldnext;
	int         quote;

	s = str = malloc (strlen (token) + 1);
	SYS_CHECKMEM (str);

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
					for (i = c = 0; i < 3
						 && *token >= '0'
						 && *token <= '7'; i++, token++) {
						c *= 8;
						c += *token - '0';
					}
					if (!*token)
						error (0, "EOF inside quote");
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
					//XXX mask ^= 0x80;
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
					c = 0x90;
					break;
				case ']':
					c = 0x91;
					break;
				case '.':
					c = 28;
					break;
				case '<':
					//XXX c = 29;
					mask = 0x80;
					continue;
				case '-':
					c = 30;
					break;
				case '>':
					//XXX c = 31;
					mask = 0x00;
					continue;
				case '(':
					c = 128;
					break;
				case '=':
					c = 129;
					break;
				case ')':
					c = 130;
					break;
				case '{':
					c = 0;
					while (*token && *token != '}'
						   && isdigit ((unsigned char)*token)) {
						c *= 10;
						c += *token - '0';
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
			*s++ = 0;
			break;;
		}
		if (boldnext)
			c = c ^ 0x80;
		boldnext = 0;
		c = c ^ mask;
		*s++ = c;
	} while (1);

	return str;
}

static void *(*const yy_flex_realloc_hack)(void *,yy_size_t) = yy_flex_realloc;
