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
%option stack

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
#include "QF/plist.h"
#include "QF/sys.h"

#define CEXPR_YYDEBUG 1

#include "QF/cexpr.h"
#include "libs/util/cexpr-parse.h"

#define YY_NO_INPUT	// debian flex
#define YY_NO_UNPUT	// debian flex
#define YY_NO_YYINPUT
#define YY_NO_YYUNPUT

typedef struct cex_tok_s {
	int textlen;
	int token;
	CEXPR_YYSTYPE s;
} cex_tok_t;

#define YYSTYPE cex_tok_t
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

static exprval_t *parse_number (cex_tok_t *tok, exprctx_t *context);
static exprval_t *parse_vector (cex_tok_t *tok, yyscan_t scanner,
								exprctx_t *context);
static exprval_t *parse_int (const char *str, exprctx_t *context);
static exprval_t *parse_uint (const char *str, exprctx_t *context);
static exprval_t *parse_size_t (const char *str, exprctx_t *context);
static exprval_t *parse_float (const char *str, exprctx_t *context);
static exprval_t *parse_float (const char *str, exprctx_t *context);
static exprval_t *parse_double (const char *str, exprctx_t *context);
static exprval_t *parse_string (const char *str, exprctx_t *context);
static exprsym_t *parse_name (const char *str, exprctx_t *context);
static exprval_t *parse_variable (const char *str, exprctx_t *context);

typedef enum {
	cex_eof = 1,
	cex_number,
	cex_vector,
} cex_term;

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
		if (ctx->msg_prefix) {
			PL_Message (ctx->messages, ctx->item, "%s:%s",
						ctx->msg_prefix, string->str);
		} else {
			PL_Message (ctx->messages, ctx->item, "%s", string->str);
		}
	} else {
		if (ctx->msg_prefix) {
			Sys_Printf ("%s:%s\n", ctx->msg_prefix, string->str);
		} else {
			Sys_Printf ("%s\n", string->str);
		}
	}
	dstring_delete (string);
}

static void
save_text (cex_tok_t *tok, const char *text, size_t textlen, int state,
		   exprctx_t *context)
{
	tok->s.string = text;
	tok->textlen = textlen;
}

// for debian flex
#ifndef yystart
#define yystart() YY_START
#define yybuffer YY_BUFFER_STATE
#endif

#define YY_USER_ACTION save_text (yylval, yytext, yyleng, yystart(), context);

%}

s			[ \t]
m			[\-+]
D			[0-9]
IDs			[a-zA-Z_]
IDc			[a-zA-Z_0-9]
ID			{IDs}{IDc}*
id			{IDc}{IDc}*
FD			[fFdD]
UL			([uU]?([lL][lL]?)?)
ULFDZ		({UL}|{FD}|[zZ])
s_string	\"(\\.|[^"\\])*\"
c_string	'(\\.|[^'\\])*'

