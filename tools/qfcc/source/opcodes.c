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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include <QF/hash.h>

#include "def.h"
#include "opcodes.h"
#include "options.h"
#include "qfcc.h"
#include "type.h"

hashtab_t  *opcode_type_table;

opcode_t   *op_done;
opcode_t   *op_return;
opcode_t   *op_return_v;
opcode_t   *op_if;
opcode_t   *op_ifnot;
opcode_t   *op_ifbe;
opcode_t   *op_ifb;
opcode_t   *op_ifae;
opcode_t   *op_ifa;
opcode_t   *op_state;
opcode_t   *op_state_f;
opcode_t   *op_goto;
opcode_t   *op_jump;
opcode_t   *op_jumpb;

#define ROTL(x,n) (((x)<<(n))|(x)>>(32-n))

static uintptr_t
get_hash (void *_op, void *_tab)
{
	opcode_t   *op = (opcode_t *) _op;
	uintptr_t   hash;

	hash = ROTL (~op->type_a, 8) + ROTL (~op->type_b, 16)
		+ ROTL (~op->type_c, 24);
	return hash + Hash_String (op->name);
}

static int
compare (void *_opa, void *_opb, void *_tab)
{
	opcode_t   *opa = (opcode_t *) _opa;
	opcode_t   *opb = (opcode_t *) _opb;
	int         cmp;

	cmp = (opa->type_a == opb->type_a)
		  && (opa->type_b == opb->type_b)
		  && (opa->type_c == opb->type_c);
	return cmp && !strcmp (opa->name, opb->name);
}

opcode_t *
opcode_find (const char *name, def_t *def_a, def_t *def_b, def_t *def_c)
{
	opcode_t    op;

	op.name = name;
	op.type_a = def_a ? def_a->type->type : ev_invalid;
	op.type_b = def_b ? def_b->type->type : ev_invalid;
	op.type_c = def_c ? def_c->type->type : ev_invalid;
	return Hash_FindElement (opcode_type_table, &op);
}

void
opcode_init (void)
{
	opcode_t   *op;

	PR_Opcode_Init ();
	opcode_type_table = Hash_NewTable (1021, 0, 0, 0);
	Hash_SetHashCompare (opcode_type_table, get_hash, compare);
	for (op = pr_opcodes; op->name; op++) {
		if (op->min_version > options.code.progsversion)
			continue;
		if (options.code.progsversion == PROG_ID_VERSION
			&& op->type_c == ev_integer)
			op->type_c = ev_float;
		Hash_AddElement (opcode_type_table, op);
		if (!strcmp (op->name, "<DONE>")) {
			op_done = op;
		} else if (!strcmp (op->name, "<RETURN>")) {
			op_return = op;
		} else if (!strcmp (op->name, "<RETURN_V>")) {
			op_return_v = op;
		} else if (!strcmp (op->name, "<IF>")) {
			op_if = op;
		} else if (!strcmp (op->name, "<IFNOT>")) {
			op_ifnot = op;
		} else if (!strcmp (op->name, "<IFBE>")) {
			op_ifbe = op;
		} else if (!strcmp (op->name, "<IFB>")) {
			op_ifb = op;
		} else if (!strcmp (op->name, "<IFAE>")) {
			op_ifae = op;
		} else if (!strcmp (op->name, "<IFA>")) {
			op_ifa = op;
		} else if (!strcmp (op->name, "<STATE>")) {
			if (op->type_c == ev_float)
				op_state_f = op;
			else
				op_state = op;
		} else if (!strcmp (op->name, "<GOTO>")) {
			op_goto = op;
		} else if (!strcmp (op->name, "<JUMP>")) {
			op_jump = op;
		} else if (!strcmp (op->name, "<JUMPB>")) {
			op_jumpb = op;
		}
	}
}
