/*
	expr_compound.c

	compound intializer expression construction and manipulations

	Copyright (C) 2020 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2020/03/11

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

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/set.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

ALLOC_STATE (element_t, elements);
ALLOC_STATE (designator_t, designators);

designator_t *
new_designator (expr_t *field, expr_t *index)
{
	if ((!field && !index) || (field && index)) {
		internal_error (0, "exactly one of field or index is required");
	}
	if (field && field->type != ex_symbol) {
		internal_error (field, "invalid field designator");
	}
	designator_t *des;
	ALLOC (256, designator_t, designators, des);
	des->field = field;
	des->index = index;
	return des;
}

element_t *
new_element (expr_t *expr, designator_t *designator)
{
	element_t  *element;
	ALLOC (256, element_t, elements, element);
	element->expr = expr;
	element->designator = designator;
	return element;
}

static element_t *
append_init_element (element_chain_t *element_chain, element_t *element)
{
	element->next = 0;
	*element_chain->tail = element;
	element_chain->tail = &element->next;
	return element;
}

expr_t *
new_compound_init (void)
{
	expr_t     *c = new_expr ();
	c->type = ex_compound;
	c->e.compound.head = 0;
	c->e.compound.tail = &c->e.compound.head;
	return c;
}

static symbol_t *
designator_field (const designator_t *des, const type_t *type)
{
	if (des->index) {
		error (des->index, "designator index in non-array");
		return 0;
	}
	symtab_t   *symtab = type->t.symtab;
	symbol_t   *sym = des->field->e.symbol;
	symbol_t   *field = symtab_lookup (symtab, sym->name);;
	if (!field) {
		const char *name = type->name;
		if (!strncmp (name, "tag ", 4)) {
			name += 4;
		}
		error (des->field, "'%s' has no member named '%s'", name, sym->name);
		return 0;
	}
	return field;
}

static int
designator_index (const designator_t *des, int ele_size, int array_size)
{
	if (des->field) {
		error (des->field, "field designator in array initializer");
		return -1;
	} else if (!is_constant (des->index)) {
		error (des->index, "non-constant designator index");
		return -1;
	} else if (!is_integral (get_type (des->index))) {
		error (des->index, "invalid designator index type");
		return -1;
	}
	int         index = expr_integral (des->index);
	if (index <= 0 || index >= array_size) {
		error (des->index, "designator index out of bounds");
		return -1;
	}
	return index * ele_size;
}

typedef struct {
	type_t     *type;
	symbol_t   *field;
	int         offset;
} initstate_t;

static initstate_t
get_designated_offset (const type_t *type, const designator_t *des)
{
	int         offset = -1;
	type_t     *ele_type = 0;
	symbol_t   *field = 0;

	if (is_struct (type) || is_union (type)) {
		field = designator_field (des, type);
		offset = field->s.offset;
		ele_type = field->type;
	} else if (is_array (type)) {
		int         array_size = type->t.array.size;
		ele_type = type->t.array.type;
		offset = designator_index (des, type_size (ele_type), array_size);
	} else if (is_nonscalar (type)) {
		ele_type = ev_types[type->type];
		if (type->t.symtab && des->field) {
			field = designator_field (des, type);
			offset = field->s.offset;
		} else {
			int         vec_width = type_width (type);
			offset = designator_index (des, type_size (ele_type), vec_width);
		}
	} else {
		error (0, "invalid initializer");
	}
	if (ele_type && des->next) {
		__auto_type state = get_designated_offset (ele_type, des->next);
		ele_type = state.type;
		offset += state.offset;
	}
	return (initstate_t) { .type = ele_type, .field = field, .offset = offset};
}

static int
skip_field (symbol_t *field)
{
	if (field->sy_type != sy_var) {
		return 1;
	}
	if (field->no_auto_init) {
		return 1;
	}
	return 0;
}

void
build_element_chain (element_chain_t *element_chain, const type_t *type,
					 expr_t *eles, int base_offset)
{
	element_t  *ele = eles->e.compound.head;

	type = unalias_type (type);

	initstate_t state = {};
	if (is_struct (type) || is_union (type)
		|| (is_nonscalar (type) && type->t.symtab)) {
		state.field = type->t.symtab->symbols;
		while (skip_field (state.field)) {
			state.field = state.field->next;
		}
		state.type = state.field->type;
		state.offset = state.field->s.offset;
	} else if (is_array (type)) {
		state.type = type->t.array.type;
	} else {
		internal_error (eles, "invalid initialization");
	}
	while (ele) {
		if (ele->designator) {
			state = get_designated_offset (type, ele->designator);
		}
		if (!state.type) {
			break;
		}

		if (state.offset >= type_size (type)) {
			if (options.warnings.initializer) {
				warning (eles, "excessive elements in initializer");
			}
			break;
		}

		if (ele->expr && ele->expr->type == ex_compound) {
			build_element_chain (element_chain, state.type, ele->expr,
								 state.offset);
		} else {
			element_t  *element = new_element (0, 0);
			element->type = state.type;
			element->offset = base_offset + state.offset;
			element->expr = ele->expr;	// null -> nil
			append_init_element (element_chain, element);
		}

		state.offset += type_size (state.type);
		if (state.field) {
			state.field = state.field->next;
			while (state.field && skip_field (state.field)) {
				state.field = state.field->next;
			}
			if (state.field) {
				state.type = state.field->type;
				state.offset = state.field->s.offset;
			}
		}

		ele = ele->next;
	}
}

void
free_element_chain (element_chain_t *element_chain)
{
	*element_chain->tail = elements_freelist;
	elements_freelist = element_chain->head;
	element_chain->head = 0;
	element_chain->tail = &element_chain->head;
}

expr_t *
append_element (expr_t *compound, element_t *element)
{
	if (compound->type != ex_compound) {
		internal_error (compound, "not a compound expression");
	}

	if (!element || (element->expr && element->expr->type == ex_error)) {
		return compound;
	}

	if (element->next) {
		internal_error (compound, "append_element: element loop detected");
	}
	append_init_element (&compound->e.compound, element);
	return compound;
}

void
assign_elements (expr_t *local_expr, expr_t *init,
				 element_chain_t *element_chain)
{
	element_t  *element;
	type_t     *init_type = get_type (init);
	set_t      *initialized = set_new_size (type_size (init_type));

	for (element = element_chain->head; element; element = element->next) {
		int         offset = element->offset;
		type_t     *type = element->type;
		expr_t     *alias = new_offset_alias_expr (type, init, offset);

		expr_t     *c;

		if (type_size (type) == 0)
			internal_error (init, "wtf");
		if (element->expr) {
			c = constant_expr (element->expr);
		} else {
			c = new_nil_expr ();
		}
		if (c->type == ex_nil) {
			c = convert_nil (c, type);
		}
		append_expr (local_expr, assign_expr (alias, c));
		set_add_range (initialized, offset, type_size (type));
	}

	unsigned    start = 0;
	for (set_iter_t *in = set_first (initialized); in; in = set_next (in)) {
		unsigned    end = in->element;
		if (end > start) {
			expr_t     *dst = new_offset_alias_expr (&type_int, init, start);
			expr_t     *zero = new_int_expr (0);
			expr_t     *count = new_int_expr (end - start);
			append_expr (local_expr, new_memset_expr (dst, zero, count));
		}
		// skip over all the initialized locations
		in = set_while (in);
		if (in) {
			start = in->element;
		}
	}
	set_delete (initialized);
}

expr_t *
initialized_temp_expr (const type_t *type, expr_t *compound)
{
	type = unalias_type (type);
	element_chain_t element_chain;
	expr_t     *temp = new_temp_def_expr (type);
	expr_t     *block = new_block_expr ();

	element_chain.head = 0;
	element_chain.tail = &element_chain.head;
	build_element_chain (&element_chain, type, compound, 0);
	assign_elements (block, temp, &element_chain);
	block->e.block.result = temp;
	free_element_chain (&element_chain);
	return block;
}