pp_number   \.?{D}({IDc}|'{IDc}|[eEpP]{m}|\.)*
pp_vnumber  '({s}*{m}?{pp_number}){2,4}{s}*'{ULFDZ}?

%x		VECTOR

%%
%{
	__auto_type context = yyget_extra (yyscanner);
%}

{pp_number}			{ return -cex_number; }
{pp_vnumber}		{ return -cex_vector; }
<VECTOR>{pp_number}	{ return -cex_number; }
<VECTOR>{m}			{ return yytext[0]; }
<VECTOR>'			{ return -cex_eof; }

{ID}(`{id})*		{
						yylval->s.symbol = parse_name (yytext, context);
						return NAME;
					}

`{IDc}+(`{id})*		{
						yylval->s.symbol = parse_name (yytext + 1, context);
						return NAME;
					}

\${ID}				{
						yylval->s.value = parse_variable (yytext + 1, context);
						return VALUE;
					}

{s_string}			{
						yylval->s.value = parse_string (yytext, context);
						return VALUE;
					}
@					{
						auto sym = parse_name ("@", context);
						if (sym) {
							yylval->s.symbol = sym;
							return NAME;
						}
						return '@';
					}

[+\-*/&|^%]=		{
						yylval->s.op = yytext[0];
						return ASX;
					}

"%%="				{
						yylval->s.op = MOD;
						return ASX;
					}

"<<="				{
						yylval->s.op = SHL;
						return ASX;
					}

">>="				{
						yylval->s.op = SHR;
						return ASX;
					}

[!(){}.*/&|^~+\-=\[\];,#%?:] {
						return yytext[0];
					}

"%%"				{
						return MOD;
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
		cex_tok_t   lval;
		int         token = yylex (&lval, scanner);
		if (token < 0) {
			cex_term term = -token;
			switch (term) {
				case cex_eof:
					break;
				case cex_number:
					lval.s.value = parse_number (&lval, context);
					token = VALUE;
					break;
				case cex_vector:
					lval.s.value = parse_vector (&lval, scanner, context);
					token = VALUE;
					break;
			}
		}
		status = cexpr_yypush_parse (ps, token, &lval.s, scanner, context);
	} while (status == YYPUSH_MORE);

	yylex_destroy (scanner);
	cexpr_yypstate_delete (ps);
	return status || context->errors;
	(void)yy_top_state;
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

static exprval_t *parse_size_t (const char *str, exprctx_t *context)
{
	exprval_t  *val = cexpr_value (&cexpr_size_t, context);
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

static exprval_t *parse_long (const char *str, exprctx_t *context)
{
	exprval_t  *val = cexpr_value (&cexpr_long, context);
	*(int64_t *) val->value = strtol (str, nullptr, 0);
	return val;
}

static exprval_t *parse_ulong (const char *str, exprctx_t *context)
{
	exprval_t  *val = cexpr_value (&cexpr_ulong, context);
	*(uint64_t *) val->value = strtoul (str, nullptr, 0);
	return val;
}

static exprval_t *parse_number (cex_tok_t *tok, exprctx_t *context)
{
	const char *str = tok->s.string;
	int len = 0;
	bool is_float = false;
	bool is_long = false;
	//FIXME is it worth worrying about too many . or mid-string type chars?
	for (const char *s = str; *s; s++, len++) {
		is_float |= *s == '.';
		is_long |= (*s | 0x20) == 'l';
	}
	// known to be at least one char
	// "tolower" is a no-op on digits or other number-related chars
	switch (str[len - 1] | 0x20) {
		case 'f': return parse_float (str, context);
		case 'd': return parse_double (str, context);
		case 'z': return parse_size_t (str, context);
		case 'u': return (is_long ? parse_ulong : parse_uint) (str, context);
	}
	if (is_float) {
		return parse_double (str, context);
	}
	return (is_long ? parse_long : parse_int) (str, context);
}

typedef struct {
	yyscan_t    scanner;
	yybuffer    buffer;
} buffer_raii_t;


static void
cexpr_restore_buffer (buffer_raii_t *raii)
{
	yy_switch_to_buffer (raii->buffer, raii->scanner);
}

static void
cexpr_delete_buffer (buffer_raii_t *raii)
{
	yy_delete_buffer (raii->buffer, raii->scanner);
}


static exprval_t *
parse_vector (cex_tok_t *tok, yyscan_t scanner, exprctx_t *context)
{
	char *text = cmemalloc (context->memsuper, tok->textlen + 1);
	strncpy (text, tok->s.string, tok->textlen);
	text[tok->textlen] = 0;

	const char *end = text + tok->textlen;
	while (end > text && *--end != '\'') continue;
	const char *start = text + 1;

	auto yyg = (struct yyguts_t *)scanner;
	auto __attribute__((cleanup (cexpr_restore_buffer)))
		saved_buffer = (buffer_raii_t) {
			.scanner = scanner,
			.buffer = YY_CURRENT_BUFFER,
		};
	auto __attribute__((cleanup (cexpr_delete_buffer)))
		buffer = (buffer_raii_t) {
			.scanner = scanner,
			.buffer = yy_scan_bytes (start, end - start, scanner),
		};

	cex_tok_t   vtok = {};
	exprval_t  *components[4+1] = {}; // max of 4
	bool        negate[4] = {};
	int         width = 0;
	int         token;
	static exprtype_t *ivec_types[] = {
		nullptr, nullptr,	// no 0 or 1 component vectors
		&cexpr_ivec2,
		&cexpr_ivec3,
		&cexpr_ivec4,
	};
	static exprtype_t *uvec_types[] = {
		nullptr, nullptr,	// no 0 or 1 component vectors
		&cexpr_uvec2,
		&cexpr_uvec3,
		&cexpr_uvec4,
	};
	static exprtype_t *vec_types[] = {
		nullptr, nullptr,	// no 0 or 1 component vectors
		&cexpr_vec2,
		&cexpr_vec3,
		&cexpr_vec4,
	};
	static exprtype_t *dvec_types[] = {
		nullptr, nullptr,	// no 0 or 1 component vectors
		&cexpr_dvec2,
		&cexpr_dvec3,
		&cexpr_dvec4,
	};
	static exprtype_t **types[] = {
		ivec_types,
		uvec_types,
		vec_types,
		dvec_types,
	};
	int type_ind = 0;//default to int

	yy_push_state (VECTOR, scanner);
	do {
		token = yylex (&vtok, scanner);
		if (-token == cex_number) {
			auto comp = parse_number (&vtok, context);
			if (comp) {
				if (width < 4) {
					components[width] = comp;
					int ind = 0;
					if (comp->type == &cexpr_int) {
						ind = 0;
					} else if (comp->type == &cexpr_uint) {
						ind = 1;
					} else if (comp->type == &cexpr_float) {
						ind = 2;
					} else if (comp->type == &cexpr_double) {
						ind = 3;
					} else {
						cexpr_error (context, "unsupported vector type: %s",
									 comp->type->name);
						return nullptr;
					}
					if (ind > type_ind) {
						type_ind = ind;
					}
				}
				width++;
			}
		} else if (token == '-') {
			if (width < 4) {
				negate[width] = true;
			}
		}
	} while (token && token != -cex_eof);
	yy_pop_state (scanner);

	if (width > 4) {
		cexpr_error (context, "too many components in vector literal");
		return nullptr;
	}
	bool is_long = false;
	bool is_float = false;
	bool is_double = false;
	bool is_unsigned = false;
	for (end++; *end; end++) {
		is_long |= (*end | 0x20) == 'l';
		is_float |= (*end | 0x20) == 'f';
		is_double |= (*end | 0x20) == 'd';
		is_unsigned |= (*end | 0x20) == 'u';
	}
	if (is_long) {
		cexpr_error (context, "long vectors not supported yet");
		return nullptr;
	}
	if (is_float) {
		type_ind = 2;
	} else if (is_unsigned) {
		type_ind = 1;
	} else if (is_double) {
		type_ind = 3;
	}
	exprval_t  *val = cexpr_value (types[type_ind][width], context);
	auto neg = cexpr_find_unop (((exprarray_t *) val->type->data)->type, '-');
	for (int i = 0; i < width; i++) {
		exprval_t *dst = nullptr;
		exprval_t dst_element = { .type = &cexpr_exprval, .value = &dst };
		cexpr_array_getindex (val, i, &dst_element, context);
		cexpr_assign_value (dst, components[i], context);
		if (negate[i]) {
			neg->func (dst, dst, context);
		}
	}
	return val;
}

static exprval_t *parse_string (const char *str, exprctx_t *context)
{
	exprval_t  *val = cexpr_value (&cexpr_string, context);
	// str includes the quotes, which add 2 to the length of the string, but
	// only one byte is needed for the terminating nul
	size_t      size = strlen (str) - 1;
	char       *dup = cmemalloc (context->memsuper, size);
	strncpy (dup, str + 1, size - 1);
	dup[size - 1] = 0;
	*(char **) val->value = dup;
	return val;
}

static exprsym_t *
parse_name (const char *name, exprctx_t *context)
{
	exprsym_t  *sym = 0;
	exprtab_t  *prev_tab = 0;

	for (exprctx_t *ctx = context; ctx && !sym; ctx = ctx->parent) {
		exprtab_t  *symtab = ctx->symtab;
		if (!symtab) {
			// scope barrier
			break;
		}
		if (symtab == prev_tab) {
			// already checked this symtab
			continue;
		}
		prev_tab = symtab;

		if (symtab->tab) {
			sym = Hash_Find (symtab->tab, name);
		} else {
			for (exprsym_t *s = symtab->symbols; s->name; s++) {
				if (!strcmp (s->name, name)) {
					sym = s;
					break;
				}
			}
		}
	}
	if (!sym) {
		sym = cmemalloc (context->memsuper, sizeof (exprsym_t));
		size_t      size = strlen (name) + 1;
		char       *sym_name = cmemalloc (context->memsuper, size);
		memcpy (sym_name, name, size);
		*sym = (exprsym_t) { .name = sym_name };
	}
	return sym;
}

static exprval_t *
parse_variable (const char *name, exprctx_t *context)
{
	exprval_t  *val = 0;
	if (strcmp (name, "cvars") == 0) {
		val = cexpr_cvar_struct (context);
	} else {
		exprsym_t  *sym = 0;
		exprtab_t  *prev_tab = 0;
		for (exprctx_t *ctx = context; ctx && !sym; ctx = ctx->parent) {
			exprtab_t  *symtab = ctx->external_variables;
			if (!symtab) {
				// scope barrier
				break;
			}
			if (symtab == prev_tab) {
				// already checked this symtab
				continue;
			}
			sym = Hash_Find (symtab->tab, name);
		}
		if (sym) {
			val = cexpr_value_reference (sym->type, sym->value, context);
		}
	}
	if (!val) {
		cexpr_error (context, "undefined variable %s", name);
	}
	return val;
}
