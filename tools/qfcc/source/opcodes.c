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

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/opcodes.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/type.h"

typedef struct v6p_uint_opcode_s {
	pr_opcode_v6p_e op;
	v6p_opcode_t opcode;
} v6p_uint_opcode_t;

static v6p_uint_opcode_t v6p_uint_opcodes[] = {
	{OP_LOAD_I_v6p,    {"load",   "load.i",   ev_entity, ev_field, ev_uint }},
	{OP_LOADBI_I_v6p,  {"load",   "loadbi.i", ev_ptr,  ev_short,   ev_uint }},
	{OP_ADDRESS_I_v6p, {"lea",    "address.i",ev_uint, ev_invalid, ev_ptr }},
	{OP_STORE_I_v6p,   {"assign", "store.i",  ev_uint, ev_uint, ev_invalid }},
	{OP_STOREP_I_v6p,  {"store",  "storep.i", ev_uint, ev_ptr,  ev_invalid }},
	{OP_STOREB_I_v6p,  {"store",  "storeb.i", ev_uint, ev_ptr,  ev_int }},
	{OP_STOREBI_I_v6p, {"store",  "storebi.i",ev_uint, ev_ptr,  ev_short }},
	{OP_IF_v6p,        {"ifnz",   "if",       ev_uint, ev_short, ev_invalid }},
	{OP_IFNOT_v6p,     {"ifz",    "ifnot",    ev_uint, ev_short, ev_invalid }},
	{OP_ADD_I_v6p,     {"add",    "add.i",    ev_uint, ev_uint, ev_uint }},
	{OP_SUB_I_v6p,     {"sub",    "sub.i",    ev_uint, ev_uint, ev_uint }},
	{OP_MUL_I_v6p,     {"mul",    "mul.i",    ev_uint, ev_uint, ev_uint }},
	{OP_DIV_I_v6p,     {"div",    "div.i",    ev_uint, ev_uint, ev_uint }},
	{OP_BITAND_I_v6p,  {"bitand", "bitand.i", ev_uint, ev_uint, ev_uint }},
	{OP_BITOR_I_v6p,   {"bitor",  "bitor.i",  ev_uint, ev_uint, ev_uint }},
	{OP_BITXOR_I_v6p,  {"bitxor", "bitxor.i", ev_uint, ev_uint, ev_uint }},
	{OP_REM_I_v6p,     {"rem",    "rem.i",    ev_uint, ev_uint, ev_uint }},
	{OP_MOD_I_v6p,     {"mod",    "mod.i",    ev_uint, ev_uint, ev_uint }},
	{OP_SHL_I_v6p,     {"shl",    "shl.i",    ev_uint, ev_uint, ev_uint }},
	{OP_BITNOT_I_v6p,  {"bitnot", "bitnot.i", ev_uint, ev_invalid, ev_int }},
	{}
};

static hashtab_t  *v6p_opcode_type_table;
static hashtab_t  *v6p_opcode_void_table;
static hashtab_t  *v6p_opcode_uint_table;
static v6p_opcode_t *v6p_opcode_map;

static hashtab_t  *rua_opcode_type_table;
static hashtab_t  *rua_opcode_void_table;

#define ROTL(x,n) ((((unsigned)(x))<<(n))|((unsigned)(x))>>(32-n))

static uintptr_t
v6p_get_hash (const void *_op, void *_tab)
{
	v6p_opcode_t *op = (v6p_opcode_t *) _op;
	uintptr_t   hash;

	hash = ROTL (~op->type_a, 8) + ROTL (~op->type_b, 16)
		+ ROTL (~op->type_c, 24);
	return hash + Hash_String (op->name);
}

static int
v6p_compare (const void *_opa, const void *_opb, void *unused)
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
v6p_get_key (const void *op, void *unused)
{
	return ((v6p_opcode_t *) op)->name;
}

static uintptr_t
v6p_uint_get_hash (const void *_op, void *_tab)
{
	__auto_type uint_op = (v6p_uint_opcode_t *) _op;
	return v6p_get_hash (&uint_op->opcode, _tab);
}

static int
v6p_uint_compare (const void *_opa, const void *_opb, void *data)
{
	__auto_type uint_opa = (v6p_uint_opcode_t *) _opa;
	__auto_type uint_opb = (v6p_uint_opcode_t *) _opb;
	return v6p_compare (&uint_opa->opcode, &uint_opb->opcode, data);
}

