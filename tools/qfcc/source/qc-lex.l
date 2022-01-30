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

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/grab.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/pragma.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

#ifndef YY_PROTO
# define YY_PROTO(x) x
#else
# define YY_FLEX_REALLOC_HACK
#endif

#define YY_NO_UNPUT
#define YY_DECL int yylex YY_PROTO(( void ))
YY_DECL;

int yyget_lineno (void) __attribute__((pure));
int yyget_leng (void) __attribute__((pure));
int yywrap (void) __attribute__((const));
char *yyget_text (void) __attribute__((pure));
int yyget_debug (void) __attribute__((pure));
FILE *yyget_in (void) __attribute__((pure));
FILE *yyget_out (void) __attribute__((pure));

static int keyword_or_id (char *token);

extern QC_YYSTYPE qc_yylval;

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
RANGE		\.\.
ELLIPSIS	\.\.\.
FRAMEID		{ID}(\.{ID})*
PRAGMAID	{ID}(-{ID})*
STRING		\"(\\.|[^"\\])*\"

%x		GRAB_FRAME GRAB_OTHER GRAB_WRITE COMMENT PRAGMA

%%
					grab_frame = GRAB_FRAME;
					grab_other = GRAB_OTHER;
					grab_write = GRAB_WRITE;

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

{INT}+[uU]?([lL][lL]?)?	{
						const char *c = yytext + yyleng - 1;
						int         i;

						if (yytext[0] == '0' && tolower (yytext[1] == 'b'))
							i = strtol (yytext + 2, 0, 2);
						else
							i = strtol (yytext, 0, 0);
						if (*c == 'u' || *c == 'U')
							qc_yylval.expr = new_int_expr (i);//FIXME
						else
							qc_yylval.expr = new_int_expr (i);
						return VALUE;
					}

{FLOAT}				{
						// advanced code defaults to double, but traditional
						// and extended code defaults to float
						if (options.traditional < 1) {
							double      d = strtod (yytext, 0);
							qc_yylval.expr = new_double_expr (d);
							qc_yylval.expr->implicit = 1;
						} else {
							float       f = strtof (yytext, 0);
							qc_yylval.expr = new_float_expr (f);
						}
						return VALUE;
					}
{FLOATf}			{
						float       f = strtof (yytext, 0);
						qc_yylval.expr = new_float_expr (f);
						return VALUE;
					}
{FLOATd}			{
						// advanced code defaults to double, but traditional
						// and extended code defaults to float
						if (options.traditional < 1) {
							double      d = strtod (yytext, 0);
							qc_yylval.expr = new_double_expr (d);
						} else {
							float       f = strtof (yytext, 0);
							qc_yylval.expr = new_float_expr (f);
							warning (0, "truncating double constant to float");
						}
						return VALUE;
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
						return VALUE;
					}

'{s}*{m}?{FLOAT}{s}+{m}?{FLOAT}{s}+{m}?{FLOAT}{s}+{m}?{FLOAT}{s}*'	{
						quat_t      q;
						sscanf (yytext, "' %f %f %f %f'",
								&q[0], &q[1], &q[2], &q[3]);
						qc_yylval.expr = new_quaternion_expr (q);
						return VALUE;
					}

'(\\[^xX0-7\r\n]|[^'\r\n]|\\[xX][0-9A-Fa-f]+|\\[0-7]+)*'	{
						const char *str = make_string (yytext, 0);

						if (str[1])
							warning (0, "multibyte char constant");
						qc_yylval.expr = new_int_expr (*str);
						return VALUE;
					}

[+\-*/&|^%]=		{
						qc_yylval.op = yytext[0];
						return ASX;
					}

"%%="				{
						qc_yylval.op = MOD;
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

"%%"				{
						qc_yylval.pointer = 0;	// ensure pointer vals are null
						return MOD;
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
							qc_yylval.expr = new_int_expr (ret);
							return VALUE;
						} else {
							BEGIN (-ret);
						}
					}

<GRAB_FRAME>{FRAMEID}	add_frame_macro (yytext);
<GRAB_OTHER>[^\r\n]*	/* skip */
<GRAB_WRITE>{STRING}	{
							const char *s = make_string (yytext, 0);
							write_frame_macros (s);
							BEGIN (GRAB_OTHER);	// ignore rest of line
						}
<PRAGMA>{ID}		{ pragma_add_arg (yytext); }

<*>\r*\n			{
						if (YY_START == PRAGMA) {
							pragma_process ();
						}
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
} keyword_t;

