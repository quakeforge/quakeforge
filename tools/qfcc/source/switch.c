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

	return (unsigned long)cl->value;
}

static int
compare (void *_cla, void *_clb, void *unused)
{
	case_label_t *cla = (case_label_t*)_cla;
	case_label_t *clb = (case_label_t*)_clb;

	return cla->value == clb->value;
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
	if (Hash_FindElement (switch_block->labels, cl)) {
		error (value, "duplicate %s", value ? "case" : "default");
		free (cl);
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

struct expr_s *
switch_expr (switch_block_t *switch_block, expr_t *break_label, 
			 expr_t *statements)
{
	case_label_t **labels, **l;
	case_label_t _default_label;
	case_label_t *default_label = &_default_label;
	expr_t     *sw = new_block_expr ();
	type_t     *type = get_type (switch_block->test);
	expr_t     *temp = new_temp_def_expr (type);
	expr_t     *default_expr;

	temp->line = switch_block->test->line;
	temp->file = switch_block->test->file;
	
	default_label->value = 0;
	default_label = Hash_FindElement (switch_block->labels, default_label);
	labels = (case_label_t **)Hash_GetList (switch_block->labels);

	if (default_label)
		default_expr = new_unary_expr ('g', default_label->label);
	else
		default_expr = new_unary_expr ('g', break_label);
	default_expr->line = temp->line;
	default_expr->file = temp->file;

	append_expr (sw, binary_expr ('b', switch_block->test, temp));
	for (l = labels; *l; l++) {
		if ((*l)->value) {
			expr_t     *cmp = binary_expr (EQ, temp, (*l)->value);
			expr_t     *test = new_binary_expr ('i',
												test_expr (cmp, 1),
												(*l)->label);
			test->line = cmp->line = temp->line;
			test->file = cmp->file = temp->file;
			append_expr (sw, test);
		}
	}
	append_expr (sw, default_expr);
	append_expr (sw, statements);
	append_expr (sw, break_label);
	return sw;
}