static uintptr_t
rua_get_hash (const void *_op, void *_tab)
{
	opcode_t   *op = (opcode_t *) _op;
	uintptr_t   hash;

	hash = ROTL (~op->types[0], 8) + ROTL (~op->types[1], 16)
		+ ROTL (~op->types[2], 24);
	hash += ROTL (~op->widths[0], 12) + ROTL (~op->widths[1], 20)
		+ ROTL (~op->widths[2], 28);
	return hash + Hash_String (op->opname);
}

static int
rua_compare (const void *_opa, const void *_opb, void *unused)
{
	opcode_t   *opa = (opcode_t *) _opa;
	opcode_t   *opb = (opcode_t *) _opb;
	int         cmp;

	cmp = (opa->types[0] == opb->types[0])
		  && (opa->types[1] == opb->types[1])
		  && (opa->types[2] == opb->types[2]);
	cmp &= (opa->widths[0] == opb->widths[0])
		  && (opa->widths[1] == opb->widths[1])
		  && (opa->widths[2] == opb->widths[2]);
	return cmp && !strcmp (opa->opname, opb->opname);
}

static const char *
rua_get_key (const void *op, void *unused)
{
	return ((opcode_t *) op)->opname;
}

static int
check_operand_type (etype_t ot1, etype_t ot2)
{
	if ((ot1 == ev_void && ot2 != ev_invalid)
		|| ot1 == ot2)
		return 1;
	return 0;
}

static int
check_operand_width (int ow1, int ow2)
{
	return (ow1 == -1 || ow1 == ow2);
}

pr_ushort_t
opcode_get (instruction_t *op)
{
	if (options.code.progsversion < PROG_VERSION) {
		return (v6p_opcode_t *) op - v6p_opcode_map;
	} else {
		return (opcode_t *) op - pr_opcodes;
	}
}

static v6p_opcode_t *
v6p_opcode_find (const char *name, operand_t *op_a, operand_t *op_b,
				 operand_t *op_c)
{
	v6p_uint_opcode_t search_op = {
		.opcode = {
			.name = name,
			.type_a = op_a ? low_level_type (op_a->type) : ev_invalid,
			.type_b = op_b ? low_level_type (op_b->type) : ev_invalid,
			.type_c = op_c ? low_level_type (op_c->type) : ev_invalid,
		},
	};
	v6p_uint_opcode_t *uint_op;
	v6p_opcode_t *op;
	v6p_opcode_t *sop;
	void      **op_list;
	int         i;

	uint_op = Hash_FindElement (v6p_opcode_uint_table, &search_op);
	if (uint_op) {
		return v6p_opcode_map + uint_op->op;
	}
	op = Hash_FindElement (v6p_opcode_type_table, &search_op.opcode);
	if (op)
		return op;
	op_list = Hash_FindList (v6p_opcode_void_table, name);
	if (!op_list)
		return op;
	for (i = 0; !op && op_list[i]; i++) {
		sop = op_list[i];
		if (check_operand_type (sop->type_a, search_op.opcode.type_a)
			&& check_operand_type (sop->type_b, search_op.opcode.type_b)
			&& check_operand_type (sop->type_c, search_op.opcode.type_c))
			op = sop;
	}
	free (op_list);
	return op;
}

static const char *unsigned_demote_ops[] = {
	"add",
	"bitand",
	"bitnot",
	"bitor",
	"bitxor",
	"eq",
	"ifnz",
	"ifz",
	"mul",
	"ne",
	"sub",
};

static int
ud_compare (const void *_a, const void *_b)
{
	const char *a = _a;
	const char *b = *(const char **)_b;
	return strcmp (a, b);
}

static etype_t __attribute__((pure))
operand_type (const operand_t *op, const char *name)
{
	if (!op) {
		return ev_invalid;
	}
	etype_t     type = low_level_type (op->type);
	if (type == ev_vector || type == ev_quaternion) {
		return ev_float;
	}
	if (type == ev_uint || type == ev_ulong) {
		if (bsearch (name, unsigned_demote_ops,
					 sizeof (unsigned_demote_ops)
						/ sizeof (unsigned_demote_ops[0]),
					 sizeof (unsigned_demote_ops[0]),
					 ud_compare)) {
			if (type == ev_uint) {
				type = ev_int;
			}
			if (type == ev_ulong) {
				type = ev_long;
			}
		}
	}
	return type;
}

