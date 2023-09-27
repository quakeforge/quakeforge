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
#include <QF/sys.h>

#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/opcodes.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/switch.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

#include "tools/qfcc/source/qc-parse.h"

typedef struct case_node_s {
	const expr_t *low;
	const expr_t *high;
	const expr_t **labels;
	const expr_t *_label;
	struct case_node_s *left, *right;
} case_node_t;

static ex_value_t * __attribute__((pure))
get_value (const expr_t *e)
{
	if (e->type == ex_symbol)
		return e->symbol->s.value;
	if (e->type != ex_value)
		internal_error (e, "bogus case label");
	return e->value;
}

static __attribute__((pure)) uintptr_t
get_hash (const void *_cl, void *unused)
{
	case_label_t *cl = (case_label_t *) _cl;
	ex_value_t  *val;

	if (!cl->value)
		return 0;
	val = get_value (cl->value);
	return Hash_Buffer (&val->v, sizeof (val->v)) + val->lltype;
}

static int
compare (const void *_cla, const void *_clb, void *unused)
{
	case_label_t *cla = (case_label_t *) _cla;
	case_label_t *clb = (case_label_t *) _clb;
	const expr_t *v1 = cla->value;
	const expr_t *v2 = clb->value;
	ex_value_t *val1, *val2;

	if (v1 == v2)
		return 1;
	if ((v1 && !v2) || (!v1 && v2))
		return 0;
	if (extract_type (v1) != extract_type (v2))
		return 0;
	val1 = get_value (v1);
	val2 = get_value (v2);
	if (val1->type != val2->type)
		return 0;
	return memcmp (&val1->v, &val2->v, sizeof (val1->v)) == 0;
}

const expr_t *
case_label_expr (switch_block_t *switch_block, const expr_t *value)
{
	case_label_t *cl = malloc (sizeof (case_label_t));

	SYS_CHECKMEM (cl);

	if (value)
		value = convert_name (value);
	if (value && !is_constant (value)) {
		error (value, "non-constant case value");
		free (cl);
		return 0;
	}
	if (!switch_block) {
		error (value, "%s outside of switch", value ? "case" : "default");
		free (cl);
		return 0;
	}
	if (!switch_block->test)
		internal_error (value, "broken switch block");
	if (value) {
		type_t     *type = get_type (switch_block->test);
		type_t     *val_type = get_type (value);
		if (!type)
			return 0;
		if (!type_assignable (type, get_type (value)))
			return error (value, "type mismatch in case label");
		if (is_integral (type) && is_integral (val_type)) {
			value = new_int_expr (expr_int (value));
			debug (value, "integeral label used in integral switch");
		} else if (is_integral (type) && is_float (val_type)) {
			warning (value, "float label used in integral switch");
			value = new_int_expr (expr_float (value));
		} else if (is_float (type) && is_integral (val_type)) {
			debug (value, "integeral label used in float switch");
			value = new_float_expr (expr_int (value));
		} else if (is_float (type) && is_float (val_type)) {
			value = new_float_expr (expr_float (value));
			debug (value, "float label used in float switch");
		}
	}
	cl->value = value;
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

	SYS_CHECKMEM (switch_block);
	switch_block->labels = Hash_NewTable (127, 0, 0, 0, 0);
	Hash_SetHashCompare (switch_block->labels, get_hash, compare);
	switch_block->test = 0;
	return switch_block;
}

static __attribute__((pure)) int
label_compare (const void *_a, const void *_b)
{
	const case_label_t **a = (const case_label_t **) _a;
	const case_label_t **b = (const case_label_t **) _b;
	const char *s1, *s2;

	if (is_string_val ((*a)->value)) {
		s1 = expr_string ((*a)->value);
		if (!s1)
			s1 = "";
		s2 = expr_string ((*b)->value);
		if (!s2)
			s2 = "";
		return strcmp (s1, s2);
	} else if (is_float_val ((*a)->value)) {
		return expr_float ((*a)->value) - expr_float ((*b)->value);
	} else if (is_int_val ((*a)->value)) {
		return expr_int ((*a)->value) - expr_int ((*b)->value);
	}
	internal_error (0, "in switch");
}

