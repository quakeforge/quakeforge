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

#include "QF/dstring.h"

#include "specifier.h"

typedef struct keyword_s {
	const char *name;
	int         value;
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

typedef struct expr_s expr_t;
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
} funcstate_t;

typedef union rua_val_s {
	int			op;
	unsigned    size;
	specifier_t spec;
	void       *pointer;			// for ensuring pointer values are null
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
} rua_val_t;

#include "tools/qfcc/source/qc-parse.h"
#include "tools/qfcc/source/glsl-parse.h"

typedef struct rua_macro_s rua_macro_t;
typedef void (*rua_macro_f) (rua_macro_t *macro, void *scanner);

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

rua_macro_t *rua_start_macro (const char *name, bool params, void *scanner);
rua_macro_t *rua_macro_param (rua_macro_t *macro, const rua_tok_t *token,
							  void *scanner);
rua_macro_t *rua_end_params (rua_macro_t *macro, void *scanner);
rua_macro_t *rua_macro_append (rua_macro_t *macro, const rua_tok_t *token,
							   void *scanner);
void rua_macro_finish (rua_macro_t *macro, void *scanner);
rua_macro_t *rua_macro_arg (const rua_tok_t *token, void *scanner);
void rua_start_pragma (void *scanner);
void rua_start_text (void *scanner);
void rua_start_expr (void *scanner);
void rua_start_include (void *scanner);
void rua_expand_on (void *scanner);
void rua_expand_off (void *scanner);
void rua_end_directive (void *scanner);
void rua_start_if (bool expand, void *scanner);
void rua_start_else (bool expand, void *scanner);
void rua_if (bool pass, void *scanner);
void rua_else (bool pass, const char *tok, void *scanner);
void rua_endif (void *scanner);
bool rua_defined (const char *sym, void *scanner);
void rua_undefine (const char *sym, void *scanner);
void rua_include_file (const char *name, void *scanner);
void rua_embed_file (const char *name, void *scanner);
int rua_parse_define (const char *def);
void rua_line_info (const expr_t *line_expr, const char *text,
					const expr_t *flags_epxr, void *scanner);

void rua_macro_file (rua_macro_t *macro, void *scanner);
void rua_macro_line (rua_macro_t *macro, void *scanner);
void rua_macro_va_opt (rua_macro_t *macro, void *scanner);
void rua_macro_va_args (rua_macro_t *macro, void *scanner);

#include "tools/qfcc/source/pre-parse.h"

typedef struct rua_parser_s {
	int (*parse) (rua_yypstate *state, int token,
				  const rua_val_t *val, rua_loc_t *loc, void *scanner);
	rua_yypstate *state;
	directive_t *(*directive) (const char *token);
	int (*keyword_or_id) (rua_val_t *lval, const char *token);
} rua_parser_t;

int rua_parse (FILE *in, rua_parser_t *parser);
int rua_parse_string (const char *str, rua_parser_t *parser);
const char *rua_directive_get_key (const void *dir, void *unused) __attribute__((pure));
const char *rua_keyword_get_key (const void *dir, void *unused) __attribute__((pure));

typedef struct language_s {
	void      (*init) (void);
	int       (*parse) (FILE *in);
	int       (*finish) (const char *file);
} language_t;

extern language_t lang_ruamoko;
extern language_t lang_pascal;

#endif//__rua_lang_h
