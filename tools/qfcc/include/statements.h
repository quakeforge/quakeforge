/*
	statement.h

	Internal statements

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/18

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
#ifndef statement_h
#define statement_h

#include "QF/pr_comp.h"

typedef enum {
	op_symbol,
	op_value,
	op_label,
	op_temp,
	op_pointer,
	op_alias,
} op_type_e;

typedef struct operand_s {
	struct operand_s *next;
	op_type_e   op_type;
	etype_t     type;			///< possibly override symbol's type
	int         size;			///< for structures
	union {
		struct symbol_s *symbol;
		struct ex_value_s *value;
		struct ex_label_s *label;
		struct ex_pointer_s *pointer;
		struct def_s   *def;
		struct operand_s *alias;
	} o;
} operand_t;

typedef struct statement_s {
	struct statement_s *next;
	const char *opcode;
	operand_t  *opa;
	operand_t  *opb;
	operand_t  *opc;
	struct expr_s *expr;	///< source expression for this statement
} statement_t;

typedef struct sblock_s {
	struct sblock_s *next;
	struct reloc_s *relocs;
	struct ex_label_s *labels;
	int         offset;			///< offset of first statement of block
	int         reachable;
	statement_t *statements;
	statement_t **tail;
} sblock_t;

struct expr_s;

sblock_t *make_statements (struct expr_s *expr);
void print_statement (statement_t *s);
void print_flow (sblock_t *sblock, const char *filename);
const char *operand_string (operand_t *op);

#endif//statement_h
