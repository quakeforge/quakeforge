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

#include "QF/progs/pr_comp.h"

typedef enum {
	op_def,
	op_value,
	op_label,
	op_temp,
	op_alias,
	op_nil,
	op_pseudo,
} op_type_e;

struct expr_s;

typedef struct tempop_s {
	struct def_s   *def;
	int             offset;
	struct type_s *type;
	struct flowvar_s *flowvar;
	struct daglabel_s *daglabel;
	struct operand_s *alias;
	struct operand_s *alias_ops;
	int         users;
	int         flowaddr;		///< "address" of temp in flow analysis, != 0
} tempop_t;

typedef struct pseudoop_s {
	struct pseudoop_s *next;
	const char *name;
	struct flowvar_s *flowvar;
	void      (*uninitialized) (struct expr_s *expr, struct pseudoop_s *op);
} pseudoop_t;

typedef struct operand_s {
	struct operand_s *next;
	op_type_e   op_type;
	struct type_s *type;		///< possibly override def's/nil's type
	int         size;			///< for structures
	int         width;			///< for SIMD selection
	struct expr_s *expr;		///< expression generating this operand
	void       *return_addr;	///< who created this operand
	union {
		struct def_s *def;
		struct ex_value_s *value;
		struct ex_label_s *label;
		tempop_t    tempop;
		struct operand_s *alias;
		pseudoop_t *pseudoop;
	};
} operand_t;

/** Overall type of statement.

	Statement types are broken down into expressions (binary and unary,
	includes address and pointer dereferencing (read)), assignment, pointer
	assignment (write to dereference pointer), move (special case of
	assignment), pointer move (special case of pointer assignment), state,
	function related (call, rcall, return and done), and flow control
	(conditional branches, goto, jump (single pointer and jump table)).
*/
typedef enum {
	st_none,		///< not a (valid) statement. Used in dags.
	st_expr,		///< c = a op b; or c = op a;
	st_assign,		///< b = a
	st_ptrassign,	///< *b = a; or *(b + c) = a;
	st_move,		///< memcpy (c, a, b); c and a are direct def references
	st_ptrmove,		///< memcpy (c, a, b); c and a are pointers
	st_memset,		///< memset (c, a, b); c is direct def reference
	st_ptrmemset,	///< memset (c, a, b); c is pointer
	st_state,		///< state (a, b); or state (a, b, c)
	st_func,		///< call, rcall or return/done
	st_flow,		///< if/ifa/ifae/ifb/ifbe/ifnot or goto or jump/jumpb
} st_type_t;

typedef struct statement_s {
	struct statement_s *next;
	st_type_t   type;
	const char *opcode;
	operand_t  *opa;
	operand_t  *opb;
	operand_t  *opc;
	struct expr_s *expr;		///< source expression for this statement
	int         number;			///< number of this statement in function
	operand_t  *use;			///< list of auxiliary operands used
	operand_t  *def;			///< list of auxiliary operands defined
	operand_t  *kill;			///< list of auxiliary operands killed
} statement_t;

typedef struct sblock_s {
	struct sblock_s *next;
	struct reloc_s *relocs;
	struct ex_label_s *labels;
	struct flownode_s *flownode;///< flow node for this block
	int         offset;			///< offset of first statement of block
	int         reachable;
	int         number;			///< number of this block in flow graph
	statement_t *statements;
	statement_t **tail;
} sblock_t;

struct expr_s;
struct type_s;
struct dstring_s;

extern const char *op_type_names[];
extern const char *st_type_names[];

const char *optype_str (op_type_e type) __attribute__((const));

operand_t *nil_operand (struct type_s *type, struct expr_s *expr);
operand_t *def_operand (struct def_s *def, struct type_s *type,
						struct expr_s *expr);
operand_t *return_operand (struct type_s *type, struct expr_s *expr);
operand_t *value_operand (struct ex_value_s *value, struct expr_s *expr);
int tempop_overlap (tempop_t *t1, tempop_t *t2) __attribute__((pure));
operand_t *temp_operand (struct type_s *type, struct expr_s *expr);
int tempop_visit_all (tempop_t *tempop, int overlap,
					  int (*visit) (tempop_t *, void *), void *data);
operand_t *alias_operand (struct type_s *type, operand_t *op,
						  struct expr_s *expr);
operand_t *label_operand (struct expr_s *label);
void free_operand (operand_t *op);

sblock_t *new_sblock (void);
statement_t *new_statement (st_type_t type, const char *opcode,
							struct expr_s *expr);
int statement_is_cond (statement_t *s) __attribute__((pure));
int statement_is_goto (statement_t *s) __attribute__((pure));
int statement_is_jumpb (statement_t *s) __attribute__((pure));
int statement_is_call (statement_t *s) __attribute__((pure));
int statement_is_return (statement_t *s) __attribute__((pure));
sblock_t *statement_get_target (statement_t *s) __attribute__((pure));
sblock_t **statement_get_targetlist (statement_t *s);
void sblock_add_statement (sblock_t *sblock, statement_t *statement);
sblock_t *make_statements (struct expr_s *expr);
void statements_count_temps (sblock_t *sblock);

void print_operand (operand_t *op);
void print_statement (statement_t *s);
void dump_dot_sblock (void *data, const char *fname);
void dot_sblock (struct dstring_s *dstr, sblock_t *sblock, int blockno);
void print_sblock (sblock_t *sblock, const char *filename);
const char *operand_string (operand_t *op);

#endif//statement_h