static int
operand_width (const char *opname, operand_t *op)
{
	if (!op) {
		return 0;
	}
	etype_t     type = low_level_type (op->type);
	if (type == ev_vector) {
		return 3;
	}
	if (type == ev_quaternion) {
		return 4;
	}
	// FIXME see FIXME in rua_opcode_find
	if ((type == ev_long || type == ev_ulong || type == ev_double)
		&& (!strcmp (opname, "load") || !strcmp (opname, "store")
			|| !strcmp (opname, "assign"))) {
		if (op->width < 3) {
			return op->width * 2;
		}
	}
	return op->width;
}
#if 0
	if (!strcmp (name, "swizzle")) {
		adjust_swizzle_op (&search_op, 0);
		adjust_swizzle_op (&search_op, 2);
	}
static void
adjust_swizzle_op (opcode_t *op, int opind)
{
	// swizzle instructions require both operands to be 4 components (4 or 8
	// words) in size with the same alignment.
	op->widths[opind] = 4;
	if (pr_type_size[op->types[opind]] == 1) {
		op->types[opind] = ev_float;
	} else if (pr_type_size[op->types[opind]] == 2) {
		op->types[opind] = ev_double;
	} else {
		internal_error (0, "unexpected swizzle op size");
	}
}
#endif
static opcode_t *
rua_opcode_find (const char *name, operand_t *op_a, operand_t *op_b,
				 operand_t *op_c)
{
	// FIXME this is a bit of an ugly hack to map 64-bit load and store/assign
	// instructions: 1 and 2 component instructions become 2 and 4 components
	// using the 32-bit instructions, while 3 and 4 remain unchanged but use
	// the 64-bit versions of the instructs (of which there are only 3 and 4
	// component versions). That bit of fun can't be helped without wasting a
	// lot of instructions, but this mapping scheme leaves a lot to be desired.
	const char *opname_a = "";
	const char *opname_c = "";
	etype_t     type;
	if ((!strcmp (name, "load") || !strcmp (name, "store")
		 || !strcmp (name, "assign"))
		&& ((type = low_level_type (op_c->type)) == ev_long
			|| type == ev_ulong || type == ev_double)) {
		opname_c = name;
		if (!strcmp (name, "assign")) {
			opname_a = name;
			if (op_c->width > 2) {
				name = "assign64";
			}
		}
		if (!strcmp (name, "load")) {
			if (op_c->width > 2) {
				name = "load64";
			}
		}
		if (!strcmp (name, "store")) {
			if (op_c->width > 2) {
				name = "store64";
			}
		}
	}

	opcode_t    search_op = {
		.opname = name,
		.types = {
			operand_type (op_a, name),
			operand_type (op_b, name),
			operand_type (op_c, name),
		},
		.widths = {
			operand_width (opname_a, op_a),
			operand_width ("", op_b),
			operand_width (opname_c, op_c),
		},
	};
	opcode_t   *op;
	opcode_t   *sop;
	void      **op_list;
	int         i;

#if 0
	printf ("%s [%s %d] [%s %d] [%s %d]\n", search_op.opname,
			pr_type_name[search_op.types[0]], search_op.widths[0],
			pr_type_name[search_op.types[1]], search_op.widths[1],
			pr_type_name[search_op.types[2]], search_op.widths[2]);
#endif
	op = Hash_FindElement (rua_opcode_type_table, &search_op);
	if (op)
		return op;
	op_list = Hash_FindList (rua_opcode_void_table, name);
	if (!op_list)
		return op;
	for (i = 0; !op && op_list[i]; i++) {
		sop = op_list[i];
		if (!(check_operand_type (sop->types[0], search_op.types[0])
			  && check_operand_type (sop->types[1], search_op.types[1])
			  && check_operand_type (sop->types[2], search_op.types[2]))) {
			continue;
		}
		if (!(check_operand_width (sop->widths[0], search_op.widths[0])
			  && check_operand_width (sop->widths[1], search_op.widths[1])
			  && check_operand_width (sop->widths[2], search_op.widths[2]))) {
#if 0
			printf ("%s [%s %d] [%s %d] [%s %d]\n", sop->opname,
					pr_type_name[sop->types[0]], sop->widths[0],
					pr_type_name[sop->types[1]], sop->widths[1],
					pr_type_name[sop->types[2]], sop->widths[2]);
#endif
			continue;
		}
		op = sop;
	}
	free (op_list);
	return op;
}

instruction_t *
opcode_find (const char *name, operand_t *op_a, operand_t *op_b,
			 operand_t *op_c)
{
	if (options.code.progsversion < PROG_VERSION) {
		return (instruction_t *) v6p_opcode_find (name, op_a, op_b, op_c);
	} else {
		return (instruction_t *) rua_opcode_find (name, op_a, op_b, op_c);
	}
}