// These keywords are all part of the Ruamoko (Objective-QC) language.
// The first time any one of them is encountered, the class system will be
// initialized.
// If not compiling for the QuakeForge VM, or if Ruamoko has been disabled,
// then they will be unavailable as keywords.
static keyword_t obj_keywords[] = {
	{"id",				OBJECT,	&type_id		},
	{"Class",			TYPE,	&type_Class		},
	{"Method",			TYPE,	&type_method	},
	{"Super",			TYPE,	&type_super		},
	{"SEL",				TYPE,	&type_SEL		},
	{"IMP",				TYPE,	&type_IMP		},

	{"@class",			CLASS					},
	{"@defs",			DEFS					},
	{"@encode",			ENCODE					},
	{"@end",			END						},
	{"@implementation",	IMPLEMENTATION			},
	{"@interface",		INTERFACE				},
	{"@private",		PRIVATE					},
	{"@protected",		PROTECTED				},
	{"@protocol",		PROTOCOL				},
	{"@public",			PUBLIC					},
	{"@reference",		REFERENCE				},
	{"@selector",		SELECTOR				},
	{"@self",			SELF					},
	{"@this",			THIS					},

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
	{"for",			FOR		},
	{"goto",		GOTO	},
	{"break",		BREAK	},
	{"continue",	CONTINUE},
	{"switch",		SWITCH	},
	{"case",		CASE	},
	{"default",		DEFAULT	},
	{"nil",			NIL		},
	{"struct",		STRUCT	},
	{"union",		STRUCT	},
	{"enum",		ENUM	},
	{"typedef",		TYPEDEF	},
	{"extern",		EXTERN	},
	{"static",		STATIC	},
	{"sizeof",		SIZEOF	},
	{"nosave",		NOSAVE	},
	{"not",			NOT		},
};

// These keywords require the QuakeForge VM to be of any use. ie, they cannot
// be supported (sanely) by v6 progs.
static keyword_t qf_keywords[] = {
	{"quaternion",	TYPE,	&type_quaternion},
	{"double",		TYPE,	&type_double},
	{"int",			TYPE,	&type_int	},
	{"unsigned",	UNSIGNED,	0	},
	{"signed",		SIGNED,	0	},
	{"long",		LONG,	0	},
	{"short",		SHORT,	0	},

	{"@function",	TYPE,	&type_func	},
	{"@args",		ARGS,	0				},
	{"@va_list",	TYPE,	&type_va_list	},
	{"@param",		TYPE,	&type_param		},

	{"@cross",		CROSS,	0	},
	{"@dot",		DOT,	0	},
};

// These keywors are always available. Other than @system and @overload, they
// form traditional QuakeC.
static keyword_t keywords[] = {
	{"void",		TYPE,		&type_void	},
	{"float",		TYPE,		&type_float	},
	{"string",		TYPE,		&type_string},
	{"vector",		TYPE,		&type_vector},
	{"entity",		TYPE,		&type_entity},
	{"local",		LOCAL,		0			},
	{"return",		RETURN,		0			},
	{"while",		WHILE,		0			},
	{"do",			DO,			0			},
	{"if",			IF,			0			},
	{"else",		ELSE,		0			},
	{"@system",		SYSTEM,		0			},
	{"@overload",	OVERLOAD,	0			},
};

static const char *
keyword_get_key (const void *kw, void *unused)
{
	return ((keyword_t*)kw)->name;
}

static int
process_keyword (keyword_t *keyword, const char *token)
{
	if (keyword->value == STRUCT) {
		qc_yylval.op = token[0];
	} else if (keyword->value == OBJECT) {
		symbol_t   *sym;

		sym = symtab_lookup (current_symtab, token);
		qc_yylval.symbol = sym;
		// the global id symbol is always just a name so attempts to redefine
		// it globally can be caught and treated as an error, but it needs to
		// be redefinable when in an enclosing scope.
		if (sym->sy_type == sy_name) {
			// this is the global id (object)
			return OBJECT;
		} else if (sym->sy_type == sy_type) {
			// id has been redeclared via a typedef
			return TYPE_NAME;
		}
		// id has been redelcared as a variable (hopefully)
		return NAME;
	} else {
		qc_yylval.type = keyword->type;
	}
	return keyword->value;
}

static int
keyword_or_id (char *token)
{
	static hashtab_t *keyword_tab;
	static hashtab_t *qf_keyword_tab;
	static hashtab_t *at_keyword_tab;
	static hashtab_t *obj_keyword_tab;

	keyword_t  *keyword = 0;
	symbol_t   *sym;

	if (!keyword_tab) {
		size_t      i;

		keyword_tab = Hash_NewTable (253, keyword_get_key, 0, 0, 0);
		qf_keyword_tab = Hash_NewTable (253, keyword_get_key, 0, 0, 0);
		at_keyword_tab = Hash_NewTable (253, keyword_get_key, 0, 0, 0);
		obj_keyword_tab = Hash_NewTable (253, keyword_get_key, 0, 0, 0);

		#define NUMKEYS(_k) (sizeof (_k) / sizeof (_k[0]))

		for (i = 0; i < NUMKEYS(keywords); i++)
			Hash_Add (keyword_tab, &keywords[i]);
		for (i = 0; i < NUMKEYS(qf_keywords); i++)
			Hash_Add (qf_keyword_tab, &qf_keywords[i]);
		for (i = 0; i < NUMKEYS(at_keywords); i++)
			Hash_Add (at_keyword_tab, &at_keywords[i]);
		for (i = 0; i < NUMKEYS(obj_keywords); i++)
			Hash_Add (obj_keyword_tab, &obj_keywords[i]);
	}
	if (options.traditional < 1) {
		keyword = Hash_Find (obj_keyword_tab, token);
		if (keyword) {
			if (!obj_initialized)
				class_init ();
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
		return process_keyword (keyword, token);
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
#ifdef yyunput
static __attribute__ ((used)) void (*yyunput_hack)(int, char*) = yyunput;
#endif
static __attribute__ ((used)) int (*input_hack)(void) = input;
#endif