static case_node_t *
new_case_node (const expr_t *low, const expr_t *high)
{
	case_node_t *node = malloc (sizeof (case_node_t));

	if (!node)
		Sys_Error ("out of memory");
	node->low = low;
	node->high = high;
	if (low == high) {
		node->labels = &node->_label;
		node->_label = 0;
	} else {
		int         size;

		if (!is_int_val (low))
			internal_error (low, "switch");
		size = expr_int (high) - expr_int (low) + 1;
		node->labels = calloc (size, sizeof (const expr_t *));
	}
	node->left = node->right = 0;
	return node;
}

static case_node_t *
balance_case_tree (case_node_t **nodes, int base, int count)
{
	case_node_t *node;
	int         index = count / 2;

	if (!count)
		return 0;

	node = nodes[base + index];

	node->left = balance_case_tree (nodes, base, index);

	base += index + 1;
	count -= index + 1;
	node->right = balance_case_tree (nodes, base, count);

	return node;
}

static case_node_t *
build_case_tree (case_label_t **labels, int count, int range)
{
	case_node_t **nodes;
	int         i, j, k;
	int         num_nodes = 0;

	qsort (labels, count, sizeof (*labels), label_compare);

	nodes = (case_node_t **) malloc (count * sizeof (case_node_t *));

	if (!nodes)
		Sys_Error ("out of memory");

	if (range && is_int_val (labels[0]->value)) {
		for (i = 0; i < count - 1; i = j, num_nodes++) {
			for (j = i + 1; j < count; j++) {
				if (expr_int (labels[j]->value)
					- expr_int (labels[j - 1]->value) > 1)
					break;
			}
			nodes[num_nodes] = new_case_node (labels[i]->value,
											  labels[j - 1]->value);
			for (k = i; k < j; k++)
				nodes[num_nodes]->labels[expr_int (labels[k]->value)
										 - expr_int (labels[i]->value)]
					= labels[k]->label;
		}
		if (i < count) {
			nodes[num_nodes] = new_case_node (labels[i]->value,
											  labels[i]->value);
			nodes[num_nodes]->labels[0] = labels[i]->label;
			num_nodes++;
		}
	} else {
		for (i = 0; i < count; i++, num_nodes++) {
			nodes[num_nodes] = new_case_node (labels[i]->value,
											  labels[i]->value);
			nodes[num_nodes]->labels[0] = labels[i]->label;
		}
	}
	return balance_case_tree (nodes, 0, num_nodes);
}

static void
build_switch (expr_t *sw, case_node_t *tree, int op, const expr_t *sw_val,
			  const expr_t *temp, const expr_t *default_label)
{
	const expr_t *test;
	const expr_t *branch;
	const expr_t *high_label = default_label;
	const expr_t *low_label = default_label;

	if (!tree) {
		branch = goto_expr (default_label);
		append_expr (sw, branch);
		return;
	}

	if (tree->right) {
		high_label = new_label_expr ();
	}
	if (tree->left) {
		low_label = new_label_expr ();
	}

	test = binary_expr (op, sw_val, tree->low);

	test = assign_expr (temp, test);
	append_expr (sw, test);

	if (tree->low == tree->high) {
		branch = branch_expr (EQ, new_alias_expr (&type_int, temp),
							  tree->labels[0]);
		append_expr (sw, branch);

		if (tree->left) {
			branch = branch_expr (GT, new_alias_expr (&type_int, temp),
								  high_label);
			append_expr (sw, branch);

			build_switch (sw, tree->left, op, sw_val, temp, default_label);

			if (tree->right)
				append_expr (sw, high_label);
		}
		if (tree->right || !tree->left) {
			build_switch (sw, tree->right, op, sw_val, temp, default_label);
		}
	} else {
		int         low = expr_int (tree->low);
		int         high = expr_int (tree->high);
		symbol_t   *table_sym;
		const expr_t *table_expr;
		expr_t     *table_init;
		const char *table_name = new_label_name ();
		int         i;
		const expr_t *range = binary_expr ('-', tree->high, tree->low);
		const expr_t *label;

		table_init = new_compound_init ();
		for (i = 0; i <= high - low; i++) {
			((expr_t *) tree->labels[i])->label.used++;
			label = address_expr (tree->labels[i], 0);
			append_element (table_init, new_element (label, 0));
		}
		table_sym = new_symbol_type (table_name,
									 array_type (&type_int,
												 high - low + 1));
		initialize_def (table_sym, table_init, pr.near_data, sc_static,
						current_symtab);
		table_expr = new_symbol_expr (table_sym);

		if (tree->left) {
			branch = branch_expr (LT, temp, low_label);
			append_expr (sw, branch);
		}
		test = binary_expr (GT, cast_expr (&type_uint, temp),
							cast_expr (&type_uint, range));
		branch = branch_expr (NE, test, high_label);
		append_expr (sw, branch);
		branch = jump_table_expr (table_expr, temp);
		append_expr (sw, branch);
		debug (sw, "switch using jump table");
		if (tree->left) {
			append_expr (sw, low_label);
			build_switch (sw, tree->left, op, sw_val, temp, default_label);
		}
		if (tree->right) {
			append_expr (sw, high_label);
			build_switch (sw, tree->right, op, sw_val, temp, default_label);
		}
	}
}