static void
v6p_opcode_init (void)
{
	const v6p_opcode_t *op;
	v6p_opcode_t *mop;

	if (v6p_opcode_type_table) {
		Hash_FlushTable (v6p_opcode_void_table);
		Hash_FlushTable (v6p_opcode_type_table);
		Hash_FlushTable (v6p_opcode_uint_table);
	} else {
		v6p_opcode_type_table = Hash_NewTable (1021, 0, 0, 0, 0);
		Hash_SetHashCompare (v6p_opcode_type_table, v6p_get_hash, v6p_compare);
		v6p_opcode_void_table = Hash_NewTable (1021, v6p_get_key, 0, 0, 0);
		v6p_opcode_uint_table = Hash_NewTable (1021, 0, 0, 0, 0);
		Hash_SetHashCompare (v6p_opcode_uint_table,
							 v6p_uint_get_hash, v6p_uint_compare);
	}

	int         num_opcodes = 0;
	for (op = pr_v6p_opcodes; op->name; op++) {
		num_opcodes++;
	}
	if (!v6p_opcode_map) {
		v6p_opcode_map = calloc (num_opcodes, sizeof (v6p_opcode_t));
	}
	for (int i = 0; i < num_opcodes; i++) {
		op = pr_v6p_opcodes + i;
		if (op->min_version > options.code.progsversion)
			continue;
		mop = v6p_opcode_map + i;
		*mop = *op;
		if (options.code.progsversion == PROG_ID_VERSION) {
			// v6 progs have no concept of integer, but the QF engine
			// treats the operands of certain operands as integers
			// irrespective the progs version, so convert the engine's
			// view of the operands to the prog's view.
			if (mop->type_a == ev_int)
				mop->type_a = ev_float;
			if (mop->type_b == ev_int)
				mop->type_b = ev_float;
			if (mop->type_c == ev_int)
				mop->type_c = ev_float;
		}
		Hash_AddElement (v6p_opcode_type_table, mop);
		if (mop->type_a == ev_void || mop->type_b == ev_void
			|| mop->type_c == ev_void)
			Hash_Add (v6p_opcode_void_table, mop);
	}
	if (options.code.progsversion != PROG_ID_VERSION) {
		for (__auto_type uiop = &v6p_uint_opcodes[0]; uiop->op; uiop++) {
			Hash_AddElement (v6p_opcode_uint_table, uiop);
		}
	}
}

static void
rua_opcode_init (void)
{
	if (rua_opcode_type_table) {
		return;
	}

	rua_opcode_type_table = Hash_NewTable (1021, 0, 0, 0, 0);
	Hash_SetHashCompare (rua_opcode_type_table, rua_get_hash, rua_compare);
	rua_opcode_void_table = Hash_NewTable (1021, rua_get_key, 0, 0, 0);

	int         num_opcodes = sizeof (pr_opcodes) / sizeof (pr_opcodes[0]);
	for (int i = 0; i < num_opcodes; i++) {
		const opcode_t *op = pr_opcodes + i;
		if (!op->opname) {
			continue;
		}
		Hash_AddElement (rua_opcode_type_table, (opcode_t *) op);
		if (op->types[0] == ev_void || op->types[1] == ev_void
			|| op->types[2] == ev_void) {
			Hash_Add (rua_opcode_void_table, (opcode_t *) op);
		}
	}
}

void
opcode_init (void)
{
	if (options.code.progsversion < PROG_VERSION) {
		v6p_opcode_init ();
	}
	rua_opcode_init ();
}

void
opcode_print_statement (pr_uint_t addr, dstatement_t *st)
{
	const char *mnemonic;
	// this is ok because v6p has < 300 instructions
	int         st_op = st->op & 0x1ff;

	if (options.code.progsversion < PROG_VERSION) {
		mnemonic = v6p_opcode_map[st_op].opname;
	} else {
		mnemonic = pr_opcodes[st_op].mnemonic;
	}
	printf ("%04x (%03x)%-8s %d:%04hx %d:%04hx %d:%04hx\n",
			addr, st_op & 0x1ff, mnemonic,
			(st->op & OP_A_BASE) >> OP_A_SHIFT, st->a,
			(st->op & OP_B_BASE) >> OP_B_SHIFT, st->b,
			(st->op & OP_C_BASE) >> OP_C_SHIFT, st->c);
}
