/*
	switch.c

	qc switch statement support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/10/24

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
static const char rcsid[] = 
	"$Id$";

#include <QF/hash.h>
#include <QF/sys.h>

#include "qfcc.h"
#include "switch.h"
#include "scope.h"
#include "qc-parse.h"

static unsigned long
get_hash (void *_cl, void *unused)
{
	case_label_t *cl = (case_label_t*)_cl;

	return cl->value ? cl->value->e.integer_val : 0;
}

static int
compare (void *_cla, void *_clb, void *unused)
{
	case_label_t *cla = (case_label_t*)_cla;
	case_label_t *clb = (case_label_t*)_clb;
	expr_t     *v1 = cla->value;
	expr_t     *v2 = clb->value;

	if (v1 == v2)
		return 1;
	if ((v1 && !v2) || (!v1 && v2))
		return 0;
	return v1->e.integer_val == v2->e.integer_val;
}

struct expr_s *
case_label_expr (switch_block_t *switch_block, expr_t *value)
{
	case_label_t *cl = malloc (sizeof (case_label_t));

	if (!cl)
		Sys_Error ("case_label_expr: Memory Allocation Failure\n");

	if (value && value->type < ex_string) {
		error (value, "non-constant case value");
		free (cl);
		return 0;
	}
	if (!switch_block) {
		error (value, "%s outside of switch", value ? "case" : "default");
		free (cl);
		return 0;
	}
	cl->value = value;
	print_expr(value);puts("");
	if (Hash_FindElement (switch_block->labels, cl)) {
		error (value, "duplicate %s", value ? "case" : "default");
		free (cl);
		return 0;
	}
	cl->label = new_label_expr ();
	Hash_AddElement (switch_block->labels, cl);
	return cl->label;
}

switch_block_t *
new_switch_block (void)
{
	switch_block_t *switch_block = malloc (sizeof (switch_block_t));
	if (!switch_block)
		Sys_Error ("new_switch_block: Memory Allocation Failure\n");
	switch_block->labels = Hash_NewTable (127, 0, 0, 0);
	Hash_SetHashCompare (switch_block->labels, get_hash, compare);
	switch_block->test = 0;
	return switch_block;
}

static int
label_compare (const void *_a, const void *_b)
{
	const case_label_t **a = (const case_label_t **)_a;
	const case_label_t **b = (const case_label_t **)_b;
	const char *s1, *s2;

	switch ((*a)->value->type) {
		case ex_string:
			s1 = (*a)->value->e.string_val ? (*a)->value->e.string_val : "";
			s2 = (*b)->value->e.string_val ? (*b)->value->e.string_val : "";
			return strcmp (s1, s2);
			break;
		case ex_float:
			return (*a)->value->e.float_val - (*b)->value->e.float_val;
			break;
		case ex_integer:
			return (*a)->value->e.integer_val - (*b)->value->e.integer_val;
			break;
		default:
			error (0, "internal compiler error in switch");
			abort ();
	}
}

static void
build_binary_jump_table (expr_t *sw, case_label_t **labels, int op, int base, int count, expr_t *sw_val, expr_t *temp, expr_t *default_label)
{
	case_label_t *l;
	expr_t     *test;
	expr_t     *branch;
	int         index;

	index = count / 2;
	l = labels [base + index];

	test = binary_expr (op, l->value, sw_val);
	test->line = sw_val->line;
	test->file = sw_val->file;

	test = binary_expr ('=', temp, test);
	test->line = sw_val->line;
	test->file = sw_val->file;

	branch = new_binary_expr ('n', test, l->label);
	branch->line = sw_val->line;
	branch->file = sw_val->file;
	append_expr (sw, branch);

	if (index) {
		expr_t     *high_label;

		if (count - (index + 1)) {
			high_label = new_label_expr ();
			high_label->line = sw_val->line;
			high_label->file = sw_val->file;
		} else {
			high_label = default_label;
		}
		branch = new_binary_expr (IFA, temp, high_label);
		branch->line = sw_val->line;
		branch->file = sw_val->file;
		append_expr (sw, branch);

		build_binary_jump_table (sw, labels, op, base, index, sw_val, temp, default_label);

		if (count - (index + 1))
			append_expr (sw, high_label);
	}

	base += index + 1;
	count -= index + 1;

	if (count)
		build_binary_jump_table (sw, labels, op, base, count, sw_val, temp, default_label);
}

struct expr_s *
switch_expr (switch_block_t *switch_block, expr_t *break_label, 
			 expr_t *statements)
{
	case_label_t **labels, **l;
	case_label_t _default_label;
	case_label_t *default_label = &_default_label;
	expr_t     *sw = new_block_expr ();
	type_t     *type = get_type (switch_block->test);
	expr_t     *sw_val = new_temp_def_expr (type);
	expr_t     *default_expr;
	int         num_labels = 0;

	sw_val->line = switch_block->test->line;
	sw_val->file = switch_block->test->file;
	
	default_label->value = 0;
	default_label = Hash_DelElement (switch_block->labels, default_label);
	labels = (case_label_t **)Hash_GetList (switch_block->labels);

	if (!default_label) {
		default_label = &_default_label;
		default_label->label = break_label;
	}
	default_expr = new_unary_expr ('g', default_label->label);
	default_expr->line = sw_val->line;
	default_expr->file = sw_val->file;

	append_expr (sw, binary_expr ('b', switch_block->test, sw_val));

	for (l = labels; *l; l++)
		num_labels++;
	if (options.code.progsversion == PROG_ID_VERSION
		|| (type->type != ev_string
			&& type->type != ev_float
			&& type->type != ev_integer)
		|| num_labels < 8) {
		for (l = labels; *l; l++) {
			expr_t     *cmp = binary_expr (EQ, sw_val, (*l)->value);
			expr_t     *test = new_binary_expr ('i',
												test_expr (cmp, 1),
												(*l)->label);
			test->line = cmp->line = sw_val->line;
			test->file = cmp->file = sw_val->file;
			append_expr (sw, test);
		}
	} else {
		expr_t     *temp = new_temp_def_expr (type);
		int         op;
		qsort (labels, num_labels, sizeof (*labels), label_compare);
		switch (type->type) {
			case ev_string:
				op = NE;
				break;
			case ev_float:
				op = '-';
				break;
			case ev_integer:
				op = '-';
				break;
			default:
				error (0, "internal compiler error in switch");
				abort ();
		}
		build_binary_jump_table (sw, labels, op, 0, num_labels, sw_val, temp, default_label->label);
	}
	append_expr (sw, default_expr);
	append_expr (sw, statements);
	append_expr (sw, break_label);
	return sw;
}
