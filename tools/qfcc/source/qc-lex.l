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
#include "tools/qfcc/include/cpp.h"
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
	int         line;
} rua_cond_t;

typedef struct DARRAY_TYPE (rua_cond_t) rua_cond_stack_t;

typedef struct {
	YY_BUFFER_STATE buffer;
	rua_cond_stack_t cond_stack;
	rua_loc_t   location;
} rua_incl_t;

typedef struct DARRAY_TYPE (rua_macro_t *) rua_macro_list_t;
typedef struct DARRAY_TYPE (rua_incl_t) rua_include_stack_t;

typedef struct rua_extra_s {
		int         start_state;
		bool        preprocessor;
		bool        recording;
		bool        params;
		bool        expand;
		bool        suppressed;
		qc_yypstate *qc_state;
		pre_yypstate *pre_state;
		pre_yypstate *args_state;
		rua_cond_stack_t cond_stack;
		rua_include_stack_t include_stack;
		dstring_t  *dstr;
		symtab_t   *macro_tab;
		rua_macro_t *pending_macro;	// function-type waiting for args
		rua_macro_t *macro;			// macro being expanded
		yyscan_t    subscanner;
		rua_loc_t   location;
		char        str_text[8];	// EPHEMERAL! for short token strings
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
static int keyword_or_id (QC_YYSTYPE *lval, const char *token);
static void update_loc (rua_loc_t *loc, size_t textlen);
static void save_text (rua_tok_t *tok, const char *text, size_t textlen,
					   int state, rua_extra_t *extra);
static void next_line (rua_loc_t *loc, yyscan_t scanner);
static void dump_state_stack (void *scanner);
static void dump_token (rua_tok_t *tok, rua_loc_t *loc, int state);
static void dump_debug (int act, const char *text, int state);

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
	rua_ellipsis,
	rua_asx,
	rua_incop,
	rua_grab,

	rua_num_term,
} rua_term;

#define YY_USER_ACTION \
	update_loc (yylloc, yyleng); \
	save_text (yylval, yytext, yyleng, yystart(), extra); \
	if (0) { dump_debug (yy_act, yytext, yystart()); } \
	if (0) { dump_token (yylval, yylloc, yystart ()); } \
	if (0) { dump_state_stack(yyscanner); }

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
%x		DIRECTIVE TEXT
%x		PRAGMA VECTOR
%s		PREPROC PREEXPR
%s		MACRO ARGS
%x		CONT

%%
						auto extra = qc_yyget_extra (yyscanner);
						yyset_debug (0, yyscanner);
						grab_frame = GRAB_FRAME;
						grab_other = GRAB_OTHER;
						grab_write = GRAB_WRITE;

<COMMENT>"/*"			{ warning (0, "nested /* in comment"); yymore();}
<COMMENT>\*+"/"			{ yy_pop_state (yyscanner); return -rua_space; }
<COMMENT>\r*\n			{ next_line (yylloc, yyscanner); yymore();}
<COMMENT>[^*/\n]*		{ yymore(); }/* munch on anything but possible end of comment */
<COMMENT>\/+[^*\n]*		{ yymore(); }/* handle /s not followed by * */
<COMMENT>\*+/[^/]		{ yymore(); }/* handle *s not followed by / */
<COMMENT><<EOF>>		{ error (0, "EOF in comment"); return 0; }
<*>"/*"					{ yy_push_state (COMMENT, yyscanner); yymore(); }
<*>"//"					{
							yymore();
							yy_push_state (LCOMMENT, yyscanner);
						}
<LCOMMENT>[^\\\r\n]+	/* consume all but \ and EOL (see line continuation) */
<LCOMMENT>[\\]+/[^\r\n]	/* consume \ */
<LCOMMENT>\r*\n			{
							next_line (yylloc, yyscanner);
							yy_pop_state (yyscanner);
							int state = yystart ();
							if (state == PREEXPR || state == PREPROC
								|| state == DIRECTIVE || state == MACRO) {
								return PRE_EOD;
							}
						}

<ARGS>^#				{	return '#'; }
^#{s}+/{D}				{
							yy_push_state (PREPROC, yyscanner);
							extra->preprocessor = true;
							return PRE_LINE;
						}
<CONT>^.				{ yyless (yyleng - 1); yy_pop_state (yyscanner); }
<*>^{s}*#{s}*			{
							yy_push_state (DIRECTIVE, yyscanner);
							extra->preprocessor = true;
						}
<ARGS><<EOF>>			{ return 0; }
<ARGS>^{s}*				{ return -rua_space; }

<DIRECTIVE>{ID}			{ {
							int tok = directive (yytext, yyscanner);
							if (tok >= 0) {
								return tok;
							}
						} }
