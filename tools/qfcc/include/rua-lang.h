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

#include "tools/qfcc/source/qc-parse.h"

typedef struct rua_loc_s {
	int         first_line, first_column;
	int         last_line, last_column;
	int         file;
} rua_loc_t;

typedef struct symtab_s symtab_t;

typedef struct rua_expr_s {
	struct rua_expr_s *next;
	rua_loc_t   location;
	int         textlen;
	int         token;
	int         id;			// arg expansion id (1...)
	const char *text;
} rua_expr_t;

typedef struct rua_macro_s {
	const char *name;
	symtab_t   *params;
	rua_expr_t *tokens;
	rua_expr_t **tail;
	int         num_tokens;
	int         num_params;
} rua_macro_t;

typedef struct rua_tok_s {
	rua_loc_t   location;
	int         textlen;
	const char *text;
	union {
		struct {
			void       *pointer;	// mirrors pointer in QC_YYSTYPE
			char        str_text[8];// if len < 8 and spec not used
			int         token;		// when recording macros
		};
		QC_YYSTYPE      value;
		struct dstring_s *dstr;
		rua_macro_t    *macro;
	};
} rua_tok_t;

rua_macro_t *rua_start_macro (const char *name, bool params, void *scanner);
rua_macro_t *rua_macro_param (rua_macro_t *macro, rua_tok_t *token,
							  void *scanner);
rua_macro_t *rua_end_params (rua_macro_t *macro, void *scanner);
rua_macro_t *rua_macro_append (rua_macro_t *macro, rua_tok_t *token,
							   void *scanner);
void rua_macro_finish (rua_macro_t *macro, void *scanner);
rua_macro_t *rua_macro_arg (rua_macro_t *arg, void *scanner);
void rua_start_pragma (void *scanner);
void rua_start_text (void *scanner);
void rua_start_expr (void *scanner);
void rua_start_include (void *scanner);
void rua_expand_on (void *scanner);
void rua_expand_off (void *scanner);
void rua_end_directive (void *scanner);
void rua_start_if (bool expand, void *scanner);
void rua_if (bool pass, void *scanner);
void rua_else (bool pass, const char *tok, void *scanner);
void rua_endif (void *scanner);
bool rua_defined (const char *sym, void *scanner);
void rua_undefine (const char *sym, void *scanner);
void rua_include_file (const char *name, void *scanner);
void rua_embed_file (const char *name, void *scanner);
int rua_parse_define (const char *def);

#include "tools/qfcc/source/pre-parse.h"

#endif//__rua_lang_h
