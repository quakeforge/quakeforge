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

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

ALLOC_STATE (element_t, elements);
ALLOC_STATE (designator_t, designators);

designator_t *
new_designator (symbol_t *field, const expr_t *index)
{
	if ((!field && !index) || (field && index)) {
		internal_error (0, "exactly one of field or index is required");
	}
	designator_t *des;
	ALLOC (256, designator_t, designators, des);
	des->field = new_symbol_expr (field);
	des->index = index;
	return des;
}

element_t *
new_element (const expr_t *expr, designator_t *designator)
{
	element_t  *element;
	ALLOC (256, element_t, elements, element);
	element->expr = expr;
	element->designator = designator;
	return element;
}

element_t *
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
	c->compound.head = 0;
	c->compound.tail = &c->compound.head;
	return c;
}

static symbol_t *
designator_field (const designator_t *des, const type_t *type)
{
	if (des->index) {
		error (des->index, "designator index in non-array");
		return 0;
	}
	symtab_t   *symtab = type->symtab;
	symbol_t   *sym = des->field->symbol;
	symbol_t   *field = symtab_lookup (symtab, sym->name);
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
designator_index (const designator_t *des, int ele_size, int array_count)
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
	if (index <= 0 || index >= array_count) {
		error (des->index, "designator index out of bounds");
		return -1;
	}
	return index * ele_size;
}

static initstate_t
get_designated_offset (const type_t *type, const designator_t *des)
{
	int         offset = -1;
	const type_t *ele_type = nullptr;
	symbol_t   *field = 0;

	if (is_struct (type) || is_union (type)) {
		field = designator_field (des, type);
		offset = field->offset;
		ele_type = field->type;
	} else if (is_array (type)) {
		int         array_count = type->array.count;
		ele_type = dereference_type (type);
		offset = designator_index (des, type_size (ele_type), array_count);
	} else if (is_nonscalar (type)) {
		ele_type = ev_types[type->type];
		if (type->symtab && des->field) {
			field = designator_field (des, type);
			offset = field->offset;
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

bool
skip_field (symbol_t *field)
{
	if (field->sy_type != sy_offset) {
		return true;
	}
	if (field->no_auto_init) {
		return true;
	}
	return false;
}

void
build_element_chain (element_chain_t *element_chain, const type_t *type,
					 const expr_t *eles, int base_offset)
{
	element_t  *ele = eles->compound.head;

	type = unalias_type (type);

	initstate_t state = {};
	if (is_algebra (type)) {
		for (auto e = ele; e; e = e->next) {
			if (!e->designator) {
				error (eles, "block initializer of multi-vector type requires "
					   "designators for all initializer elements");
				return;
			}
		}
		auto t = algebra_struct_type (type);
		if (!t) {
			error (eles, "block initializer on simple multi-vector type");
			return;
		}
		type = t;
	} else if (is_struct (type) || is_union (type)
			   || (is_nonscalar (type) && type->symtab)) {
		state.field = type->symtab->symbols;
		// find first initializable field
		while (state.field && skip_field (state.field)) {
			state.field = state.field->next;
		}
		if (state.field) {
			state.type = state.field->type;
			state.offset = state.field->offset;
		}
	} else if (is_matrix (type)) {
		state.type = vector_type (base_type (type), type->width);
	} else if (is_nonscalar (type)) {
		state.type = base_type (type);
	} else if (is_array (type)) {
		state.type = dereference_type (type);
	} else {
		error (eles, "invalid initialization");
		return;
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
								 base_offset + state.offset);
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
				state.offset = state.field->offset;
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
	append_init_element (&compound->compound, element);
	return compound;
}

void
assign_elements (expr_t *local_expr, const expr_t *init,
				 element_chain_t *element_chain)
{
	element_t  *element;
	auto init_type = get_type (init);
	set_t      *initialized = set_new_size (type_size (init_type));

	for (element = element_chain->head; element; element = element->next) {
		scoped_src_loc (element->expr);
		int         offset = element->offset;
		auto type = element->type;
		const expr_t *alias = new_offset_alias_expr (type, init, offset);

		const expr_t *c;

		if (type_size (type) == 0)
			internal_error (init, "wtw");
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
			auto dst = new_offset_alias_expr (&type_int, init, start);
			auto zero = new_int_expr (0, false);
			auto count = new_int_expr (end - start, false);
			append_expr (local_expr, new_memset_expr (dst, zero, count));
		}
		// skip over all the initialized locations
		in = set_while (in);
		if (in) {
			start = in->element;
		}
	}
	if (start < (unsigned) type_size (init_type)) {
		auto dst = new_offset_alias_expr (&type_int, init, start);
		auto zero = new_int_expr (0, false);
		auto count = new_int_expr (type_size (init_type) - start, false);
		append_expr (local_expr, new_memset_expr (dst, zero, count));
	}
	set_delete (initialized);
}

const expr_t *
initialized_temp_expr (const type_t *type, const expr_t *expr)
{
	if (expr->type == ex_compound && expr->compound.type) {
		type = expr->compound.type;
	}
	expr_t     *block = new_block_expr (0);

	//type = unalias_type (type);
	expr_t     *temp = new_temp_def_expr (type);

	block->block.result = temp;

	if (expr->type == ex_compound) {
		element_chain_t element_chain;

		element_chain.head = 0;
		element_chain.tail = &element_chain.head;
		build_element_chain (&element_chain, type, expr, 0);
		assign_elements (block, temp, &element_chain);
		free_element_chain (&element_chain);
	} else if (expr->type == ex_multivec) {
		expr = algebra_assign_expr (temp, expr);
		append_expr (block, algebra_optimize (expr));
	} else {
		internal_error (expr, "unexpected expression type: %s",
						expr_names[expr->type]);
	}
	return block;
}
