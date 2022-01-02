/*
	opcodes.c

	opcode searching

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/06/01

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

#include <QF/hash.h>

#include "tools/qfcc/include/opcodes.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/type.h"

hashtab_t  *opcode_type_table;
hashtab_t  *opcode_void_table;
v6p_opcode_t *opcode_map;

#define ROTL(x,n) ((((unsigned)(x))<<(n))|((unsigned)(x))>>(32-n))

static uintptr_t
get_hash (const void *_op, void *_tab)
{
	v6p_opcode_t *op = (v6p_opcode_t *) _op;
	uintptr_t   hash;

	hash = ROTL (~op->type_a, 8) + ROTL (~op->type_b, 16)
		+ ROTL (~op->type_c, 24);
	return hash + Hash_String (op->name);
}

static int
compare (const void *_opa, const void *_opb, void *unused)
{
	v6p_opcode_t *opa = (v6p_opcode_t *) _opa;
	v6p_opcode_t *opb = (v6p_opcode_t *) _opb;
	int         cmp;

	cmp = (opa->type_a == opb->type_a)
		  && (opa->type_b == opb->type_b)
		  && (opa->type_c == opb->type_c);
	return cmp && !strcmp (opa->name, opb->name);
}

static const char *
get_key (const void *op, void *unused)
{
	return ((v6p_opcode_t *) op)->name;
}

static int
check_operand_type (etype_t ot1, etype_t ot2)
{
	if ((ot1 == ev_void && ot2 != ev_invalid)
		|| ot1 == ot2)
		return 1;
	return 0;
}

v6p_opcode_t *
opcode_find (const char *name, operand_t *op_a, operand_t *op_b,
			 operand_t *op_c)
{
	v6p_opcode_t search_op = {};
	v6p_opcode_t *op;
	v6p_opcode_t *sop;
	void      **op_list;
	int         i;

	search_op.name = name;
	search_op.type_a = op_a ? low_level_type (op_a->type) : ev_invalid;
	search_op.type_b = op_b ? low_level_type (op_b->type) : ev_invalid;
	search_op.type_c = op_c ? low_level_type (op_c->type) : ev_invalid;
	op = Hash_FindElement (opcode_type_table, &search_op);
	if (op)
		return op;
	op_list = Hash_FindList (opcode_void_table, name);
	if (!op_list)
		return op;
	for (i = 0; !op && op_list[i]; i++) {
		sop = op_list[i];
		if (check_operand_type (sop->type_a, search_op.type_a)
			&& check_operand_type (sop->type_b, search_op.type_b)
			&& check_operand_type (sop->type_c, search_op.type_c))
			op = sop;
	}
	free (op_list);
	return op;
}

void
opcode_init (void)
{
	const v6p_opcode_t *op;
	v6p_opcode_t *mop;

	if (opcode_type_table) {
		Hash_FlushTable (opcode_void_table);
		Hash_FlushTable (opcode_type_table);
	} else {
		PR_Opcode_Init ();
		opcode_type_table = Hash_NewTable (1021, 0, 0, 0, 0);
		Hash_SetHashCompare (opcode_type_table, get_hash, compare);
		opcode_void_table = Hash_NewTable (1021, get_key, 0, 0, 0);
	}

	int         num_opcodes = 0;
	for (op = pr_v6p_opcodes; op->name; op++) {
		num_opcodes++;
	}
	if (!opcode_map) {
		opcode_map = calloc (num_opcodes, sizeof (v6p_opcode_t));
	}
	for (int i = 0; i < num_opcodes; i++) {
		op = pr_v6p_opcodes + i;
		if (op->min_version > options.code.progsversion)
			continue;
		mop = opcode_map + i;
		*mop = *op;
		if (options.code.progsversion == PROG_ID_VERSION) {
			// v6 progs have no concept of integer, but the QF engine
			// treats the operands of certain operands as integers
			// irrespective the progs version, so convert the engine's
			// view of the operands to the prog's view.
			if (mop->type_a == ev_integer)
				mop->type_a = ev_float;
			if (mop->type_b == ev_integer)
				mop->type_b = ev_float;
			if (mop->type_c == ev_integer)
				mop->type_c = ev_float;
		}
		Hash_AddElement (opcode_type_table, mop);
		if (mop->type_a == ev_void || mop->type_b == ev_void
			|| mop->type_c == ev_void)
			Hash_Add (opcode_void_table, mop);
	}
}
