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
%option bison-locations
%option bison-bridge
%option reentrant
%option prefix="qc_yy"
%option noyywrap
%option debug
%option stack

%top{
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
#include <QF/va.h>

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/grab.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/pragma.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"
}

%{
#ifndef YY_PROTO
# define YY_PROTO(x) x
#else
# define YY_FLEX_REALLOC_HACK
#endif

#define YY_NO_INPUT	// debian flex
#define YY_NO_UNPUT	// debian flex
#define YY_NO_YYINPUT
#define YY_NO_YYUNPUT

#define YYSTYPE rua_tok_t
#define YYLTYPE rua_loc_t

typedef struct {
	bool        saw_true;
	bool        saw_else;
	bool        own_state;
	bool        enabled;
} rua_cond_t;

typedef struct DARRAY_TYPE (rua_cond_t) rua_cond_stack_t;
typedef struct DARRAY_TYPE (rua_expr_t) rua_expr_stack_t;

typedef struct rua_extra_s {
		int         start_state;
		bool        preprocessor;
		bool        recording;
		bool        params;
		bool        expand;
		qc_yypstate *qc_state;
		pre_yypstate *pre_state;
		rua_cond_stack_t cond_stack;
		rua_expr_stack_t expr_stack;
		symtab_t   *macro_tab;
		yyscan_t    subscanner;
	} rua_extra_t;
#define YY_EXTRA_TYPE rua_extra_t *

rua_extra_t *qc_yyget_extra (yyscan_t yyscanner) __attribute__((pure));
int yyget_lineno (yyscan_t yyscanner) __attribute__((pure));
int yyget_column (yyscan_t yyscanner) __attribute__((pure));
YYSTYPE *yyget_lval (yyscan_t yyscanner) __attribute__((pure));
YYLTYPE *yyget_lloc (yyscan_t yyscanner) __attribute__((pure));
int yyget_leng (yyscan_t yyscanner) __attribute__((pure));
char *yyget_text (yyscan_t yyscanner) __attribute__((pure));
int yyget_debug (yyscan_t yyscanner) __attribute__((pure));
FILE *yyget_in (yyscan_t yyscanner) __attribute__((pure));
FILE *yyget_out (yyscan_t yyscanner) __attribute__((pure));

static int directive (const char *token, yyscan_t scanner);
static int keyword_or_id (YYSTYPE *lval, const char *token);
static void user_action (rua_tok_t *tok, rua_loc_t *loc,
						 const char *text, size_t textlen, int state);
static void undo_loc (rua_loc_t *loc);
static void next_line (rua_loc_t *loc);

typedef enum {
	rua_eof = 1,
	rua_ignore,
	rua_error,
	rua_id,
	rua_number,
	rua_vector,
	rua_string,
	rua_char,
	rua_space,

	rua_num_term,
} rua_term;

#define YY_USER_ACTION user_action (yylval, yylloc, yytext, yyleng, yystart());

%}

s			[ \t]
m			[\-+]
D			[0-9]
IDs			[a-zA-Z_]
IDc			[a-zA-Z_0-9]
ID			{IDs}{IDc}*
FD			[fFdD]
UL			([uU]?([lL][lL]?)?)
ULFD		({UL}|{FD})
RANGE		\.\.
ELLIPSIS	\.\.\.
FRAMEID		{ID}(\.{ID})*
PRAGMAID	{ID}(-{ID})*
s_string	\"(\\.|[^"\\])*\"
c_string	'(\\.|[^'\\])*'
h_string    <[^>]*>
q_string    \"[^\"]*\"

pp_number	\.?{D}({IDc}|'{IDc}|[eEpP]{m}|\.)*
pp_vnumber	'({s}*{m}?{pp_number}){2,4}{s}*'{ULFD}?

%x		GRAB_FRAME GRAB_OTHER GRAB_WRITE
%x		COMMENT LCOMMENT
%x		BOL DIRECTIVE TEXT SUPPRESS SUPPRESSC PRAGMA
%x      VECTOR
%s		PREPROC PREEXPR MACRO

%%
						auto extra = qc_yyget_extra (yyscanner);
						yyset_debug (0, yyscanner);
						grab_frame = GRAB_FRAME;
						grab_other = GRAB_OTHER;
						grab_write = GRAB_WRITE;
						yylval->pointer = 0;	// ensure pointer vals are null

<COMMENT>"/*"			{ warning (0, "nested /* in comment"); }
<COMMENT>\*+"/"			{ yy_pop_state (yyscanner); return -rua_space; }
<COMMENT>\n				{ next_line (yylloc); }
<COMMENT>[^*/\n]*		/* munch on anything but possible end of comment */
<COMMENT>\/+[^*\n]*		/* handle /s not followed by * */
<COMMENT>\*+[^/\n]*		/* handle *s not followed by * */
<COMMENT><<EOF>>		{ error (0, "EOF in comment"); return 0; }
<*>"/*"					{ yy_push_state (COMMENT, yyscanner); }
<*>"//"					{ yy_push_state (LCOMMENT, yyscanner); }/* cf <*>\r\n */
<LCOMMENT>[^\\\r\n]*	/* consume all but \ and EOL (see line continuation) */
<LCOMMENT>[\\]*			/* consume \ */

<*>^#{s}*				{
							yy_push_state (DIRECTIVE, yyscanner);
							extra->preprocessor = true;
						}
