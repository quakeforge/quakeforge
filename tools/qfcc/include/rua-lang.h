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

typedef struct rua_tok_s {
	rua_loc_t   location;
	int         textlen;
	const char *text;
	union {
		struct {
			void       *pointer;	// mirrors pointer in QC_YYSTYPE
			char        str_text[8];// if len < 8 and spec not used
		};
		QC_YYSTYPE      value;
	};
} rua_tok_t;

#endif//__rua_lang_h