static void
check_enum_switch (switch_block_t *switch_block)
{
	case_label_t cl;
	symbol_t   *enum_val;
	type_t     *type = get_type (switch_block->test);

	for (enum_val = type->t.symtab->symbols; enum_val;
		 enum_val = enum_val->next) {
		cl.value = new_int_expr (enum_val->s.value->v.int_val);
		if (!Hash_FindElement (switch_block->labels, &cl)) {
			warning (switch_block->test,
					 "enumeration value `%s' not handled in switch",
					 enum_val->name);
		}
	}
}

const expr_t *
switch_expr (switch_block_t *switch_block, const expr_t *break_label,
			 const expr_t *statements)
{
	if (switch_block->test->type == ex_error) {
		return switch_block->test;
	}

	int         saved_line = pr.source_line;
	pr_string_t saved_file = pr.source_file;
	pr.source_line = switch_block->test->line;
	pr.source_file = switch_block->test->file;

	case_label_t **labels, **l;
	case_label_t _default_label;
	case_label_t *default_label = &_default_label;
	expr_t     *sw = new_block_expr (0);
	type_t     *type = get_type (switch_block->test);
	const expr_t *sw_val = new_temp_def_expr (type);
	const expr_t *default_expr;
	int         num_labels = 0;

	default_label->value = 0;
	default_label = Hash_DelElement (switch_block->labels, default_label);
	labels = (case_label_t **) Hash_GetList (switch_block->labels);

	if (!default_label) {
		default_label = &_default_label;
		default_label->label = break_label;
		if (options.warnings.enum_switch && is_enum (type))
			check_enum_switch (switch_block);
	}

	append_expr (sw, assign_expr (sw_val, switch_block->test));

	for (l = labels; *l; l++)
		num_labels++;
	if (options.code.progsversion == PROG_ID_VERSION
		|| (!is_string(type) && !is_float(type) && !is_integral (type))
		|| num_labels < 8) {
		for (l = labels; *l; l++) {
			const expr_t *cmp = binary_expr (EQ, sw_val, (*l)->value);
			const expr_t *test = branch_expr (NE, test_expr (cmp),
											  (*l)->label);

			append_expr (sw, test);
		}
		default_expr = goto_expr (default_label->label);
		append_expr (sw, default_expr);
	} else {
		const expr_t *temp;
		int         op;
		case_node_t *case_tree;

		if (is_string(type))
			temp = new_temp_def_expr (&type_int);
		else
			temp = new_temp_def_expr (type);
		case_tree = build_case_tree (labels, num_labels, is_integral (type));
		op = '-';
		if (type->type == ev_string)
			op = NE;
		build_switch (sw, case_tree, op, sw_val, temp, default_label->label);
	}
	pr.source_line = saved_line;
	pr.source_file = saved_file;
	append_expr (sw, statements);
	append_expr (sw, break_label);
	return sw;
}