<*>^{s}*				{ yy_push_state (BOL, yyscanner); return -rua_space; }
<BOL>#{s}*				{ BEGIN (DIRECTIVE); extra->preprocessor = true; }
<BOL>.					{
							yy_pop_state (yyscanner);
							undo_loc (yylloc);
							yyless (0);
						}

<DIRECTIVE>{ID}			{ {
							int tok = directive (yytext, yyscanner);
							if (tok >= 0) {
								return tok;
							}
						} }
<PREPROC,MACRO>{ID}		{ return PRE_ID; }
<PREPROC>{ID}\(			{ return PRE_IDp; }

<DIRECTIVE>\r*\n		|
<PREEXPR>\r*\n			|
<MACRO,TEXT>\r*\n		|
<PREPROC>\r*\n			{ next_line (yylloc); return PRE_EOD; }

<SUPPRESS>^[^#/\n \t]+	{ BEGIN (SUPPRESSC); }
<SUPPRESSC>[^/\n]*		/* nom nom */
<SUPPRESSC>\/[^/\n]*	/* nom nom */
<SUPPRESSC>\n			{ next_line (yylloc); BEGIN (SUPPRESS); }
<SUPPRESS>\n			{ next_line (yylloc); }
<SUPPRESS>\/[^/\n]*		/* nom nom */
<SUPPRESS><<EOF>>		{ error (0, "unterminated #if"); return 0; }

<TEXT>[^\\\r\n]*		{ return PRE_TEXT; }
<TEXT>[\\]*				{ return PRE_TEXT; }
<MACRO>##				{ return PRE_CONCAT; }
<MACRO>#				{ return '#'; }
<PREEXPR>{h_string}		{ return PRE_HSTRING; }
<PREEXPR>{q_string}		{ return PRE_QSTRING; }
<PREEXPR>defined		{ return PRE_DEFINED; }
<PREEXPR>{ID}			{ return PRE_ID; }

^#{s}+{D}+{s}+\"(\.|[^"\n])*\".*$ { line_info (yytext + 1); }
^#line{s}+{D}+{s}+\"(\.|[^"\n])*\".*$ { line_info (yytext + 5); }

^{s}*#{s}*pragma{s}+	{ yy_push_state (PRAGMA, yyscanner); }

{ID}					|
@{ID}					{ return -rua_id; }
@						{ return '@'; }

{pp_number}				{ return -rua_number; }
{pp_vnumber}			{ return -rua_vector; }
<VECTOR>{pp_number}		{ return -rua_number; }
<VECTOR>{m}				{ return yytext[0]; }
<VECTOR>'				{ return -rua_eof; }

{s_string}				{ return -rua_string; };
{c_string}				{ return -rua_char; }

[+\-*/&|^%]=			{ yylval->value.op = yytext[0]; return ASX; }

"%%="					{ yylval->value.op = MOD; return ASX; }
"<<="					{ yylval->value.op = SHL; return ASX; }
">>="					{ yylval->value.op = SHR; return ASX; }

[!(){}.*/&|^~+\-=\[\];,#%?:] { return yytext[0]; }

"·"						{ return DOT; }
"⋀"						{ return WEDGE; }
"•"						{ return DOT; }
"∧"						{ return WEDGE; }
"∨"						{ return REGRESSIVE; }
"†"						{ return REVERSE; }
"∗"						{ return STAR; }
"×"						{ return CROSS; }
"⋆"						{ return DUAL; }

"%%"					{ return MOD; }

{ELLIPSIS}				{ return ELLIPSIS; }

"<<"					{ return SHL; }
">>"					{ return SHR; }

"&&"					{ return AND; }
"||"					{ return OR; }
"=="					{ return EQ; }
"!="					{ return NE; }
"<="					{ return LE; }
">="					{ return GE; }
"<"						{ return LT; }
">"						{ return GT; }

"++"					{ yylval->value.op = '+'; return INCOP; }
"--"					{ yylval->value.op = '-'; return INCOP; }

"$"{s}*{FRAMEID}		{
							int ret = do_grab (yytext);
							if (ret >= 0) {
								yylval->value.expr = new_int_expr (ret, false);
								return VALUE;
							} else {
								yy_push_state (-ret, yyscanner);
							}
						}

<GRAB_FRAME>{FRAMEID}	{ add_frame_macro (yytext); }
<GRAB_OTHER>[^\r\n]*	/* skip */
<GRAB_WRITE>{s_string}	{
							const char *s = make_string (yytext, 0);
							write_frame_macros (s);
							BEGIN (GRAB_OTHER);	// ignore rest of line
						}
<PRAGMA>{PRAGMAID}		{ pragma_add_arg (yytext); }
<PRAGMA>@{PRAGMAID}		{ pragma_add_arg (yytext); }

<*>\\\r*\n				{ next_line (yylloc); }	/* line continuation */
<*>\r*\n				{
							if (YY_START == PRAGMA) {
								pragma_process ();
							}
							next_line (yylloc);
							if (yyg->yy_start_stack_ptr) {
								yy_pop_state (yyscanner);
							}
						}

<*>{s}+					{ return -rua_space; }

<*>.					{
							error (0, "all your typo are belong to us:%d %d-%d",
								   yystart(),
								   yylloc->first_column, yylloc->last_column);
						}

%%

#define ARRCOUNT(_k) (sizeof (_k) / sizeof (_k[0]))

typedef struct {
	const char *name;
	int         value;
	specifier_t spec;
} keyword_t;

typedef struct {
	const char *name;
	int         value;
} directive_t;

// preprocessor directives in ruamoko and quakec
static directive_t rua_directives[] = {
	{"include",  PRE_INCLUDE},
	{"embed",    PRE_EMBED},
	{"define",   PRE_DEFINE},
	{"undef",    PRE_UNDEF},
	{"error",    PRE_ERROR},
	{"warning",  PRE_WARNING},
	{"pragma",   PRE_PRAGMA},
	{"line",     PRE_LINE},
};

static directive_t cond_directives[] = {
	{"if",       PRE_IF},
	{"ifdef",    PRE_IFDEF},
	{"ifndef",   PRE_IFNDEF},
	{"else",     PRE_ELSE},
	{"elif",     PRE_ELIF},
	{"elifdef",  PRE_ELIFDEF},
	{"elifndef", PRE_ELIFNDEF},
	{"endif",    PRE_ENDIF},
};

static const char *
directive_get_key (const void *dir, void *unused)
{
	return ((directive_t*)dir)->name;
}

static int
directive (const char *token, yyscan_t scanner)
{
	static hashtab_t *rua_directive_tab;
	static hashtab_t *cond_directive_tab;

	yy_pop_state (scanner); // pop DIRECTIVE off the stack
	yy_push_state (PREPROC, scanner);

	if (!rua_directive_tab) {
		rua_directive_tab = Hash_NewTable (253, directive_get_key, 0, 0, 0);
		cond_directive_tab = Hash_NewTable (253, directive_get_key, 0, 0, 0);

		for (size_t i = 0; i < ARRCOUNT(rua_directives); i++) {
			Hash_Add (rua_directive_tab, &rua_directives[i]);
		}
		for (size_t i = 0; i < ARRCOUNT(cond_directives); i++) {
			Hash_Add (cond_directive_tab, &cond_directives[i]);
		}
	}
	directive_t *directive = Hash_Find (cond_directive_tab, token);
	if (!directive && yy_top_state (scanner) != SUPPRESS) {
		directive = Hash_Find (rua_directive_tab, token);
		if (!directive) {
			error (0, "invalid directive `%s`", token);
			yy_pop_state (scanner);
			yy_push_state (LCOMMENT, scanner);
		}
	}
	if (!directive) {
		if (yy_top_state (scanner) == SUPPRESS) {
			yy_pop_state (scanner);// suppressed non-conditional directive
			yy_push_state (LCOMMENT, scanner);
		}
		return -1;
	}
	//auto lex = qc_yyget_extra (scanner);
	//lex->current_lang = &lex->pre_lang;
	return directive->value;
}

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
	{"auto",		TYPE_SPEC, .spec = { .type = &type_auto } },
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

	{"@hadamard",	HADAMARD,	},
	{"@cross",		CROSS,		},
	{"@dot",		DOT,		},
	{"@wedge",		WEDGE,		},
	{"@regressive",	REGRESSIVE,	},
	{"@geometric",	GEOMETRIC,	},
	{"@algebra",	ALGEBRA,	},
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
process_keyword (YYSTYPE *lval, keyword_t *keyword, const char *token)
{
	if (lval->text == lval->str_text) {
		lval->text = save_string (lval->str_text);
	}
	if (keyword->value == STRUCT) {
		lval->value.op = token[0];
	} else if (keyword->value == OBJECT_NAME) {
		symbol_t   *sym;

		sym = symtab_lookup (current_symtab, token);
		lval->value.symbol = sym;
		// the global id symbol is always just a name so attempts to redefine
		// it globally can be caught and treated as an error, but it needs to
		// be redefinable when in an enclosing scope.
		if (sym->sy_type == sy_name) {
			// this is the global id (object)
			lval->value.spec = (specifier_t) {
				.type = sym->type,
				.sym = sym,
			};
			return OBJECT_NAME;
		} else if (sym->sy_type == sy_type) {
			// id has been redeclared via a typedef
			lval->value.spec = (specifier_t) {
				.type = sym->type,
				.sym = sym,
			};
			return TYPE_NAME;
		}
		// id has been redelcared as a variable (hopefully)
		return NAME;
	} else {
		lval->value.spec = keyword->spec;
	}
	return keyword->value;
}

static int
keyword_or_id (YYSTYPE *lval, const char *token)
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

		for (i = 0; i < ARRCOUNT(keywords); i++)
			Hash_Add (keyword_tab, &keywords[i]);
		for (i = 0; i < ARRCOUNT(qf_keywords); i++)
			Hash_Add (qf_keyword_tab, &qf_keywords[i]);
		for (i = 0; i < ARRCOUNT(at_keywords); i++)
			Hash_Add (at_keyword_tab, &at_keywords[i]);
		for (i = 0; i < ARRCOUNT(obj_keywords); i++)
			Hash_Add (obj_keyword_tab, &obj_keywords[i]);
		for (i = 0; i < ARRCOUNT(rua_keywords); i++)
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
		return process_keyword (lval, keyword, token);
	if (token[0] == '@') {
		return '@';
	}
	sym = symtab_lookup (current_symtab, token);
	if (!sym)
		sym = new_symbol (token);
	lval->value.symbol = sym;
	if (sym->sy_type == sy_type) {
		lval->value.spec = (specifier_t) {
			.type = sym->type,
			.sym = sym,
		};
		return TYPE_NAME;
	}
	if (sym->sy_type == sy_class)
		return CLASS_NAME;
	return NAME;
}

enum {
	suff_error = -1,
	suff_implicit,
	suff_unsigned,
	suff_long,
	suff_unsigned_long,
	suff_float,
	suff_double,
	suff_long_double,
};

static int
parse_suffix (const char *suffix, bool fp)
{
	int  expl = suff_implicit;
	bool unsign = false;

	if (!*suffix) {
		return 0;
	}
	if (fp) {
		if (*suffix == 'f' || *suffix == 'F') {
			expl = suff_float;
			suffix++;
		} else if (*suffix == 'd' || *suffix == 'D') {
			// treat as explicit double unless it's a proper C decimal
			// suffix, in which case the decimal part will be ignored
			// (non-standard, but no decimal supprt)
			expl = suff_double;
			suffix++;
			if (*suffix == 'f' || *suffix == 'F'
				|| *suffix == 'd' || *suffix == 'D'
				|| *suffix == 'l' || *suffix == 'L') {
				warning (0, "decimal fp treated as binary fp");
				expl = suff_double;
				suffix++;
			}
		} else if (*suffix == 'l' || *suffix == 'L') {
			expl = suff_long_double;
			suffix++;
		}
	} else {
		if (*suffix == 'f' || *suffix == 'F') {
			expl = suff_float;
			suffix++;
		} else if (*suffix == 'd' || *suffix == 'D') {
			expl = suff_double;
			suffix++;
		} else {
			if (*suffix == 'u' || *suffix == 'U') {
				unsign = true;
				expl = suff_unsigned;
				suffix++;
			}
			if (*suffix == 'l' || *suffix == 'L') {
				expl = unsign ? suff_unsigned_long : suff_long;
				suffix++;
				if (*suffix == 'l' || *suffix == 'L') {
					suffix++;
				}
				if (!unsign && (*suffix == 'u' || *suffix == 'U')) {
					expl = suff_unsigned_long;
					suffix++;
				}
			}
		}
	}

	if (*suffix) {
		return suff_error;
	}

	return expl;
}

static int
parse_number (rua_tok_t *tok, yyscan_t scanner)
{
	bool binary = false;
	const char *type = "integer";
	bool hex = false;
	bool fp = false;
	char buf[tok->textlen + 1], *dst = buf;
	const char *src = tok->text;
	if ((*dst = *src++) == '0') {
		switch ((*++dst = *src++)) {
			case 'b': case 'B':
				binary = true;
				type = "binary";
				break;
			case 'x': case 'X':
				hex = true;
				type = "hexadecimal";
				break;
			case '.':
				fp = true;
				break;
			case '1' ... '9':
				type = "octal";	// unless fp becomes true
				break;
			case '\'': case '_':
				if (*src == 'b' || *src == 'B' || *src == 'x' || *src == 'X') {
					error (0, "digit separator outside digit sequence");
					return -rua_error;
				}
				break;
		}
	}
	if (*dst) {
		dst += (*dst != '\'' && *dst != '_');
		while ((*dst = *src++)) {
			if (hex && (*dst == 'p' || *dst == 'P')) {
				fp = true;
			}
			if (!hex && (*dst == 'e' || *dst == 'E')) {
				fp = true;
			}
			if (*dst == '.') {
				fp = true;
			}
			// strip out digit separators (' is standard C, _ is a rust
			// thing, but it does look a bit nicerer than ', so why not).
			dst += (*dst != '\'' && *dst != '_');
		}
	}
	// use long long to avoid bit-size issues on windows
	long long   lvalue = 0;
	double      fvalue = 0;
	char       *endptr = 0;
	if (binary) {
		// to get here, 0b (or 0B) was seen, so buf is guaranted to start with
		// that
		lvalue = strtoll (buf + 2, &endptr, 2);
	} else {
		if (fp) {
			fvalue = strtod (buf, &endptr);
		} else {
			lvalue = strtoll (buf, &endptr, 0);
		}
	}

	int expl = parse_suffix (endptr, fp);
	if (expl < 0) {
		error (0, "invalid suffix \"%s\" on %s constant", endptr,
			   fp ? "floating" : type);
		return -rua_error;
	}

	if (fp) {
		if (expl == suff_float) {
			tok->value.expr = new_float_expr (fvalue);
		} else {
			if (expl == suff_long_double) {
				warning (0, "long double treated as double");
				expl = suff_double;
			}
			tok->value.expr = new_double_expr (fvalue, expl == suff_implicit);
		}
	} else {
		if (expl == suff_unsigned) {
			tok->value.expr = new_uint_expr (lvalue);
		} else if (expl == suff_long || expl == suff_implicit) {
			tok->value.expr = new_long_expr (lvalue, expl == suff_implicit);
		} else if (expl == suff_unsigned_long) {
			tok->value.expr = new_ulong_expr (lvalue);
		} else if (expl == suff_float) {
			tok->value.expr = new_float_expr (lvalue);
		} else if (expl == suff_double) {
			tok->value.expr = new_double_expr (lvalue, false);
		} else {
			internal_error (0, "invalid suffix enum: %d", expl);
		}
	}
	return VALUE;
}

typedef struct {
	yyscan_t    scanner;
	yybuffer    buffer;
} buffer_raii_t;

static void
qc_restore_buffer (buffer_raii_t *raii)
{
	yy_switch_to_buffer (raii->buffer, raii->scanner);
}

static void
qc_delete_buffer (buffer_raii_t *raii)
{
	yy_delete_buffer (raii->buffer, raii->scanner);
}

static int
parse_vector (rua_tok_t *tok, yyscan_t scanner)
{
	const char *end = tok->text + tok->textlen;
	while (end > tok->text && *--end != '\'') continue;

	const char *start = tok->text + 1;
	auto yyg = (struct yyguts_t *)scanner;
	auto __attribute__((cleanup (qc_restore_buffer)))
		saved_buffer = (buffer_raii_t) {
			.scanner = scanner,
			.buffer = YY_CURRENT_BUFFER,
		};
	auto __attribute__((cleanup (qc_delete_buffer)))
		buffer = (buffer_raii_t) {
			.scanner = scanner,
			.buffer = yy_scan_bytes (start, end - start, scanner),
		};

	int         token;
	rua_tok_t   vtok = { .location = tok->location, };
	vtok.location.first_column++;
	const expr_t *components[4+1] = {};	// currently, max of 4
	bool        negate[4] = {};
	int         width = 0;

	yy_push_state (VECTOR, scanner);
	do {
		token = yylex (&vtok, &vtok.location, scanner);
		if (-token == rua_number) {
			token = parse_number (&vtok, scanner);
		}
		if (token == VALUE) {
			if (width < 4) {
				components[width] = vtok.value.expr;
			}
			width++;
		} else if (token == '-') {
			if (width < 4) {
				negate[width] = true;
			}
		}
	} while (token && token != -rua_eof);
	yy_pop_state (scanner);

	if (width > 4) {
		error (0, "too many components in vector literal");
		width = 4;
		return -rua_error;
	}
	bool fp = false;
	for (int i = 0; i < width; i++) {
		if (!components[i]->implicit) {
			error (0, "explict numeric constant in vector literal."
				   " Suggest suffix after closing '.");
			return -rua_error;
		}
		fp |= is_double (get_type (components[i]));
	}

	// end points at the final ' and thus any suffix is after that
	int expl = parse_suffix (++end, fp);
	if (expl < 0) {
		error (0, "invalid suffix \"%s\" on %s vector constant", end,
			   fp ? "floating" : "integer");
		return -rua_error;
	}
	union {
		pr_float_t f[4];
		pr_int_t   i[4];
		pr_double_t d[4];
		pr_long_t  l[4];
		pr_type_t  t[PR_SIZEOF (lvec4)];
	} data;
	type_t *type = 0;
	if (expl == suff_long_double) {
		warning (0, "long double treated as double");
		expl = suff_double;
	}
	if (expl == suff_float) {
		for (int i = 0; i < width; i++) {
			auto c = components[i];
			if (is_double (get_type (c))) {
				data.f[i] = expr_double (c);
			} else {
				data.f[i] = expr_long (c);
			}
			if (negate[i]) {
				data.f[i] = -data.f[i];
			}
		}
		type = &type_float;
	} else if (expl == suff_double) {
		for (int i = 0; i < width; i++) {
			auto c = components[i];
			if (is_double (get_type (c))) {
				data.d[i] = expr_double (c);
			} else {
				data.d[i] = expr_long (c);
			}
			if (negate[i]) {
				data.d[i] = -data.d[i];
			}
		}
		type = &type_double;
	} else if (expl == suff_implicit) {
		if (fp) {
			for (int i = 0; i < width; i++) {
				auto c = components[i];
				if (is_double (get_type (c))) {
					data.f[i] = expr_double (c);
				} else {
					data.f[i] = expr_long (c);
				}
				if (negate[i]) {
					data.f[i] = -data.f[i];
				}
			}
			type = &type_float;
		} else {
			for (int i = 0; i < width; i++) {
				auto c = components[i];
				data.i[i] = expr_long (c);
				if (negate[i]) {
					data.i[i] = -data.i[i];
				}
			}
			type = &type_int;
		}
	} else if (expl == suff_unsigned) {
		for (int i = 0; i < width; i++) {
			auto c = components[i];
			data.i[i] = fp ? expr_double (c) : expr_long (c);
			if (negate[i]) {
				data.i[i] = -data.i[i];
			}
		}
		type = &type_uint;
	} else if (expl == suff_long || expl == suff_unsigned_long) {
		for (int i = 0; i < width; i++) {
			auto c = components[i];
			data.l[i] = expr_long (c);
			if (negate[i]) {
				data.l[i] = -data.l[i];
			}
		}
		type = expl == suff_unsigned_long ? &type_ulong : &type_long;
	}
	type = vector_type (type, width);
	tok->value.expr = new_value_expr (new_type_value (type, data.t),
									  expl == suff_implicit);

	return VALUE;
}

static int
parse_string (rua_tok_t *tok, int type, yyscan_t scanner)
{
	const char *str = make_string (tok->text, 0);
	if (type == rua_char) {
		if (str[1]) {
			warning (0, "multibyte char constant");
		}
		tok->value.expr = new_int_expr (*str, false);
		return VALUE;
	} else {
		tok->value.expr = new_string_expr (str);
		return STRING;
	}
}

static void
next_line (rua_loc_t *loc)
{
	if (options.preprocess_only) {
		puts ("");
	}
	loc->first_line = loc->last_line;
	loc->first_column = loc->last_column;
	loc->last_column = 1;
	loc->last_line++;
	pr.source_line++;
}

static void
undo_loc (rua_loc_t *loc)
{
	// the next call to update_yylloc will sort out first_line and first_column
	loc->last_line = loc->first_line;
	loc->last_column = loc->first_column;
}

static void
user_action (rua_tok_t *tok, rua_loc_t *loc, const char *text, size_t textlen,
			 int state)
{
	if (state == COMMENT || state == LCOMMENT || isspace (*text)) {
		tok->str_text[0] = ' ';
		tok->str_text[1] = 0;
		tok->text = tok->str_text;
		textlen = 1;
	} else if (state == SUPPRESS) {
		tok->str_text[0] = 0;
		tok->text = tok->str_text;
		textlen = 0;
	} else if (textlen < sizeof (tok->text)) {
		strncpy (tok->str_text, text, textlen);
		tok->str_text[textlen] = 0;
		tok->text = tok->str_text;
	} else {
		tok->text = save_string (text);
	}
	tok->textlen = textlen;
	loc->first_line = loc->last_line;
	loc->first_column = loc->last_column;
	// \n handling rules will take care of the column and line
	loc->last_column += textlen;
#if 0
	printf ("start: %2d [%3d %3d] [%3d %3d] '%s'\n", state,
			loc->first_line, loc->first_column,
			loc->last_line, loc->last_column,
			quote_string (text));
#endif
}

static void
expand_macro (rua_extra_t *extra, rua_macro_t *macro)
{
	int         num_tokens = macro->num_tokens;

	int         base = extra->expr_stack.size;
	DARRAY_OPEN_AT (&extra->expr_stack, base, num_tokens);
	int         index = num_tokens;
	for (auto t = macro->tokens; t; t = t->next) {
		extra->expr_stack.a[base + --index] = *t;
	}
	if (index) {
		internal_error (0, "taniwha can't count: %d", index);
	}
	for (index = num_tokens; index-- > 0;) {
		auto e = &extra->expr_stack.a[base + index];
		if (e->token == '#') {
			e->token = -rua_string;
			auto p = e - 1;
			auto str = quote_string (p->text);
			e->text = save_string (va (0, "\"%s\"", str));

			extra->expr_stack.a[base + --index] = (rua_expr_t) {
				.token = -rua_ignore,
			};
		} else if (e->token == PRE_CONCAT) {
			auto p1 = e + 1;
			auto p2 = e - 1;
			auto str = va (0, "%s%s", p1->text, p2->text);
			yy_scan_string (str, extra->subscanner);
			rua_tok_t   tok = { .location = e->location, };
			int token = yylex (&tok, &tok.location, extra->subscanner);
			if (token && yylex (&tok, &tok.location, extra->subscanner)) {
				error (0, "pasting \"%s\" and \"%s\" does not give a valid"
					   " preprocessing token", p1->text, p2->text);
				*e = (rua_expr_t) {
					.token = -rua_space,
					.text = " ",
				};
			} else {
				*e = (rua_expr_t) {
					.location = tok.location,
					.textlen = tok.textlen,
					.token = token,
					.text = save_string (tok.text),
				};
				*p2 = *p1 = (rua_expr_t) {
					.token = -rua_ignore,
				};
				index--;
			}
		}
	}
}

static int
preproc_token (rua_extra_t *extra, int token, rua_tok_t *tok, yyscan_t *scanner)
{
	if (token < 0) {
		rua_term    term = -token;
		switch (term) {
			case rua_id:
			case rua_ignore:
			case rua_num_term:
				internal_error (0, "unexpected rua token: %d", term);
			case rua_eof:
			case rua_error:
				break;
			case rua_number:
				token = parse_number (tok, scanner);
				break;
			case rua_vector:
				token = parse_vector (tok, scanner);
				break;
			case rua_string:
			case rua_char:
				break;
			case rua_space:
				if (!extra->recording || extra->params) {
					token = -rua_ignore;
				}
				break;
		}
	}
	if (-token != rua_ignore && extra->recording) {
		tok->token = token;
		if (token != PRE_EOD && token != ',' && token != ')') {
			token = PRE_TOKEN;
		}
	}
	return token;
}

static int
qc_token (rua_extra_t *extra, int token, rua_tok_t *tok, yyscan_t *scanner)
{
	auto value = &tok->value;

	if (token < 0) {
		rua_term    term = -token;
		switch (term) {
			case rua_ignore:
			case rua_num_term:
				internal_error (0, "unexpected rua token: %d", term);
			case rua_eof:
			case rua_error:
				break;
			case rua_id:
				token = keyword_or_id (tok, tok->text);
				break;
			case rua_number:
				token = parse_number (tok, scanner);
				if (token == VALUE && value->expr->implicit) {
					if (is_long (get_type (value->expr))) {
						pr_long_t v = expr_long (value->expr);
						if (v < INT32_MIN || v > INT32_MAX) {
							warning (0, "integer value truncated");
						}
						value->expr = new_int_expr (v, true);
					}
				}
				break;
			case rua_vector:
				token = parse_vector (tok, scanner);
				break;
			case rua_string:
			case rua_char:
				token = parse_string (tok, -token, scanner);
				break;
			case rua_space:
				if (options.preprocess_only) {
					printf (" ");
				}
				break;
		}
	}
	return token;
}

static int
qc_process (rua_extra_t *extra, int token, rua_tok_t *tok, yyscan_t scanner)
{
	auto loc = &tok->location;

	if (-token == rua_ignore) {
		return YYPUSH_MORE;
	}

	if (extra->expand || !extra->preprocessor) {
		symbol_t   *sym = 0;
		if (token == PRE_ID || token == -rua_id) {
			sym = symtab_lookup (extra->macro_tab, tok->text);
		}
		if (sym) {
			expand_macro (extra, sym->s.macro);
			return YYPUSH_MORE;
		}
	}
	if (extra->preprocessor) {
		auto state = extra->pre_state;
		token = preproc_token (extra, token, tok, scanner);
		if (-token == rua_ignore) {
			return YYPUSH_MORE;
		}
		return pre_yypush_parse (state, token, tok, loc, scanner);
	} else {
		if (options.preprocess_only) {
			printf ("%s", tok->text);
			return token ? YYPUSH_MORE : 0;
		}
		token = qc_token (extra, token, tok, scanner);
		if (token >= 0) {
			auto state = extra->qc_state;
			auto value = &tok->value;
			return qc_yypush_parse (state, token, value, loc, scanner);
		}
	}
	return YYPUSH_MORE;
}

int
qc_yyparse (FILE *in)
{
	int         status;
	yyscan_t    scanner;
	rua_tok_t   tok = { .location = { 1, 1, 1, 1, pr.source_file }, };
	rua_extra_t extra = {
		.qc_state = qc_yypstate_new (),
		.pre_state = pre_yypstate_new (),
		.cond_stack = DARRAY_STATIC_INIT (8),
		.expr_stack = DARRAY_STATIC_INIT (32),
		.macro_tab = new_symtab (0, stab_global),
	};

	yylex_init_extra (&extra, &scanner);
	yylex_init (&extra.subscanner);
	yyset_in (in, scanner);
	do {
		int token = yylex (&tok, &tok.location, scanner);
		while (true) {
			status = qc_process (&extra, token, &tok, scanner);
			if (status != YYPUSH_MORE || !extra.expr_stack.size) {
				break;
			}
			auto expr = DARRAY_REMOVE (&extra.expr_stack);
			token = expr.token;
			tok = (rua_tok_t) {
				.location = expr.location,
				.textlen = expr.textlen,
				.text = expr.text,	// macro token strings use save_string
				.token = token,
			};
		}
	} while (status == YYPUSH_MORE);

	yylex_destroy (extra.subscanner);
	yylex_destroy (scanner);
	qc_yypstate_delete (extra.qc_state);
	pre_yypstate_delete (extra.pre_state);
	free (extra.cond_stack.a);
	free (extra.expr_stack.a);
	return status;
}

rua_macro_t *
rua_start_macro (const char *name, bool params, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->params = params;
	extra->recording = true;

	int len = strlen (name);
	if (name[len - 1] == '(') {
		name = save_substring (name, len - 1);
	} else {
		name = save_string (name);
	}

	yy_pop_state (scanner);
	yy_push_state (MACRO, scanner);

	rua_macro_t *macro = malloc (sizeof (*macro));
	*macro = (rua_macro_t) {
		.name = name,
		.params = new_symtab (0, stab_param),
		.tail = &macro->tokens,
	};
	return macro;
}

rua_macro_t *
rua_end_params (rua_macro_t *macro, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->params = false;
	return macro;
}

rua_macro_t *
rua_macro_append (rua_macro_t *macro, rua_tok_t *token, void *scanner)
{
	if (token->token == -rua_space) {
		if (!macro->tokens) {
			// ignore leading space
			return macro;
		}
		auto t = (rua_expr_t *) macro->tail;
		if (t->token == '#' || t->token == PRE_CONCAT) {
			// ignore space after '#' and '##'
			return macro;
		}
	}
	rua_expr_t *expr = (rua_expr_t *) macro->tail;
	if (token->token != PRE_CONCAT
		|| !macro->tokens
		|| expr->token != -rua_space) {
		expr = malloc (sizeof (*expr));
		macro->num_tokens++;
	}
	*expr = (rua_expr_t) {
		.location = token->location,
		.textlen = token->textlen,
		.token = token->token,
		.text = save_string (token->text),
	};
	*macro->tail = expr;
	macro->tail = &expr->next;
	return macro;
}

rua_macro_t *
rua_macro_param (rua_macro_t *macro, rua_tok_t *token, void *scanner)
{
	if (token->token == -rua_space) {
		// ignore spaces
		return macro;
	}
	if (token->token != PRE_ID) {
		error (0, "expected parameter name, found \"%s\"", token->text);
		return macro;
	}
	if (symtab_lookup (macro->params, token->text)) {
		error (0, "duplicate macro parameter \"%s\"", token->text);
		return macro;
	}
	auto sym = new_symbol (token->text);
	symtab_addsymbol (macro->params, sym);
	return macro;
}

void
rua_macro_finish (rua_macro_t *macro, void *scanner)
{
	if (macro->tokens) {
		while (((rua_expr_t *) macro->tail)->token == -rua_space) {
			// space is never added as the first token, so it's guaranteed
			// there is always at least two tokens
			rua_expr_t *t;
			for (t = macro->tokens; t->next->next; t = t->next) continue;
			free (t->next);
			t->next = 0;
			macro->tail = &t->next;
			macro->num_tokens--;
		}
		if (macro->tokens->token == PRE_CONCAT
			|| ((rua_expr_t *) macro->tail)->token == PRE_CONCAT) {
			error (0, "'##' cannot appear at either end of a macro expansion");
			return;
		}
		for (auto t = macro->tokens; t; t = t->next) {
			if (t->token == '#'
				&& (!t->next
					|| t->next->token != PRE_ID
					|| !symtab_lookup (macro->params, t->next->text))) {
				error (0, "'#' is not followed by a macro parameter");
				return;
			}
		}
	}

	auto extra = qc_yyget_extra (scanner);
	auto macro_tab = extra->macro_tab;
	auto sym = symtab_lookup (macro_tab, macro->name);
	if (sym) {
		auto o = sym->s.macro->tokens;
		auto n = macro->tokens;
		while (o && n && o->text == n->text) {
			o = o->next;
			n = n->next;
		}
		if (o || n) {
			error (0, "\"%s\" redefined", macro->name);
		}
		return;
	}
	sym = new_symbol (macro->name);
	sym->sy_type = sy_macro;
	sym->s.macro = macro;
	symtab_addsymbol (macro_tab, sym);
}

void
rua_start_text (void *scanner)
{
	yy_pop_state (scanner);
	yy_push_state (TEXT, scanner);
}

void
rua_start_expr (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->expand = true;

	yy_pop_state (scanner);
	yy_push_state (PREEXPR, scanner);
}

void
rua_expand_on (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->expand = true;
}

void
rua_expand_off (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->expand = false;
}

void
rua_end_directive (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->preprocessor = false;
	extra->recording = false;
	extra->expand = false;
	yy_pop_state (scanner);
}

static void
dump_state_stack (void *scanner)
{
	auto yyg = (struct yyguts_t *)scanner;
	for (int i = 0; i < yyg->yy_start_stack_ptr; i++) {
		printf ("%d ", yyg->yy_start_stack[i]);
	}
	printf (": %d\n", yystart ());
}

void
rua_if (bool pass, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	rua_cond_t  cond = {
		.saw_true = pass,
		.saw_else = false,
		.own_state = true,
		.enabled = pass,
	};
	if (extra->cond_stack.size) {
		auto c = extra->cond_stack.a[extra->cond_stack.size - 1];
		cond.own_state = c.own_state & c.enabled;
	}
	DARRAY_APPEND (&extra->cond_stack, cond);
	yy_pop_state (scanner);	// remove DIRECTIVE/PREEXPR state
	if (cond.own_state && !cond.enabled) {
		yy_push_state (SUPPRESS, scanner);
	}
	// put PREEXPR on the stack for EOD to pop
	yy_push_state (PREEXPR, scanner);
}

void
rua_else (bool pass, const char *tok, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (!extra->cond_stack.size) {
		error (0, "#else without #if");
		return;
	}
	yy_pop_state (scanner);	// remove DIRECTIVE state
	auto cond = &extra->cond_stack.a[extra->cond_stack.size - 1];
	if (cond->saw_else) {
		error (0, "#%s after #else", tok);
		return;
	}
	if (cond->own_state && !cond->enabled) {
		yy_pop_state (scanner);
	}
	pass &= !cond->saw_true;
	cond->enabled = pass;
	cond->saw_true |= pass;
	cond->saw_else = strcmp (tok, "else") == 0;
	if (cond->own_state && !cond->enabled) {
		yy_push_state (SUPPRESS, scanner);
	}
	// put PREEXPR on the stack for EOD to pop
	yy_push_state (PREEXPR, scanner);
}

void
rua_endif (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (!extra->cond_stack.size) {
		error (0, "#endif without #if");
		return;
	}
	if (0) dump_state_stack (scanner);
	yy_pop_state (scanner);	// remove DIRECTIVE state
	auto cond = DARRAY_REMOVE (&extra->cond_stack);
	if (cond.own_state && !cond.enabled) {
		yy_pop_state (scanner);
	}
	// put PREEXPR on the stack for EOD to pop
	yy_push_state (PREEXPR, scanner);
}

bool
rua_defined (const char *name, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	auto macro_tab = extra->macro_tab;
	return symtab_lookup (macro_tab, name);
}

void
rua_undefine (const char *name, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	auto macro_tab = extra->macro_tab;
	auto sym = symtab_lookup (macro_tab, name);
	if (sym) {
		symtab_removesymbol (macro_tab, sym);
	}
}
