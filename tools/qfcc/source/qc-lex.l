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

static int keyword_or_id (const char *token);
static expr_t *parse_float_vector (const char *token, int width);
static expr_t *parse_int_vector (const char *token, int width);

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
FCOMP		{m}?{FLOAT}
FD			[fFdD]
INT			({D}+|0[xX]{X}+|0[bB]{B})
ICOMP		{m}?{INT}
UL			([uU]?([lL][lL]?)?)
ULFD		({UL}|{FD})
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

{INT}+{UL}?			{
						const char *c = yytext + yyleng - 1;
						pr_long_t   i;

						if (yytext[0] == '0' && tolower (yytext[1] == 'b'))
							i = strtol (yytext + 2, 0, 2);
						else
							i = strtol (yytext, 0, 0);
						if (tolower (*c) == 'u') {
							if (tolower (c[1]) == 'l') {
								qc_yylval.expr = new_ulong_expr (i);
							} else {
								qc_yylval.expr = new_uint_expr (i);
							}
						} else {
							if (tolower (c[1]) == 'l') {
								qc_yylval.expr = new_long_expr (i);
							} else {
								qc_yylval.expr = new_int_expr (i);
								qc_yylval.expr->implicit = 1;
							}
						}
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

'{s}*{ICOMP}{s}+{ICOMP}{s}*'{ULFD}?	{
						qc_yylval.expr = parse_int_vector (yytext, 2);
						return VALUE;
					}

'{s}*{ICOMP}{s}+{ICOMP}{s}+{ICOMP}{s}*'{ULFD}?	{
						qc_yylval.expr = parse_int_vector (yytext, 3);
						return VALUE;
					}

'{s}*{ICOMP}{s}+{ICOMP}{s}+{ICOMP}{s}+{ICOMP}{s}*'{ULFD}?	{
						qc_yylval.expr = parse_int_vector (yytext, 4);
						return VALUE;
					}

'{s}*{FCOMP}{s}+{FCOMP}{s}*'{FD}?	{
						qc_yylval.expr = parse_float_vector (yytext, 2);
						return VALUE;
					}

'{s}*{FCOMP}{s}+{FCOMP}{s}+{FCOMP}{s}*'{FD}?	{
						qc_yylval.expr = parse_float_vector (yytext, 3);
						return VALUE;
					}

'{s}*{FCOMP}{s}+{FCOMP}{s}+{FCOMP}{s}+{FCOMP}{s}*'{FD}?	{
						qc_yylval.expr = parse_float_vector (yytext, 4);
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
<PRAGMA>@{ID}		{ pragma_add_arg (yytext); }

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
	specifier_t spec;
} keyword_t;

// These keywords are part of the Ruamoko language and require the QuakeForge
// Ruamoko VM.
static keyword_t rua_keywords[] = {
#define VEC_TYPE(type_name, base_type) \
	{ #type_name, TYPE_SPEC, .spec = { .type = &type_##type_name } },
#include "tools/qfcc/include/vec_types.h"
};

// These keywords are all part of the Ruamoko (Objective-QC) language.
// The first time any one of them is encountered, the class system will be
// initialized.
// If not compiling for the QuakeForge VM, or if Ruamoko has been disabled,
// then they will be unavailable as keywords.
static keyword_t obj_keywords[] = {
	{"id",				OBJECT_NAME, .spec = { .type = &type_id } 		},
	{"Class",			TYPE_SPEC, .spec = { .type = &type_Class } 		},
	{"Method",			TYPE_SPEC, .spec = { .type = &type_method } 	},
	{"Super",			TYPE_SPEC, .spec = { .type = &type_super } 		},
	{"SEL",				TYPE_SPEC, .spec = { .type = &type_SEL } 		},
	{"IMP",				TYPE_SPEC, .spec = { .type = &type_IMP } 		},

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
	{"not",			NOT		},
};

// These keywords require the QuakeForge VM to be of any use. ie, they cannot
// be supported (sanely) by v6 progs.
static keyword_t qf_keywords[] = {
	{"quaternion",	TYPE_SPEC, .spec = { .type = &type_quaternion } },
	{"double",		TYPE_SPEC, .spec = { .type = &type_double } },
	{"int",			TYPE_SPEC, .spec = { .type = &type_int } 	},
	{"unsigned",	TYPE_SPEC, .spec = { .is_unsigned = 1 } },
	{"signed",		TYPE_SPEC, .spec = { .is_signed = 1 } },
	{"long",		TYPE_SPEC, .spec = { .is_long = 1 } },
	{"short",		TYPE_SPEC, .spec = { .is_short = 1 } },

	{"@function",	TYPE_SPEC, .spec = { .type = &type_func } 	},
	{"@args",		ARGS,					},
	{"@va_list",	TYPE_SPEC, .spec = { .type = &type_va_list } 	},
	{"@param",		TYPE_SPEC, .spec = { .type = &type_param } 		},
	{"@return",     AT_RETURN,		},

	{"@cross",		CROSS,		},
	{"@dot",		DOT,		},
	{"@hadamard",	HADAMARD,		},
};

// These keywors are always available. Other than the @ keywords, they
// form traditional QuakeC.
static keyword_t keywords[] = {
	{"void",		TYPE_SPEC, .spec = { .type = &type_void } 	},
	{"float",		TYPE_SPEC, .spec = { .type = &type_float } 	},
	{"string",		TYPE_SPEC, .spec = { .type = &type_string } },
	{"vector",		TYPE_SPEC, .spec = { .type = &type_vector } },
	{"entity",		TYPE_SPEC, .spec = { .type = &type_entity } },
	{"local",		LOCAL,					},
	{"return",		RETURN,					},
	{"while",		WHILE,					},
	{"do",			DO,						},
	{"if",			IF,						},
	{"else",		ELSE,					},
	{"@system",		SYSTEM,					},
	{"@overload",	OVERLOAD,				},
	{"@attribute",  ATTRIBUTE,				},
	{"@handle",     HANDLE,					},
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
	} else if (keyword->value == OBJECT_NAME) {
		symbol_t   *sym;

		sym = symtab_lookup (current_symtab, token);
		qc_yylval.symbol = sym;
		// the global id symbol is always just a name so attempts to redefine
		// it globally can be caught and treated as an error, but it needs to
		// be redefinable when in an enclosing scope.
		if (sym->sy_type == sy_name) {
			// this is the global id (object)
			qc_yylval.spec = (specifier_t) {
				.type = sym->type,
				.sym = sym,
			};
			return OBJECT_NAME;
		} else if (sym->sy_type == sy_type) {
			// id has been redeclared via a typedef
			qc_yylval.spec = (specifier_t) {
				.type = sym->type,
				.sym = sym,
			};
			return TYPE_NAME;
		}
		// id has been redelcared as a variable (hopefully)
		return NAME;
	} else {
		qc_yylval.spec = keyword->spec;
	}
	return keyword->value;
}

static int
keyword_or_id (const char *token)
{
	static hashtab_t *keyword_tab;
	static hashtab_t *qf_keyword_tab;
	static hashtab_t *at_keyword_tab;
	static hashtab_t *obj_keyword_tab;
	static hashtab_t *rua_keyword_tab;

	keyword_t  *keyword = 0;
	symbol_t   *sym;

	if (!keyword_tab) {
		size_t      i;

		keyword_tab = Hash_NewTable (253, keyword_get_key, 0, 0, 0);
		qf_keyword_tab = Hash_NewTable (253, keyword_get_key, 0, 0, 0);
		at_keyword_tab = Hash_NewTable (253, keyword_get_key, 0, 0, 0);
		obj_keyword_tab = Hash_NewTable (253, keyword_get_key, 0, 0, 0);
		rua_keyword_tab = Hash_NewTable (253, keyword_get_key, 0, 0, 0);

		#define NUMKEYS(_k) (sizeof (_k) / sizeof (_k[0]))

		for (i = 0; i < NUMKEYS(keywords); i++)
			Hash_Add (keyword_tab, &keywords[i]);
		for (i = 0; i < NUMKEYS(qf_keywords); i++)
			Hash_Add (qf_keyword_tab, &qf_keywords[i]);
		for (i = 0; i < NUMKEYS(at_keywords); i++)
			Hash_Add (at_keyword_tab, &at_keywords[i]);
		for (i = 0; i < NUMKEYS(obj_keywords); i++)
			Hash_Add (obj_keyword_tab, &obj_keywords[i]);
		for (i = 0; i < NUMKEYS(rua_keywords); i++)
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
		return process_keyword (keyword, token);
	if (token[0] == '@') {
		return '@';
	}
	sym = symtab_lookup (current_symtab, token);
	if (!sym)
		sym = new_symbol (token);
	qc_yylval.symbol = sym;
	if (sym->sy_type == sy_type) {
		qc_yylval.spec = (specifier_t) {
			.type = sym->type,
			.sym = sym,
		};
		return TYPE_NAME;
	}
	if (sym->sy_type == sy_class)
		return CLASS_NAME;
	return NAME;
}

static expr_t *
parse_int_vector (const char *token, int width)
{
	char        t1 = 0, t2 = 0;
	type_t     *type = 0;

	union {
		pr_long_t   l[4];
		pr_type_t   t[PR_SIZEOF (dvec4)];
	}           long_data = {};
	pr_type_t  *data = __builtin_choose_expr (
		sizeof (pr_long_t) == sizeof (int64_t), long_data.t, (void) 0);

	switch (width) {
		case 4:
			sscanf (token, "' %"SCNi64" %"SCNi64" %"SCNi64" %"SCNi64" '%c%c",
					&long_data.l[0], &long_data.l[1],
					&long_data.l[2], &long_data.l[3], &t1, &t2);
			break;
		case 3:
			sscanf (token, "' %"SCNi64" %"SCNi64" %"SCNi64" '%c%c",
					&long_data.l[0], &long_data.l[1],
					&long_data.l[2], &t1, &t2);
			break;
		case 2:
			sscanf (token, "' %"SCNi64" %"SCNi64" '%c%c",
					&long_data.l[0], &long_data.l[1], &t1, &t2);
			break;
	}
	t1 = tolower (t1);
	t2 = tolower (t2);
	if (options.code.progsversion < PROG_VERSION) {
		if (!t1) {
			t1 = 'f';
		}
	}
	expr_t     *expr = 0;
	switch (t1) {
		case 'u':
			if (t2 == 'l') {
				type = &type_ulong;
				type = vector_type (type, width);
				expr = new_value_expr (new_type_value (type, data));
			} else {
				type = &type_uint;
				union {
					pr_uint_t   u[4];
					pr_type_t   t[PR_SIZEOF (ivec4)];
				}           uint_data = {
					.u = {
						long_data.l[0],
						long_data.l[1],
						long_data.l[2],
						long_data.l[3],
					}
				};
				data = uint_data.t;
				type = vector_type (type, width);
				expr = new_value_expr (new_type_value (type, data));
			}
			break;
		case 'l':
			type = &type_long;
			type = vector_type (type, width);
			expr = new_value_expr (new_type_value (type, data));
			break;
		case 'f':
			type = &type_float;
			union {
				pr_float_t  f[4];
				pr_type_t   t[PR_SIZEOF (vec4)];
			}           float_data = {
				.f = {
					long_data.l[0],
					long_data.l[1],
					long_data.l[2],
					long_data.l[3],
				}
			};
			data = float_data.t;
			type = vector_type (type, width);
			expr = new_value_expr (new_type_value (type, data));
			break;
		case 'd':
			type = &type_double;
			union {
				pr_double_t d[4];
				pr_type_t   t[PR_SIZEOF (dvec4)];
			}           double_data = {
				.d = {
					long_data.l[0],
					long_data.l[1],
					long_data.l[2],
					long_data.l[3],
				}
			};
			data = double_data.t;
			type = vector_type (type, width);
			expr = new_value_expr (new_type_value (type, data));
			break;
		case 0:
			type = &type_int;
			union {
				pr_int_t    i[4];
				pr_type_t   t[PR_SIZEOF (ivec4)];
			}           int_data = {
				.i = {
					long_data.l[0],
					long_data.l[1],
					long_data.l[2],
					long_data.l[3],
				}
			};
			data = int_data.t;
			type = vector_type (type, width);
			expr = new_value_expr (new_type_value (type, data));
			break;
	}
	expr->implicit = !t1;
	return expr;
}

static expr_t *
parse_float_vector (const char *token, int width)
{
	char        t = 0;
	type_t     *type = 0;

	union {
		pr_double_t d[4];
		pr_type_t   t[PR_SIZEOF (dvec4)];
	}           double_data = {};
	pr_type_t  *data = __builtin_choose_expr (
		sizeof (pr_double_t) == sizeof (double), double_data.t, (void) 0);

	switch (width) {
		case 4:
			sscanf (token, "' %lf %lf %lf %lf '%c",
					&double_data.d[0], &double_data.d[1],
					&double_data.d[2], &double_data.d[3], &t);
			break;
		case 3:
			sscanf (token, "' %lf %lf %lf '%c",
					&double_data.d[0], &double_data.d[1],
					&double_data.d[1], &t);
			type = (t == 'f' || t == 'F') ? &type_vec3 : &type_dvec3;
			break;
		case 2:
			sscanf (token, "' %lf %lf '%c",
					&double_data.d[0], &double_data.d[1], &t);
			type = (t == 'f' || t == 'F') ? &type_vec2 : &type_dvec2;
			break;
	}
	if (options.code.progsversion < PROG_VERSION) {
		if (!t) {
			t = 'f';
		}
	}
	if (t == 'f' || t == 'F') {
		volatile union {
			pr_float_t  f[4];
			pr_type_t   t[PR_SIZEOF (vec4)];
		}           float_data = {
			.f = {
				double_data.d[0],
				double_data.d[1],
				double_data.d[2],
				double_data.d[3],
			}
		};
		data = (pr_type_t *) float_data.t;
		type = &type_float;
	} else {
		type = &type_double;
	}
	type = vector_type (type, width);
	expr_t     *expr = new_value_expr (new_type_value (type, data));
	expr->implicit = !t;
	return expr;
}

#ifdef YY_FLEX_REALLOC_HACK
static __attribute__ ((used)) void *(*const yy_flex_realloc_hack)(void *,yy_size_t) = yy_flex_realloc;
#else
#ifdef yyunput
static __attribute__ ((used)) void (*yyunput_hack)(int, char*) = yyunput;
#endif
static __attribute__ ((used)) int (*input_hack)(void) = input;
#endif