<PREPROC>{ID}			{ return PRE_ID; }
<PREPROC>{ID}\(			{ return PRE_IDp; }

<ARGS>\r*\n				{ next_line (yylloc, yyscanner); return -rua_space; }
<DIRECTIVE>\r*\n		|
<PREEXPR>\r*\n			|
<MACRO,TEXT>\r*\n		|
<PREPROC>\r*\n			{ next_line (yylloc, yyscanner); return PRE_EOD; }

<TEXT>[^\\\r\n]*		{ return PRE_TEXT; }
<TEXT>[\\]*				{ return PRE_TEXT; }
<MACRO>##				{ return PRE_CONCAT; }
<MACRO>#				{ return '#'; }
<PREPROC>{h_string}		{ return PRE_HSTRING; }
<PREPROC>{q_string}		{ return PRE_QSTRING; }
<PREEXPR>{q_string}		{ return PRE_QSTRING; }
<PREEXPR>defined		{ return PRE_DEFINED; }
<PREEXPR>{ID}			{ return PRE_ID; }

<INITIAL,ARGS,MACRO>{
	{ID}				|
	@{ID}				{ return -rua_id; }
}
@						{ return '@'; }

{pp_number}				{ return -rua_number; }
{pp_vnumber}			{ return -rua_vector; }
<VECTOR>{pp_number}		{ return -rua_number; }
<VECTOR>{m}				{ return yytext[0]; }
<VECTOR>'				{ return -rua_eof; }

{s_string}				{ return -rua_string; };
{c_string}				{ return -rua_char; }

[+\-*/&|^%]=			{ return -rua_asx; }
"%%="					{ return -rua_asx; }
"<<="					{ return -rua_asx; }
">>="					{ return -rua_asx; }

[!(){}.*/&|^~+\-=\[\];,#%?:] { return yytext[0]; }

"·"						{ return QC_DOT; }
"⋀"						{ return QC_WEDGE; }
"•"						{ return QC_DOT; }
"∧"						{ return QC_WEDGE; }
"∨"						{ return QC_REGRESSIVE; }
"†"						{ return QC_REVERSE; }
"∗"						{ return QC_STAR; }
"×"						{ return QC_CROSS; }
"⋆"						{ return QC_DUAL; }

"%%"					{ return QC_MOD; }

{ELLIPSIS}				{ return -rua_ellipsis; }

"<<"					{ return QC_SHL; }
">>"					{ return QC_SHR; }

"&&"					{ return QC_AND; }
"||"					{ return QC_OR; }
"=="					{ return QC_EQ; }
"!="					{ return QC_NE; }
"<="					{ return QC_LE; }
">="					{ return QC_GE; }
"<"						{ return QC_LT; }
">"						{ return QC_GT; }

"++"					{ return -rua_incop; }
"--"					{ return -rua_incop; }

"$"{s}*{FRAMEID}		{ return -rua_grab; }

<GRAB_FRAME>{FRAMEID}	{ add_frame_macro (yytext); }
<GRAB_OTHER>[^\r\n]*	/* skip */
<GRAB_WRITE>{s_string}	{
							const char *s = make_string (yytext, 0);
							write_frame_macros (s);
							BEGIN (GRAB_OTHER);	// ignore rest of line
						}
<PRAGMA>{PRAGMAID}		|
<PRAGMA>@{PRAGMAID}		{ return PRE_ID; }
<PRAGMA>\r*\n			{ next_line (yylloc, yyscanner); return PRE_EOD; }

<*>\\\r*\n				{
							/* line continuation */
							next_line (yylloc, yyscanner);
						}
<CONT><<EOF>>			{
							warning (0, "\\ at end of file");
							yy_pop_state (yyscanner);
							return 0;
						}
<*>\r*\n				{
							next_line (yylloc, yyscanner);
							if (yyg->yy_start_stack_ptr) {
								yy_pop_state (yyscanner);
								extra->preprocessor = false;
							}
						}

<*>{s}+					{ return -rua_space; }

<*>.					{
							error (0, "all your typo are belong to us:%d %d-%d",
								   yystart(),
								   yylloc->column, yylloc->last_column);
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
	{"notice",   PRE_NOTICE},
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
	auto extra = qc_yyget_extra (scanner);
	directive_t *directive = Hash_Find (cond_directive_tab, token);
	if (!directive && !extra->suppressed) {
		directive = Hash_Find (rua_directive_tab, token);
		if (!directive) {
			error (0, "invalid directive `%s`", token);
		}
	}
	if (!directive) {
		yy_pop_state (scanner);// suppressed non-conditional directive
		yy_push_state (LCOMMENT, scanner);
		extra->preprocessor = false;
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
	{ #type_name, QC_TYPE_SPEC, .spec = { .type = &type_##type_name } },
#include "tools/qfcc/include/vec_types.h"
};

// These keywords are all part of the Ruamoko (Objective-QC) language.
// The first time any one of them is encountered, the class system will be
// initialized.
// If not compiling for the QuakeForge VM, or if Ruamoko has been disabled,
// then they will be unavailable as keywords.
static keyword_t obj_keywords[] = {
	{"id",				QC_OBJECT_NAME, .spec = { .type = &type_id } 		},
	{"Class",			QC_TYPE_SPEC, .spec = { .type = &type_Class } 		},
	{"Method",			QC_TYPE_SPEC, .spec = { .type = &type_method } 	},
	{"Super",			QC_TYPE_SPEC, .spec = { .type = &type_super } 		},
	{"SEL",				QC_TYPE_SPEC, .spec = { .type = &type_SEL } 		},
	{"IMP",				QC_TYPE_SPEC, .spec = { .type = &type_IMP } 		},

	{"@class",			QC_CLASS					},
	{"@defs",			QC_DEFS					},
	{"@encode",			QC_ENCODE					},
	{"@end",			QC_END						},
	{"@implementation",	QC_IMPLEMENTATION			},
	{"@interface",		QC_INTERFACE				},
	{"@private",		QC_PRIVATE					},
	{"@protected",		QC_PROTECTED				},
	{"@protocol",		QC_PROTOCOL				},
	{"@public",			QC_PUBLIC					},
	{"@reference",		QC_REFERENCE				},
	{"@selector",		QC_SELECTOR				},
	{"@self",			QC_SELF					},
	{"@this",			QC_THIS					},

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
	{"for",			QC_FOR		},
	{"goto",		QC_GOTO	},
	{"break",		QC_BREAK	},
	{"continue",	QC_CONTINUE},
	{"switch",		QC_SWITCH	},
	{"case",		QC_CASE	},
	{"default",		QC_DEFAULT	},
	{"nil",			QC_NIL		},
	{"struct",		QC_STRUCT	},
	{"union",		QC_STRUCT	},
	{"enum",		QC_ENUM	},
	{"typedef",		QC_TYPEDEF	},
	{"extern",		QC_EXTERN	},
	{"static",		QC_STATIC	},
	{"sizeof",		QC_SIZEOF	},
	{"not",			QC_NOT		},
	{"auto",		QC_TYPE_SPEC, .spec = { .type = &type_auto } },
};

// These keywords require the QuakeForge VM to be of any use. ie, they cannot
// be supported (sanely) by v6 progs.
static keyword_t qf_keywords[] = {
	{"quaternion",	QC_TYPE_SPEC, .spec = { .type = &type_quaternion } },
	{"double",		QC_TYPE_SPEC, .spec = { .type = &type_double } },
	{"int",			QC_TYPE_SPEC, .spec = { .type = &type_int } 	},
	{"unsigned",	QC_TYPE_SPEC, .spec = { .is_unsigned = 1 } },
	{"signed",		QC_TYPE_SPEC, .spec = { .is_signed = 1 } },
	{"long",		QC_TYPE_SPEC, .spec = { .is_long = 1 } },
	{"short",		QC_TYPE_SPEC, .spec = { .is_short = 1 } },

	{"@function",	QC_TYPE_SPEC, .spec = { .type = &type_func } 	},
	{"@args",		QC_ARGS,				},
	{"@va_list",	QC_TYPE_SPEC, .spec = { .type = &type_va_list } 	},
	{"@param",		QC_TYPE_SPEC, .spec = { .type = &type_param } 		},
	{"@return",     QC_AT_RETURN,		},

	{"@hadamard",	QC_HADAMARD,	},
	{"@cross",		QC_CROSS,		},
	{"@dot",		QC_DOT,		},
	{"@wedge",		QC_WEDGE,		},
	{"@regressive",	QC_REGRESSIVE,	},
	{"@geometric",	QC_GEOMETRIC,	},
	{"@algebra",	QC_ALGEBRA,	},
};

// These keywors are always available. Other than the @ keywords, they
// form traditional QuakeC.
static keyword_t keywords[] = {
	{"void",		QC_TYPE_SPEC, .spec = { .type = &type_void } 	},
	{"float",		QC_TYPE_SPEC, .spec = { .type = &type_float } 	},
	{"string",		QC_TYPE_SPEC, .spec = { .type = &type_string } },
	{"vector",		QC_TYPE_SPEC, .spec = { .type = &type_vector } },
	{"entity",		QC_TYPE_SPEC, .spec = { .type = &type_entity } },
	{"local",		QC_LOCAL,					},
	{"return",		QC_RETURN,					},
	{"while",		QC_WHILE,					},
	{"do",			QC_DO,						},
	{"if",			QC_IF,						},
	{"else",		QC_ELSE,					},
	{"@system",		QC_SYSTEM,					},
	{"@overload",	QC_OVERLOAD,				},
	{"@attribute",  QC_ATTRIBUTE,				},
	{"@handle",     QC_HANDLE,					},
};

static const char *
keyword_get_key (const void *kw, void *unused)
{
	return ((keyword_t*)kw)->name;
}

static int
process_keyword (QC_YYSTYPE *lval, keyword_t *keyword, const char *token)
{
	if (keyword->value == QC_STRUCT) {
		lval->op = token[0];
	} else if (keyword->value == QC_OBJECT_NAME) {
		symbol_t   *sym;

		sym = symtab_lookup (current_symtab, token);
		lval->symbol = sym;
		// the global id symbol is always just a name so attempts to redefine
		// it globally can be caught and treated as an error, but it needs to
		// be redefinable when in an enclosing scope.
		if (sym->sy_type == sy_name) {
			// this is the global id (object)
			lval->spec = (specifier_t) {
				.type = sym->type,
				.sym = sym,
			};
			return QC_OBJECT_NAME;
		} else if (sym->sy_type == sy_type) {
			// id has been redeclared via a typedef
			lval->spec = (specifier_t) {
				.type = sym->type,
				.sym = sym,
			};
			return QC_TYPE_NAME;
		}
		// id has been redelcared as a variable (hopefully)
		return QC_NAME;
	} else {
		lval->spec = keyword->spec;
	}
	return keyword->value;
}

static int
keyword_or_id (QC_YYSTYPE *lval, const char *token)
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
	lval->symbol = sym;
	if (sym->sy_type == sy_type) {
		lval->spec = (specifier_t) {
			.type = sym->type,
			.sym = sym,
		};
		return QC_TYPE_NAME;
	}
	if (sym->sy_type == sy_class)
		return QC_CLASS_NAME;
	return QC_NAME;
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
			// (non-standard, but no decimal support)
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

static const expr_t *
parse_number (const rua_tok_t *tok, yyscan_t scanner)
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
					return 0;
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
		return 0;
	}

	if (fp) {
		if (expl == suff_float) {
			return new_float_expr (fvalue);
		} else {
			if (expl == suff_long_double) {
				warning (0, "long double treated as double");
				expl = suff_double;
			}
			return new_double_expr (fvalue, expl == suff_implicit);
		}
	} else {
		if (expl == suff_unsigned) {
			return new_uint_expr (lvalue);
		} else if (expl == suff_long || expl == suff_implicit) {
			return new_long_expr (lvalue, expl == suff_implicit);
		} else if (expl == suff_unsigned_long) {
			return new_ulong_expr (lvalue);
		} else if (expl == suff_float) {
			return new_float_expr (lvalue);
		} else if (expl == suff_double) {
			return new_double_expr (lvalue, false);
		} else {
			internal_error (0, "invalid suffix enum: %d", expl);
		}
	}
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

static const expr_t *
parse_vector (const rua_tok_t *tok, yyscan_t scanner)
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
	vtok.location.column++;
	const expr_t *components[4+1] = {};	// currently, max of 4
	bool        negate[4] = {};
	int         width = 0;

	yy_push_state (VECTOR, scanner);
	do {
		token = yylex (&vtok, &vtok.location, scanner);
		if (-token == rua_number) {
			auto expr = parse_number (&vtok, scanner);
			if (expr) {
				if (width < 4) {
					components[width] = expr;
				}
				width++;
			}
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
		return 0;
	}
	bool fp = false;
	for (int i = 0; i < width; i++) {
		if (!components[i]->implicit) {
			error (0, "explict numeric constant in vector literal."
				   " Suggest suffix after closing '.");
			return 0;
		}
		fp |= is_double (get_type (components[i]));
	}

	// end points at the final ' and thus any suffix is after that
	int expl = parse_suffix (++end, fp);
	if (expl < 0) {
		error (0, "invalid suffix \"%s\" on %s vector constant", end,
			   fp ? "floating" : "integer");
		return 0;
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
	return new_value_expr (new_type_value (type, data.t),
						   expl == suff_implicit);
}

static int
parse_string (QC_YYSTYPE *lval, const rua_tok_t *tok, int type,
			  yyscan_t scanner)
{
	const char *str = make_string (tok->text, 0);
	if (type == rua_char) {
		if (str[1]) {
			warning (0, "multibyte char constant");
		}
		lval->expr = new_int_expr (*str, false);
		return QC_VALUE;
	} else {
		lval->expr = new_string_expr (str);
		return QC_STRING;
	}
}

static void
next_line (rua_loc_t *loc, yyscan_t scanner)
{
	auto extra = qc_yyget_extra (scanner);
	loc->line = loc->last_line;
	loc->column = loc->last_column;
	loc->last_column = 1;
	loc->last_line++;
	if (!extra->recording && options.preprocess_output) {
		puts ("");
	}
}

static void
update_loc (rua_loc_t *loc, size_t textlen)
{
	loc->line = loc->last_line;
	loc->column = loc->last_column;
	// \n handling rules will take care of the column and line
	loc->last_column += textlen;
}

static void
save_text (rua_tok_t *tok, const char *text, size_t textlen, int state,
		   rua_extra_t *extra)
{
	/*if (state == TEXT) {
		while (isspace (*text)) {
			text++;
		}
	}
	if (state == COMMENT || state == LCOMMENT || isspace (*text)) {
		tok->str_text[0] = ' ';
		tok->str_text[1] = 0;
		tok->text = tok->str_text;
		textlen = 1;
	} else */if (extra && textlen < sizeof (extra->str_text)) {
		strncpy (extra->str_text, text, textlen);
		extra->str_text[textlen] = 0;
		tok->text = extra->str_text;
	} else {
		tok->text = save_string (text);
	}
	tok->textlen = textlen;
}

static bool
join_tokens (rua_tok_t *out, const rua_tok_t *p1, const rua_tok_t *p2,
			 rua_extra_t *extra)
{
	auto str = va (0, "%s%s", p1 ? p1->text : "", p2 ? p2->text : "");
	yy_scan_string (str, extra->subscanner);
	rua_tok_t   tok = { .text = str, };
	if (p1) {
		tok.location = p1->location;
	}
	int token = yylex (&tok, &tok.location, extra->subscanner);
	if (token && yylex (&tok, &tok.location, extra->subscanner)) {
		error (0, "pasting \"%s\" and \"%s\" does not give a valid"
			   " preprocessing token", p1->text, p2->text);
		out->token = -rua_space;
		out->text = " ";
		return false;
	}
	tok.token = token;
	*out = tok;
	return true;
}

static bool
stringize_arg (rua_tok_t *out, rua_macro_t *arg, rua_extra_t *extra)
{
	dstring_copystr (extra->dstr, "\"");
	for (auto e = arg->tokens; e; e = e->next) {
		auto str = quote_string (e->text);
		dstring_appendstr (extra->dstr, str);
	}
	dstring_appendstr (extra->dstr, "\"");
	*out = (rua_tok_t) {
		.textlen = extra->dstr->size - 1,
		.token = -rua_string,
		.text = save_string (extra->dstr->str),
	};
	return true;
}

static int
preproc_token (rua_extra_t *extra, PRE_YYSTYPE *lval, const rua_tok_t *tok,
			   yyscan_t *scanner)
{
	int         token = tok->token;
	if (!token) {
		return token;
	}
	if (token < 0) {
		rua_term    term = -token;
		switch (term) {
			case rua_ignore:
			case rua_num_term:
			case rua_grab:
				internal_error (0, "unexpected rua token: %d", term);
			case rua_eof:
			case rua_error:
			case rua_id:
			case rua_ellipsis:
			case rua_asx:
			case rua_incop:
				break;
			case rua_number:
				if (!extra->recording) {
					token = -rua_error;
					if ((lval->expr = parse_number (tok, scanner))) {
						token = PRE_VALUE;
					}
				}
				break;
			case rua_vector:
				if (!extra->recording) {
					token = -rua_error;
					if (!(lval->expr = parse_vector (tok, scanner))) {
						token = PRE_VALUE;
					}
				}
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
		if (token == PRE_ID) {
			token = -rua_id;
		}
		lval->t.token = token;
		if (token != PRE_EOD && token != ',' && token != '(' && token != ')') {
			token = PRE_TOKEN;
		}
	}
	return token;
}

static const expr_t *
convert_long (const expr_t *value)
{
	pr_long_t v = expr_long (value);
	if (is_int (type_default)) {
		if (v < INT32_MIN || v > UINT32_MAX) {
			warning (0, "integer value truncated");
		}
		return new_int_expr (v, true);
	} else {
		float f = v;
		if (f != v) {
			warning (0, "cannot represent value");
		}
		return new_float_expr (f);
	}
}

static int
qc_token (rua_extra_t *extra, QC_YYSTYPE *lval, const rua_tok_t *tok,
		  yyscan_t *scanner)
{
	int         token = tok->token;

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
				token = keyword_or_id (lval, tok->text);
				break;
			case rua_grab:
				{
					int ret = do_grab (tok->text);
					if (ret >= 0) {
						lval->expr = new_int_expr (ret, false);
						token = QC_VALUE;
					} else {
						yy_push_state (-ret, scanner);
					}
				}
				break;
			case rua_number:
				token = -rua_error;
				lval->expr = parse_number (tok, scanner);
				if (lval->expr) {
					token = QC_VALUE;
					if (lval->expr->implicit) {
						if (is_long (get_type (lval->expr))) {
							lval->expr = convert_long (lval->expr);
						}
					}
				}
				break;
			case rua_vector:
				token = -rua_error;
				if ((lval->expr = parse_vector (tok, scanner))) {
					token = QC_VALUE;
				}
				break;
			case rua_string:
			case rua_char:
				token = parse_string (lval, tok, -token, scanner);
				break;
			case rua_space:
				if (options.preprocess_output) {
					printf (" ");
				}
				break;
			case rua_ellipsis:
				token = QC_ELLIPSIS;
				break;
			case rua_asx:
				token = QC_ASX;
				lval->op = tok->text[0];
				switch (tok->text[1]) {
					case '=':	break;
					case '%':   lval->op = QC_MOD;	break;
					case '<':   lval->op = QC_SHL;	break;
					case '>':   lval->op = QC_SHR;	break;
				}
				break;
			case rua_incop:
				token = QC_INCOP;
				lval->op = tok->text[0];
				break;
		}
	}
	return token;
}

static void
rua_reset_args (rua_macro_t *macro)
{
	macro->num_args = 0;

	int  num_params = macro->num_params;
	if (num_params < 0) {
		num_params = -num_params;
	}
	if (!macro->args) {
		macro->args = calloc (num_params, sizeof (rua_macro_t *));
		for (int i = 0; i < num_params; i++) {
			macro->args[i] = malloc (sizeof (rua_macro_t));
		}
	}
	for (int i = 0; i < num_params; i++) {
		auto arg = macro->args[i];
		*arg = (rua_macro_t) {
			.tail = &arg->tokens,
		};
	}
}

static void
rua_start_args (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->recording = true;

	rua_reset_args (extra->pending_macro);
	yy_push_state (ARGS, scanner);
}

static void
rua_end_args (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->recording = false;
	yy_pop_state (scanner);
}

static void __attribute__((used))
dump_macro (rua_macro_t *macro)
{
	printf ("\n%s: ", macro->name);
	for (auto t = macro->tokens; t; t = t->next) {
		printf ("%s", t->text);
	}
	for (int i = 0; i < macro->num_args; i++) {
		dump_macro (macro->args[i]);
	}
}

static int
qc_process (rua_extra_t *extra, int token, rua_tok_t *tok, yyscan_t scanner)
{
	auto loc = &tok->location;

	if (-token == rua_ignore) {
		return YYPUSH_MORE;
	}

	pr.loc = *loc;

	rua_macro_t *macro = 0;
	if (extra->pending_macro) {
		PRE_YYSTYPE lval = { .t = *tok };

		if (extra->recording) {
			token = preproc_token (extra, &lval, tok, scanner);
			int status = pre_yypush_parse (extra->args_state, token,
										   &lval, &tok->location, scanner);
			if (status == YYPUSH_MORE) {
				return YYPUSH_MORE;
			}
			rua_end_args (scanner);

			macro = extra->pending_macro;
			extra->pending_macro = macro->next;
			macro->next = 0;
			if (status != 0) {
				error (0, "unterminated argument list invoking macro \"%s\"",
					   macro->name);
				return -1;
			}

			int num_args = macro->num_args;
			if (num_args == 1 && !macro->args[0]->num_tokens) {
				num_args = 0;
			}
			if (macro->num_params >= 0) {
				if (num_args != macro->num_params) {
					error (0, "macro \"%s\" passed %d arguments, but takes"
						   " just %d", macro->name, num_args,
						   macro->num_params);
					return YYPUSH_MORE;
				}
			} else {
				if (num_args < ~macro->num_params) {
					error (0, "macro \"%s\" requires %d arguments, but only"
						   " %d given", macro->name, ~macro->num_params,
						   num_args);
					return YYPUSH_MORE;
				}
			}
		} else if (token == -rua_space) {
			return YYPUSH_MORE;
		} else if (token == '(') {
			rua_start_args (scanner);
			token = PRE_ARGS;
			int status = pre_yypush_parse (extra->args_state, token,
										   &lval, &tok->location, scanner);
			if (status != YYPUSH_MORE) {
				internal_error (0, "can't start parsing macro args");
			}
			return YYPUSH_MORE;
		} else {
			// XXX put id back into stream to be passed to main parser
			// (expansion failed)
			return YYPUSH_MORE;
		}
	} else if (extra->expand || (!extra->preprocessor && !extra->suppressed)) {
		symbol_t   *sym = 0;
		if (token == -rua_id || (extra->preprocessor && token == PRE_ID)) {
			sym = symtab_lookup (extra->macro_tab, tok->text);
		}
		if (sym) {
			if (sym->s.macro->params) {
				extra->pending_macro = sym->s.macro;
				return YYPUSH_MORE;
			} else {
				macro = sym->s.macro;
			}
		}
	}
	if (macro) {
		if (macro->update) {
			macro->update (macro, scanner);
		}
		if (macro->tokens) {
			macro->next = extra->macro;
			extra->macro = macro;
			macro->cursor = macro->tokens;
		}
		return YYPUSH_MORE;
	}
	if (extra->preprocessor) {
		auto state = extra->pre_state;
		PRE_YYSTYPE lval = { .t = *tok };

		token = preproc_token (extra, &lval, tok, scanner);
		if (-token == rua_ignore) {
			return YYPUSH_MORE;
		}
		return pre_yypush_parse (state, token, &lval, loc, scanner);
	} else {
		if (!extra->suppressed) {
			if (options.preprocess_only) {
				if (options.preprocess_output) {
					printf ("%s", tok->text);
				}
				return token ? YYPUSH_MORE : 0;
			}
			QC_YYSTYPE  lval = {};
			token = qc_token (extra, &lval, tok, scanner);
			if (token >= 0) {
				auto state = extra->qc_state;
				return qc_yypush_parse (state, token, &lval, loc, scanner);
			}
		}
	}
	return YYPUSH_MORE;
}

static rua_tok_t
next_macro_token (rua_extra_t *extra)
{
	auto e = *extra->macro->cursor;
	extra->macro->cursor = e.next;
	return e;
}

static rua_tok_t *
get_arg_token (bool last, rua_tok_t *arg, const rua_macro_t *macro)
{
	symbol_t   *sym;

	if (arg->token == -rua_id && macro->params
	    && (sym = symtab_lookup (macro->params, arg->text))
	    && !macro->args[sym->s.offset]->next) {
		auto a = macro->args[sym->s.offset];
		if (last) {
			if (a->tokens) {
				return (rua_tok_t *) a->tail;
			}
			return 0;
		} else {
			return a->tokens;
		}
	}
	return arg;
}

static void
copy_token (rua_macro_t *dst, const rua_tok_t *t)
{
	rua_tok_t *e = malloc (sizeof (*e));
	*e = *t;
	e->next = 0;
	*dst->tail = e;
	dst->tail = &e->next;
}

static void
copy_arg_token (rua_macro_t *dst, bool skip_first, bool skip_last,
				const rua_tok_t *arg, const rua_macro_t *macro)
{
	symbol_t   *sym;

	if (arg->token == -rua_id && macro->params
	    && (sym = symtab_lookup (macro->params, arg->text))
	    && !macro->args[sym->s.offset]->next) {
		auto a = macro->args[sym->s.offset];
		if (!a->tokens) {
			return;
		}
		for (auto t = skip_first ? a->tokens->next : a->tokens;
			 t; t = t->next) {
			if (skip_last && !t->next) {
				break;
			}
			copy_token (dst, t);
		}
	} else if (!skip_first && !skip_last) {
		copy_token (dst, arg);
	}
}

static void
collect_args (rua_macro_t *m, rua_macro_t *macro, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	m->next = extra->pending_macro;
	extra->pending_macro = m;
	auto t = macro->cursor;
	for (; t && t->token == -rua_space; t = t->next) continue;
	// to be in an arg, a function-like macro must either have
	// both open and close parans, or none
	if (t && t->token == '(') {
		rua_reset_args (m);
		int paren = 1;
		auto a = rua_macro_arg (t, scanner);
		for (t = t->next; t; t = t->next) {
			if (t->token == '(') {
				paren++;
			} else if (t->token == ')') {
				if (!(--paren)) {
					t = t->next;
					break;
				}
			}
			if (t->token == ',' && paren == 1) {
				a = rua_macro_arg (t, scanner);
			} else {
				rua_macro_append (a, t, scanner);
			}
		}
	}
	macro->cursor = t;
	extra->pending_macro = m->next;
}

static int
next_token (rua_tok_t *tok, yyscan_t scanner)
{
	auto extra = qc_yyget_extra (scanner);
	int token;
rescan:
	while (extra->macro && !extra->macro->cursor) {
		auto macro = extra->macro;
		extra->macro = macro->next;
		macro->next = 0;
	}
	if (extra->macro && extra->macro->cursor) {
		symbol_t   *sym;
		auto macro = extra->macro;
		auto e = next_macro_token (extra);
		token = e.token;
		if (token == '#') {
			auto n = e.next;
			rua_macro_t *arg;
			macro->cursor = macro->cursor->next;	// consume arg
			if (strcmp (n->text, "__VA_OPT__") == 0) {
				sym = symtab_lookup (extra->macro_tab, n->text);
				auto m = sym->s.macro;
				collect_args (m, macro, scanner);
				m->next = 0;
				arg = m->args[0];
			} else {
				sym = symtab_lookup (macro->params, n->text);
				arg = macro->args[sym->s.offset];
			}
			stringize_arg (&e, arg, extra);
		} else if (e.next && e.next->token == PRE_CONCAT) {
			rua_macro_t *m = malloc (sizeof (*m));
			*m = (rua_macro_t) {
				.tail = &m->tokens,
			};

			bool sf = false;
			bool sl;
			do {
				auto p = get_arg_token (true, &e, macro);
				auto n = get_arg_token (false, e.next->next, macro);
				rua_tok_t   cat;
				if (!(sl = join_tokens (&cat, p, n, extra))) {
					cat.location = e.next->location;
					cat.token = -rua_space;
					cat.text = " ";
				}
				copy_arg_token (m, sf, sl, &e, macro);
				if (cat.token) {
					copy_token (m, &cat);
				}
				next_macro_token (extra);	// consume ##
				e = next_macro_token (extra);
				sf = sl;
			} while (e.next && e.next->token == PRE_CONCAT);
			copy_arg_token (m, sl, false, &e, macro);
			m->cursor = m->tokens;
			m->next = extra->macro;
			extra->macro = m;
			goto rescan;
		} else if (token == -rua_id && macro->params
				   && (sym = symtab_lookup (macro->params, e.text))
				   && !macro->args[sym->s.offset]->next) {
			auto arg = macro->args[sym->s.offset];
			arg->cursor = arg->tokens;
			arg->next = extra->macro;
			extra->macro = arg;
			goto rescan;
		} else if (token == -rua_id && !macro->name /* in arg */
				   && (sym = symtab_lookup (extra->macro_tab, e.text))
				   && !sym->s.macro->next) {
			auto m = sym->s.macro;
			if (m->update) {
				m->update (m, scanner);
			}
			m->cursor = m->tokens;
			if (!m->params) {
				m->next = extra->macro;
				extra->macro = m;
				goto rescan;
			}

			collect_args (m, macro, scanner);
			m->next = extra->macro;
			extra->macro = m;
			goto rescan;
		}
		*tok = e;
	} else {
		tok->token = yylex (tok, &extra->location, scanner);
		tok->location = extra->location;
		if (extra->recording && tok->text == extra->str_text) {
			tok->text = save_string (tok->text);
		}
	}
	return tok->token;
}

int
qc_yyparse (FILE *in)
{
	rua_extra_t extra = {
		.qc_state = qc_yypstate_new (),
		.pre_state = pre_yypstate_new (),
		.args_state = pre_yypstate_new (),
		.cond_stack = DARRAY_STATIC_INIT (8),
		.include_stack = DARRAY_STATIC_INIT (8),
		.dstr = dstring_new (),
		.macro_tab = cpp_macros ? cpp_macros : new_symtab (0, stab_global),
		.location = { 1, 1, 1, 1, pr.loc.file },
	};

	yyscan_t    scanner;
	yylex_init_extra (&extra, &scanner);
	yylex_init (&extra.subscanner);
	yyset_in (in, scanner);

	int         status;
	rua_tok_t   tok = {};
	do {
		int token = next_token (&tok, scanner);
		if (!token && extra.cond_stack.size) {
			int ind = extra.cond_stack.size - 1;
			auto cond = extra.cond_stack.a[ind];
			error (0, "end of file in conditional started on %d", cond.line);
		}
		if (!token && extra.include_stack.size) {
			//printf ("*** pop include %d\n", (int) extra.include_stack.size);
			auto incl = DARRAY_REMOVE (&extra.include_stack);
			extra.location = incl.location;
			struct yyguts_t * yyg = (struct yyguts_t*)scanner;//FIXME
			yy_delete_buffer (YY_CURRENT_BUFFER, scanner);
			set_line_file (-1, 0, 2);
			yy_switch_to_buffer (incl.buffer, scanner);
			DARRAY_CLEAR (&extra.cond_stack);
			extra.cond_stack = incl.cond_stack;
			continue;
		}
		status = qc_process (&extra, token, &tok, scanner);
	} while (status == YYPUSH_MORE);

	yylex_destroy (extra.subscanner);
	yylex_destroy (scanner);
	qc_yypstate_delete (extra.qc_state);
	pre_yypstate_delete (extra.pre_state);
	pre_yypstate_delete (extra.args_state);
	free (extra.cond_stack.a);
	free (extra.include_stack.a);
	dstring_delete (extra.dstr);
	return status;
}

rua_macro_t *
rua_start_macro (const char *name, bool params, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (extra->suppressed) {
		return 0;
	}
	extra->recording = true;
	extra->params = params;

	if (name) {
		int len = strlen (name);
		if (name[len - 1] == '(') {
			name = save_substring (name, len - 1);
		} else {
			name = save_string (name);
		}
	}

	yy_pop_state (scanner);
	yy_push_state (MACRO, scanner);

	rua_macro_t *macro = malloc (sizeof (*macro));
	*macro = (rua_macro_t) {
		.name = name,
		.params = params ? new_symtab (0, stab_param) : 0,
		.tail = &macro->tokens,
	};
	return macro;
}

rua_macro_t *
rua_end_params (rua_macro_t *macro, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (extra->suppressed) {
		return 0;
	}
	extra->params = false;
	//macro->params->parent = extra->macro_tab;
	return macro;
}

rua_macro_t *
rua_macro_append (rua_macro_t *macro, const rua_tok_t *token, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (extra->suppressed || !macro) {
		return 0;
	}
	if (token->token == -rua_space) {
		if (!macro->tokens) {
			// ignore leading space
			return macro;
		}
		auto t = (rua_tok_t *) macro->tail;
		if (t->token == '#' || t->token == PRE_CONCAT) {
			// ignore space after '#' and '##'
			return macro;
		}
	}
	auto expr = (rua_tok_t *) macro->tail;
	if (token->token != PRE_CONCAT
		|| !macro->tokens
		|| expr->token != -rua_space) {
		expr = malloc (sizeof (*expr));
		macro->num_tokens++;
	}
	*expr = *token;
	*macro->tail = expr;
	macro->tail = &expr->next;
	return macro;
}

rua_macro_t *
rua_macro_param (rua_macro_t *macro, const rua_tok_t *token, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (extra->suppressed) {
		return 0;
	}
	if (token->token == -rua_space) {
		// ignore spaces
		return macro;
	}
	if (token->token != -rua_id && token->token != -rua_ellipsis) {
		error (0, "expected parameter name, found \"%s\"", token->text);
		return macro;
	}
	if (macro->num_params < 0) {
		error (0, "... must be the last parameter");
		return macro;
	} else if (symtab_lookup (macro->params, token->text)) {
		error (0, "duplicate macro parameter \"%s\"", token->text);
		return macro;
	}
	auto sym = new_symbol (token->text);
	sym->sy_type = sy_var;
	sym->s.offset = macro->num_params++;
	symtab_addsymbol (macro->params, sym);
	if (token->token == -rua_ellipsis) {
		macro->num_params = -macro->num_params;
	}
	return macro;
}

void
rua_macro_finish (rua_macro_t *macro, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (extra->suppressed) {
		return;
	}
	if (macro->tokens) {
		while (((rua_tok_t *) macro->tail)->token == -rua_space) {
			// space is never added as the first token, so it's guaranteed
			// there is always at least two tokens
			rua_tok_t  *t;
			for (t = macro->tokens; t->next->next; t = t->next) continue;
			free (t->next);
			t->next = 0;
			macro->tail = &t->next;
			macro->num_tokens--;
		}
		if (macro->tokens->token == PRE_CONCAT
			|| ((rua_tok_t *) macro->tail)->token == PRE_CONCAT) {
			error (0, "'##' cannot appear at either end of a macro expansion");
			return;
		}
		for (auto t = macro->tokens; t; t = t->next) {
			if (t->token == '#'
				&& (!t->next
					|| t->next->token != -rua_id
					|| (!symtab_lookup (macro->params, t->next->text)
						&& strcmp (t->next->text, "__VA_OPT__") != 0))) {
				error (0, "'#' is not followed by a macro parameter");
				return;
			}
			if (t->token == -rua_id && strcmp (t->text, "__VA_OPT__") == 0) {
				auto u = t->next;
				for (; u && u->token == -rua_space; u = u->next) continue;
				if (!u) {
unterminated_va_opt:
					error (0, "unterminated __VA_OPT__");
					return;
				} else if (u->token != '(') {
					error (0, "__VA_OPT__ must be followed by an open "
						   "parenthesis");
					return;
				}
				if (u->next && u->next->token == PRE_CONCAT) {
hashhash_error:
					error (0, "'##' cannot appear at either end of __VA_OPT__");
					return;
				}
				int paren = 1;
				for (u = u->next; u; u = u->next) {
					if (u->token == '(') {
						paren++;
					} else if (u->token == ')') {
						if (!(--paren)) {
							break;
						}
					} else if (u->token == -rua_id
							   && strcmp (u->text, "__VA_OPT__") == 0) {
						error (0, "__VA_OPT__ may not appear in a __VA_OPT__");
						return;
					} else if (u->token == PRE_CONCAT && paren == 1
							   && u->next && u->next->token == ')') {
						goto hashhash_error;
					}
				}
				if (!u) {
					goto unterminated_va_opt;
				}
			}
		}
	}

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

rua_macro_t *
rua_macro_arg (const rua_tok_t *token, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	auto macro = extra->pending_macro;
	rua_macro_t *arg;
	if (macro->num_params < 0 && macro->num_args == -macro->num_params) {
		arg = macro->args[macro->num_args - 1];
		rua_macro_append (arg, token, scanner);
	} else {
		if (macro->num_args < macro->num_params
			|| macro->num_args < -macro->num_params) {
			arg = macro->args[macro->num_args++];
		} else {
			// count the excess args for error reporting
			macro->num_args++;
			arg = 0;
		}
	}
	return arg;
}

void
rua_start_pragma (void *scanner)
{
	if (options.preprocess_output) {
		printf ("#pragma");
	}
	yy_pop_state (scanner);
	yy_push_state (PRAGMA, scanner);
}

void
rua_start_text (void *scanner)
{
	yy_pop_state (scanner);
	yy_push_state (TEXT, scanner);
}

void
rua_start_include (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->expand = true;

	yy_pop_state (scanner);
	yy_push_state (PREPROC, scanner);
}

void
rua_start_expr (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->expand = !extra->suppressed;

	yy_pop_state (scanner);
	yy_push_state (PREEXPR, scanner);
}

void
rua_expand_on (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->expand = !extra->suppressed;
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
dump_debug (int act, const char *text, int start)
{
	if (act == 0) {
		printf("--scanner backing up\n" );
	} else if (act < YY_NUM_RULES) {
		printf("--accepting rule at line %ld\n",
			(long)yy_rule_linenum[act]);
	} else if (act == YY_NUM_RULES) {
		printf("--accepting default rule\n");
	} else if (act == YY_NUM_RULES + 1 ) {
		printf("--(end of buffer or a NUL)\n");
	} else {
		printf("--EOF (start condition %d)\n", start);
	}
}

static void
dump_state_stack (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	auto yyg = (struct yyguts_t *)scanner;
	for (int i = 0; i < yyg->yy_start_stack_ptr; i++) {
		printf ("%d ", yyg->yy_start_stack[i]);
	}
	printf (": %d s:%s p:%s r:%s e:%s a:%s %s:%d\n", yystart (),
			extra ? extra->suppressed ? "true" : "false" : "n/a",
			extra ? extra->preprocessor ? "true" : "false" : "n/a",
			extra ? extra->recording ? "true" : "false" : "n/a",
			extra ? extra->expand ? "true" : "false" : "n/a",
			extra ? extra->params ? "true" : "false" : "n/a",
			pr.strings ? GETSTR(pr.loc.file) : "", pr.loc.line);
}

static void
dump_token (rua_tok_t *tok, rua_loc_t *loc, int state)
{
	printf ("start: %2d [%3d %3d] [%3d %3d] %d '%s'\n", state,
			loc->line, loc->column,
			loc->last_line, loc->last_column,
			tok->token, quote_string (tok->text));
}

void
rua_start_if (bool expand, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	rua_cond_t  cond = {
		.saw_true = false,
		.saw_else = false,
		.own_state = true,
		.enabled = false,
		.line = pr.loc.line,
	};
	if (extra->cond_stack.size) {
		auto c = extra->cond_stack.a[extra->cond_stack.size - 1];
		cond.own_state = c.own_state & c.enabled;
	}
	DARRAY_APPEND (&extra->cond_stack, cond);
	extra->expand = expand;

	yy_pop_state (scanner);
	yy_push_state (PREEXPR, scanner);
}

void
rua_start_else (bool expand, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	extra->expand = expand;

	yy_pop_state (scanner);
	yy_push_state (PREEXPR, scanner);
}

void
rua_if (bool pass, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (!extra->cond_stack.size) {
		internal_error (0, "#if without start_if");
		return;
	}
	auto cond = &extra->cond_stack.a[extra->cond_stack.size - 1];
	cond->saw_true = cond->enabled = pass;
	//yy_pop_state (scanner);	// remove DIRECTIVE/PREEXPR state
	if (cond->own_state && !cond->enabled) {
		extra->suppressed = true;
	}
	//printf ("#if on %s:%d %d\n", GETSTR(pr.loc.file),
	//		cond->line, extra->suppressed);
	// put PREEXPR on the stack for EOD to pop
	//yy_push_state (PREEXPR, scanner);
}

void
rua_else (bool pass, const char *tok, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (!extra->cond_stack.size) {
		error (0, "#else without #if");
		return;
	}
	//yy_pop_state (scanner);	// remove DIRECTIVE state
	auto cond = &extra->cond_stack.a[extra->cond_stack.size - 1];
	if (cond->saw_else) {
		error (0, "#%s after #else", tok);
		return;
	}
	if (cond->own_state && !cond->enabled) {
		extra->suppressed = false;
		//yy_pop_state (scanner);
	}
	pass &= !cond->saw_true;
	cond->enabled = pass;
	cond->saw_true |= pass;
	cond->saw_else = strcmp (tok, "else") == 0;
	//printf ("#else on %s:%d for %d %d\n", GETSTR(pr.loc.file),
	//		pr.loc.line, cond->line, extra->suppressed);
	cond->line = pr.loc.line;
	if (cond->own_state && !cond->enabled) {
		extra->suppressed = true;
	}
	// put PREEXPR on the stack for EOD to pop
	//yy_push_state (PREEXPR, scanner);
}

void
rua_endif (void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (!extra->cond_stack.size) {
		error (0, "#endif without #if");
		return;
	}
	//yy_pop_state (scanner);	// remove DIRECTIVE state
	auto cond = DARRAY_REMOVE (&extra->cond_stack);
	//printf ("#endif on %s:%d for %d %d\n", GETSTR(pr.loc.file),
	//		pr.loc.line, cond.line, extra->suppressed);
	if (cond.own_state && !cond.enabled) {
		extra->suppressed = false;
		//yy_pop_state (scanner);
	}
	// put PREEXPR on the stack for EOD to pop
	//yy_push_state (PREEXPR, scanner);
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
	if (extra->suppressed) {
		return;
	}
	auto macro_tab = extra->macro_tab;
	auto sym = symtab_lookup (macro_tab, name);
	if (sym) {
		symtab_removesymbol (macro_tab, sym);
	}
}

void
rua_include_file (const char *name, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (extra->suppressed) {
		return;
	}
	struct yyguts_t * yyg = (struct yyguts_t*)scanner;//FIXME
	int quote = *name;
	name = make_string (name, 0);
	bool is_system;
	auto found = cpp_find_file (name, quote, &is_system);
	if (!found || !(yyin = fopen (found, "rb"))) {
		error (0, "fatal error: %s: %s", name, strerror (errno));
		exit (1);
	}
	//printf ("*** push include %s\n", name);
	set_line_file (1, found, 1);
	DARRAY_APPEND (&extra->include_stack, ((rua_incl_t) {
		.buffer = YY_CURRENT_BUFFER,
		.cond_stack = extra->cond_stack,
		.location = extra->location,
	}));
	extra->cond_stack = (rua_cond_stack_t) DARRAY_STATIC_INIT (8);
	auto buffer = yy_create_buffer (yyin, YY_BUF_SIZE, scanner);
	yy_switch_to_buffer (buffer, scanner);
	extra->location = (rua_loc_t) {
		.line = 1,
		.column = 1,
		.last_line = 1,
		.last_column = 1,
		.file = ReuseString (found),
	};
}

void
rua_embed_file (const char *name, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	if (extra->suppressed) {
		return;
	}
	internal_error (0, "not implemented");
	(void)yy_top_state;
}

int
rua_parse_define (const char *def)
{
	int         status;
	yyscan_t    scanner;
	rua_tok_t   tok = { .location = { 1, 1, 1, 1 }, .token = -1, };
	rua_extra_t extra = {
		.preprocessor = true,
		.pre_state = pre_yypstate_new (),
		.macro_tab = cpp_macros,
	};

	yylex_init_extra (&extra, &scanner);
	yy_scan_string (def, scanner);

	yy_push_state (PREPROC, scanner);

	do {
		if (tok.token == -1) {
			tok.token = PRE_DEFINE;
		} else {
			tok.token = yylex (&tok, &tok.location, scanner);
			status = 0;
		}
		while (tok.token) {
			status = qc_process (&extra, tok.token, &tok, scanner);
			if (status != YYPUSH_MORE || !extra.macro) {
				break;
			}
			tok = *extra.macro->cursor;
			extra.macro->cursor = extra.macro->cursor->next;
			if (!extra.macro->cursor) {
				extra.macro = extra.macro->next;
			}
		}
	} while (status == YYPUSH_MORE);

	yylex_destroy (scanner);
	pre_yypstate_delete (extra.pre_state);
	dstring_delete (extra.dstr);
	return status;
}

void
rua_line_info (const expr_t *line_expr, const char *file,
			   const expr_t *flags_expr, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);

	int line = expr_long (line_expr);
	int flags = flags_expr ? expr_long (flags_expr) : 0;

	if (file) {
		if (*file != '"') {
			error (0, "\"%s\" is not a valid filename", file);
			return;
		}
		file = make_string (file, 0);
	}
	set_line_file (line, file, flags);
	extra->location = pr.loc;
}

void
rua_macro_file (rua_macro_t *macro, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	int         file = extra->location.file;
	if (macro->tokens && macro->tokens->location.file == file) {
		return;
	}
	macro->tokens = 0;
	macro->tail = &macro->tokens;
	macro->num_tokens = 0;

	rua_tok_t *expr = malloc (sizeof (*expr));
	macro->num_tokens++;
	const char *file_str = save_string (va (0, "\"%s\"",
											quote_string (GETSTR (file))));
	*expr = (rua_tok_t) {
		.location = extra->location,
		.textlen = strlen (file_str),
		.token = -rua_string,
		.text = file_str,
	};
	*macro->tail = expr;
	macro->tail = &expr->next;
}

void
rua_macro_line (rua_macro_t *macro, void *scanner)
{
	auto extra = qc_yyget_extra (scanner);
	int         line = extra->location.line;

	if (macro->tokens && macro->tokens->location.line == line) {
		return;
	}
	macro->tokens = 0;
	macro->tail = &macro->tokens;
	macro->num_tokens = 0;

	rua_tok_t *expr = malloc (sizeof (*expr));
	macro->num_tokens++;
	const char *line_str = save_string (va (0, "%d", line));
	*expr = (rua_tok_t) {
		.location = extra->location,
		.textlen = strlen (line_str),
		.token = -rua_number,
		.text = line_str,
	};
	*macro->tail = expr;
	macro->tail = &expr->next;
}

void
rua_macro_va_opt (rua_macro_t *macro, void *scanner)
{
	macro->tokens = 0;
	macro->tail = &macro->tokens;

	auto extra = qc_yyget_extra (scanner);
	auto cur = extra->macro;
	if (!cur || !cur->params || cur->num_params >= 0) {
		warning (0, "__VA_OPT__ can appear only in the expansion of a "
				 "variadic macro");
		return;
	}
	auto arg = cur->args[~cur->num_params];
	if (arg->tokens) {
		macro->tokens = macro->args[0]->tokens;
		macro->tail = macro->args[0]->tail;
	}
}

void
rua_macro_va_args (rua_macro_t *macro, void *scanner)
{
	macro->tokens = 0;
	macro->tail = &macro->tokens;

	auto extra = qc_yyget_extra (scanner);
	auto cur = extra->macro;
	if (cur && strcmp (cur->name, "__VA_OPT__") == 0) {
		cur = cur->next;
	}
	if (!cur || !cur->params || cur->num_params >= 0) {
		warning (0, "__VA_ARGS__ can appear only in the expansion of a "
				 "variadic macro");
		return;
	}
	auto arg = cur->args[~cur->num_params];
	if (arg->tokens) {
		macro->tokens = arg->tokens;
		macro->tail = arg->tail;
	}
}
