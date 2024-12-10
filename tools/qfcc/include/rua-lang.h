/*
	rua-lang.h

	Shared data stuctures for lexing and parsing.

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

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

#ifndef __rua_lang_h
#define __rua_lang_h

#include <stdio.h>

#include "QF/dstring.h"

#include "specifier.h"

typedef struct keyword_s {
	const char *name;
	int         value;
	bool        use_name;
	specifier_t spec;
} keyword_t;

typedef struct directive_s {
	const char *name;
	int         value;
} directive_t;

typedef struct rua_loc_s {
	int         line, column;
	int         last_line, last_column;
	int         file;
} rua_loc_t;

#define RUA_LOC_DEFAULT(Current, Rhs, N) \
	do if (N) { \
		(Current).line         = YYRHSLOC (Rhs, 1).line;        \
		(Current).column       = YYRHSLOC (Rhs, 1).column;      \
		(Current).last_line    = YYRHSLOC (Rhs, N).last_line;   \
		(Current).last_column  = YYRHSLOC (Rhs, N).last_column; \
		(Current).file         = YYRHSLOC (Rhs, N).file;        \
	} else {                                                    \
		(Current).line         = (Current).last_line   =        \
		  YYRHSLOC (Rhs, 0).last_line;                          \
		(Current).column       = (Current).last_column =        \
		  YYRHSLOC (Rhs, 0).last_column;                        \
		(Current).file         = YYRHSLOC (Rhs, 0).file;        \
	} while (0)

typedef struct rua_ctx_s rua_ctx_t;
typedef struct expr_s expr_t;
typedef struct symbol_s symbol_t;
typedef struct symtab_s symtab_t;
typedef struct function_s function_t;

typedef struct rua_tok_s {
	struct rua_tok_s *next;
	rua_loc_t   location;
	int         textlen;
	int         token;
	const char *text;
} rua_tok_t;

typedef struct {
	symtab_t   *symtab;
	function_t *function;
	specifier_t spec;
} funcstate_t;

typedef union rua_val_s {
	int			op;
	unsigned    size;
	specifier_t spec;
	void       *pointer;			// for ensuring pointer values are null
	const char *string;
	funcstate_t funcstate;
	const struct type_s	*type;
	const struct expr_s	*expr;
	struct expr_s *mut_expr;
	struct element_s *element;
	struct function_s *function;
	struct switch_block_s *switch_block;
	struct param_s	*param;
	struct method_s	*method;
	struct class_s	*class;
	struct category_s *category;
	struct class_type_s	*class_type;
	struct protocol_s *protocol;
	struct protocollist_s *protocol_list;
	struct keywordarg_s *keywordarg;
	struct methodlist_s *methodlist;
	struct symbol_s *symbol;
	struct symtab_s *symtab;
	struct attribute_s *attribute;
	struct designator_s *designator;

	struct glsl_block_s *block;
} rua_val_t;

#include "tools/qfcc/source/qc-parse.h"
#include "tools/qfcc/source/glsl-parse.h"

typedef struct rua_macro_s rua_macro_t;
typedef void (*rua_macro_f) (rua_macro_t *macro, rua_ctx_t *ctx);

typedef struct rua_macro_s {
	rua_macro_t *next;
	const char *name;
	symtab_t   *params;
	rua_tok_t  *tokens;
	rua_tok_t **tail;
	int         num_tokens;
	int         num_params;
	rua_tok_t  *cursor;
	rua_macro_f update;

	int         num_args;
	rua_macro_t **args;
} rua_macro_t;

typedef struct rua_preval_s {
	rua_tok_t   t;
	union {
		const expr_t *expr;
		dstring_t  *dstr;
		rua_macro_t *macro;
	};
} rua_preval_t;

rua_macro_t *rua_start_macro (const char *name, bool params, rua_ctx_t *ctx);
rua_macro_t *rua_macro_param (rua_macro_t *macro, const rua_tok_t *token,
							  rua_ctx_t *ctx);
rua_macro_t *rua_end_params (rua_macro_t *macro, rua_ctx_t *ctx);
rua_macro_t *rua_macro_append (rua_macro_t *macro, const rua_tok_t *token,
							   rua_ctx_t *ctx);
void rua_macro_finish (rua_macro_t *macro, rua_ctx_t *ctx);
rua_macro_t *rua_macro_arg (const rua_tok_t *token, rua_ctx_t *ctx);
void rua_start_pragma (rua_ctx_t *ctx);
void rua_start_text (rua_ctx_t *ctx);
void rua_start_expr (rua_ctx_t *ctx);
void rua_start_include (rua_ctx_t *ctx);
void rua_expand_on (rua_ctx_t *ctx);
void rua_expand_off (rua_ctx_t *ctx);
void rua_end_directive (rua_ctx_t *ctx);
void rua_start_if (bool expand, rua_ctx_t *ctx);
void rua_start_else (bool expand, rua_ctx_t *ctx);
void rua_if (bool pass, rua_ctx_t *ctx);
void rua_else (bool pass, const char *tok, rua_ctx_t *ctx);
void rua_endif (rua_ctx_t *ctx);
bool rua_defined (const char *sym, rua_ctx_t *ctx);
void rua_undefine (const char *sym, rua_ctx_t *ctx);
void rua_include_file (const char *name, rua_ctx_t *ctx);
void rua_embed_file (const char *name, rua_ctx_t *ctx);
int rua_parse_define (const char *def);
void rua_line_info (const expr_t *line_expr, const char *text,
					const expr_t *flags_epxr, rua_ctx_t *ctx);

void rua_macro_file (rua_macro_t *macro, rua_ctx_t *ctx);
void rua_macro_line (rua_macro_t *macro, rua_ctx_t *ctx);
void rua_macro_va_opt (rua_macro_t *macro, rua_ctx_t *ctx);
void rua_macro_va_args (rua_macro_t *macro, rua_ctx_t *ctx);

void rua_print_location (FILE *out, const rua_loc_t *loc);

#include "tools/qfcc/source/pre-parse.h"

typedef struct rua_parser_s {
	int (*parse) (rua_yypstate *state, int token,
				  const rua_val_t *val, rua_loc_t *loc, rua_ctx_t *ctx);
	rua_yypstate *state;
	directive_t *(*directive) (const char *token);
	int (*keyword_or_id) (rua_val_t *lval, const char *token, rua_ctx_t *ctx);
} rua_parser_t;

int rua_parse (FILE *in, rua_parser_t *parser, rua_ctx_t *ctx);
int rua_parse_string (const char *str, rua_parser_t *parser, rua_ctx_t *ctx);
const char *rua_directive_get_key (const void *dir, void *unused) __attribute__((pure));
const char *rua_keyword_get_key (const void *dir, void *unused) __attribute__((pure));

typedef struct language_s {
	bool        initialized;
	bool        always_overload;
	void      (*init) (rua_ctx_t *ctx);
	int       (*parse) (FILE *in, rua_ctx_t *ctx);
	int       (*finish) (const char *file, rua_ctx_t *ctx);
	void      (*extension) (const char *name, const char *value,
							rua_ctx_t *ctx);
	void      (*version) (int version, const char *profile, rua_ctx_t *ctx);
	bool      (*on_include) (const char *name, rua_ctx_t *ctx);
	void      (*parse_declaration) (specifier_t spec, symbol_t *sym,
									const expr_t *init, symtab_t *symtab,
									expr_t *block, rua_ctx_t *ctx);
	void       *sublanguage;
} language_t;

typedef struct rua_ctx_s {
	struct rua_extra_s *extra;
	void       *scanner;
	language_t *language;
} rua_ctx_t;

extern language_t lang_ruamoko;
extern language_t lang_pascal;

int qc_parse_string (const char *str, rua_ctx_t *ctx);

void rua_parse_declaration (specifier_t spec, symbol_t *sym,
							const expr_t *init, symtab_t *symtab,
							expr_t *block, rua_ctx_t *ctx);

#endif//__rua_lang_h
