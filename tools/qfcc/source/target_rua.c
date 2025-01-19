/*
	target_rua.c

	Ruamoko progs backend.

	Copyright (C) 2024 Bill Currie <bill@taniwha.org>

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

#include "QF/va.h"
#include "QF/progs/pr_comp.h"

#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/flow.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/switch.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

// standardized base register to use for all locals (arguments, local defs,
// params)
#define LOCALS_REG 1
// keep the stack aligned to 8 words (32 bytes) so lvec etc can be used without
// having to do shenanigans with mixed-alignment stack frames
#define STACK_ALIGN 8

static bool
ruamoko_value_too_large (const type_t *val_type)
{
	return type_size (val_type) > MAX_DEF_SIZE;
}

static void
create_param (symtab_t *parameters, symbol_t *param)
{
	defspace_t *space = parameters->space;
	def_t      *def = new_def (param->name, 0, space, sc_param);
	int         size = type_size (param->type);
	int         alignment = param->type->alignment;
	if (alignment < 4) {
		alignment = 4;
	}
	def->offset = defspace_alloc_aligned_highwater (space, size, alignment);
	def->type = param->type;
	param->def = def;
	param->sy_type = sy_def;
	param->lvalue = !def->readonly;
	symtab_addsymbol (parameters, param);
	if (is_vector(param->type) && options.code.vector_components)
		init_vector_components (param, 0, parameters);
}

static void
ruamoko_build_scope (symbol_t *fsym)
{
	function_t *func = fsym->metafunc->func;

	for (param_t *p = fsym->params; p; p = p->next) {
		symbol_t   *param;
		if (!p->selector && !p->type && !p->name) {
			// ellipsis marker
			param = new_symbol_type (".args", &type_va_list);
		} else {
			if (!p->type) {
				continue;					// non-param selector
			}
			if (is_void (p->type)) {
				if (p->name) {
					error (0, "invalid parameter type for %s", p->name);
				} else if (p != fsym->params || p->next) {
					error (0, "void must be the only parameter");
					continue;
				} else {
					continue;
				}
			}
			if (!p->name) {
				error (0, "parameter name omitted");
				p->name = save_string ("");
			}
			param = new_symbol_type (p->name, p->type);
		}
		create_param (func->parameters, param);
		if (p->qual == pq_out) {
			param->def->param = false;
			param->def->out_param = true;
		} else if (p->qual == pq_inout) {
			param->def->out_param = true;
		} else if (p->qual == pq_const) {
			param->def->readonly = true;
		}
		param->def->reg = func->temp_reg;
	}
}

static void
ruamoko_build_code (function_t *func, const expr_t *statements)
{
	/* Create a function entry block to set up the stack frame and add the
	 * actual function code to that block. This ensure that the adjstk and
	 * with statements always come first, regardless of what ideas the
	 * optimizer gets.
	 */
	expr_t     *e;
	expr_t     *entry = new_block_expr (0);
	entry->loc = func->def->loc;

	e = new_adjstk_expr (0, 0);
	e->loc = entry->loc;
	append_expr (entry, e);

	e = new_with_expr (2, LOCALS_REG, new_short_expr (0));
	e->loc = entry->loc;
	append_expr (entry, e);

	append_expr (entry, statements);
	statements = entry;

	/* Mark all local defs as using the base register used for stack
	 * references.
	 */
	func->temp_reg = LOCALS_REG;
	for (def_t *def = func->locals->space->defs; def; def = def->next) {
		if (def->local || def->param) {
			def->reg = LOCALS_REG;
		}
	}
	for (def_t *def = func->parameters->space->defs; def; def = def->next) {
		if (def->local || def->param) {
			def->reg = LOCALS_REG;
		}
	}

	func->code = pr.code->size;
	lineno_base = func->def->loc.line;
	func->sblock = make_statements (statements);
	if (options.code.optimize) {
		flow_data_flow (func);
	} else {
		statements_count_temps (func->sblock);
	}
	emit_statements (func->sblock);

	defspace_sort_defs (func->parameters->space);
	defspace_sort_defs (func->locals->space);

	defspace_t *space = defspace_new (ds_virtual);

	if (func->arguments) {
		func->arguments->size = func->arguments->max_size;
		merge_spaces (space, func->arguments, STACK_ALIGN);
		func->arguments = 0;
	}

	merge_spaces (space, func->locals->space, STACK_ALIGN);
	func->locals->space = space;

	// allocate 0 words to force alignment and get the address
	func->params_start = defspace_alloc_aligned_highwater (space, 0,
														   STACK_ALIGN);

	dstatement_t *st = &pr.code->code[func->code];
	if (pr.code->size > func->code && st->op == OP_ADJSTK) {
		if (func->params_start) {
			st->b = -func->params_start;
		} else {
			// skip over adjstk so a zero adjustment doesn't get executed
			func->code += 1;
		}
	}
	merge_spaces (space, func->parameters->space, STACK_ALIGN);
	func->parameters->space = space;

	// force the alignment again so the full stack slot is counted when
	// the final parameter is smaller than STACK_ALIGN words
	defspace_alloc_aligned_highwater (space, 0, STACK_ALIGN);
}

static int
copy_elements (expr_t *block, const expr_t *dst, const expr_t *src, int base)
{
	int         index = 0;
	for (auto li = src->vector.list.head; li; li = li->next) {
		auto e = li->expr;
		if (e->type == ex_vector) {
			index += copy_elements (block, dst, e, index + base);
		} else {
			auto type = get_type (e);
			auto dst_ele = new_offset_alias_expr (type, dst, index + base);
			append_expr (block, assign_expr (dst_ele, e));
			index += type_width (type);
		}
	}
	return index;
}

static const expr_t *
ruamoko_assign_vector (const expr_t *dst, const expr_t *src)
{
	expr_t     *block = new_block_expr (0);

	copy_elements (block, dst, src, 0);
	block->block.result = dst;
	return block;
}

static switch_block_t *switch_block;
const expr_t *
ruamoko_proc_switch (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	auto test = expr_process (expr->switchblock.test, ctx);

	auto sb = switch_block;
	switch_block = new_switch_block ();
	switch_block->test = test;
	auto body = expr_process (expr->switchblock.body, ctx);

	auto break_label = expr->switchblock.break_label;
	auto swtch = switch_expr (switch_block, break_label, body);
	switch_block = sb;
	return swtch;
}

const expr_t *
ruamoko_proc_caselabel (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	if (expr->caselabel.end_value) {
		internal_error (expr, "case ranges not implemented");
	}
	auto value = expr_process (expr->caselabel.value, ctx);
	if (is_error (value)) {
		return value;
	}
	return case_label_expr (switch_block, value);
}

const expr_t *
ruamoko_field_array (const expr_t *e)
{
	ex_list_t list = {};
	// convert the left-branching field/array expression chain to a list
	while (e->type == ex_field || e->type == ex_array) {
		for (; e->type == ex_field; e = e->field.object) {
			list_prepend (&list, e);
		}
		for (; e->type == ex_array; e = e->array.base) {
			list_prepend (&list, e);
		}
	}
	// e is now the base object of the field/array expression chain

	int num_obj = list_count (&list);
	const expr_t *objects[num_obj];
	list_scatter (&list, objects);
	for (int i = 0; i < num_obj; i++) {
		auto obj = objects[i];
		auto base_type = get_type (e);
		if (obj->type == ex_field) {
			if (obj->field.member->type != ex_symbol) {
				internal_error (obj->field.member, "not a symbol");
			}
			auto sym = obj->field.member->symbol;
			if (is_pointer (base_type)) {
				auto offset = new_short_expr (sym->offset);
				e = offset_pointer_expr (e, offset);
				e = cast_expr (pointer_type (obj->field.type), e);
				e = unary_expr ('.', e);
			} else if (e->type == ex_uexpr && e->expr.op == '.') {
				auto offset = new_short_expr (sym->offset);
				e = offset_pointer_expr (e->expr.e1, offset);
				e = cast_expr (pointer_type (obj->field.type), e);
				e = unary_expr ('.', e);
			} else {
				e = new_offset_alias_expr (obj->field.type, e, sym->offset);
			}
		} else if (obj->type == ex_array) {
			scoped_src_loc (obj->array.index);
			int base_ind = 0;
			if (is_array (base_type)) {
				base_ind = base_type->array.base;
			}
			auto ele_type = obj->array.type;
			auto base = new_int_expr (base_ind, false);
			auto scale = new_int_expr (type_size (ele_type), false);
			auto offset = binary_expr ('*', base, scale);
			auto index = binary_expr ('*', obj->array.index, scale);
			offset = binary_expr ('-', index, offset);
			const expr_t *ptr;
			if (is_array (base_type)) {
				if (e->type == ex_uexpr && e->expr.op == '.') {
					ptr = e->expr.e1;
				} else {
					auto alias = new_offset_alias_expr (ele_type, e, 0);
					ptr = new_address_expr (ele_type, alias, 0);
				}
			} else if (is_nonscalar (base_type) || is_matrix (base_type)) {
				auto alias = new_offset_alias_expr (ele_type, e, 0);
				ptr = new_address_expr (ele_type, alias, 0);
			} else {
				ptr = e;
			}
			ptr = offset_pointer_expr (ptr, offset);
			ptr = cast_expr (pointer_type (obj->field.type), ptr);
			e = unary_expr ('.', ptr);
		} else {
			internal_error (obj, "what the what?!?");
		}
	}
	return e;
}

const expr_t *
ruamoko_proc_address (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	auto e = expr_process (expr->expr.e1, ctx);
	if (is_error (e)) {
		return e;
	}
	if (e->type == ex_field || e->type == ex_array) {
		e = ruamoko_field_array (e);
	}
	return address_expr (e, nullptr);
}

static const expr_t *
ruamoko_vector_compare (int op, const expr_t *e1, const expr_t *e2)
{
	// both e1 and e2 should have the same types here
	auto type = get_type (e1);
	if (op != QC_EQ && op != QC_NE) {
		return error (e2, "invalid comparison for %s", type->name);
	}
	int hop = op == QC_EQ ? '&' : '|';
	auto e = new_binary_expr (op, e1, e2);
	e->expr.type = bool_type (type);
	e = new_horizontal_expr (hop, e, &type_int);
	return e;
}

static const expr_t *
ruamoko_shift_op (int op, const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);

	if (is_matrix (t1) || is_matrix (t2)) {
		return error (e1, "invalid operands for %s", get_op_string (op));
	}
	if (is_real (t1)) {
		warning (e1, "shift of floating point value");
	}
	if (is_real (t2)) {
		warning (e2, "shift by floating point value");
	}
	if (is_double (t1)) {
		t1 = vector_type (&type_long, type_width (t1));
		t2 = vector_type (&type_long, type_width (t1));
	}
	if (is_float (t1)) {
		t1 = vector_type (&type_int, type_width (t1));
		t2 = vector_type (&type_int, type_width (t1));
	}
	if (is_uint (t1) || is_int (t1)) {
		t2 = vector_type (&type_int, type_width (t1));
	}
	if (is_ulong (t1) || is_long (t1)) {
		t2 = vector_type (&type_long, type_width (t1));
	}
	e1 = cast_expr (t1, e1);
	e2 = cast_expr (t2, e2);
	auto e = new_binary_expr (op, e1, e2);
	e->expr.type = t1;
	return fold_constants (e);
}

static const expr_t *
ruamoko_test_expr (const expr_t *expr)
{
	scoped_src_loc (expr);
	auto type = get_type (expr);
	if (is_bool (type) && is_scalar (type)) {
		return expr;
	}
	if (is_lbool (type) && is_scalar (type)) {
		expr = new_alias_expr (&type_ivec2, expr);
		expr = fold_constants (expr);
		expr = edag_add_expr (expr);
		return new_horizontal_expr ('|', expr, &type_int);
	}
	if (is_boolean (type)) {
		// the above is_bool and is_lbool tests ensure a boolean type
		// is a vector (there are no bool matrices)
		type = base_type (type);
		expr = new_horizontal_expr ('|', expr, type);
		if (type_size (type) > 1) {
			expr = fold_constants (expr);
			expr = edag_add_expr (expr);
			expr = new_alias_expr (&type_ivec2, expr);
			expr = fold_constants (expr);
			expr = edag_add_expr (expr);
			expr = new_horizontal_expr ('|', expr, &type_bool);
		}
		return expr;
	}
	if (is_enum (type)) {
		expr_t     *zero, *one;
		if (!enum_as_bool (type, &zero, &one)) {
			warning (expr, "enum doesn't convert to bool");
		}
		return new_alias_expr (&type_bool, expr);
	}
	switch (type->type) {
		case ev_float:
		case ev_double:
		case ev_short:
		case ev_ushort:
		{
			// short and ushort handled with the same code as float/double
			// because they have no backing type and and thus constants, which
			// fold_constants will take care of.
			auto zero = new_zero_expr (type);
			auto btype = bool_type (type);
			expr = typed_binary_expr (btype, QC_NE, expr, zero);
			if (type_width (btype) > 1) {
				btype = base_type (btype);
				expr = fold_constants (expr);
				expr = edag_add_expr (expr);
				expr = new_horizontal_expr ('|', expr, btype);
			}
			if (type_size (btype) > 1) {
				expr = fold_constants (expr);
				expr = edag_add_expr (expr);
				expr = new_alias_expr (&type_ivec2, expr);
				expr = fold_constants (expr);
				expr = edag_add_expr (expr);
				expr = new_horizontal_expr ('|', expr, &type_bool);
			}
			return expr;
		}
		case ev_vector:
		case ev_quaternion:
		{
			auto zero = new_zero_expr (type);
			auto btype = bool_type (type);
			expr = typed_binary_expr (btype, QC_NE, expr, zero);
			expr = fold_constants (expr);
			expr = edag_add_expr (expr);
			expr = new_horizontal_expr ('|', expr, &type_bool);
			return expr;
		}
		case ev_string:
			if (!options.code.ifstring) {
				return new_alias_expr (&type_bool, expr);
			}
			return typed_binary_expr (&type_bool, QC_NE, expr,
									  new_string_expr (0));
		case ev_int:
		case ev_uint:
			if (type_width (type) > 1) {
				expr = new_horizontal_expr ('|', expr, &type_int);
				expr = fold_constants (expr);
				expr = edag_add_expr (expr);
			}
			if (is_constant (expr)) {
				return new_bool_expr (expr_int (expr));
			}
			return new_alias_expr (&type_bool, expr);
		case ev_long:
		case ev_ulong:
			if (type_width (type) > 1) {
				expr = new_horizontal_expr ('|', expr, &type_long);
				expr = fold_constants (expr);
				expr = edag_add_expr (expr);
			}
			expr = new_alias_expr (&type_ivec2, expr);
			return new_horizontal_expr ('|', expr, &type_int);
		case ev_entity:
		case ev_field:
		case ev_func:
		case ev_ptr:
			return new_alias_expr (&type_bool, expr);
		case ev_void:
		case ev_invalid:
		case ev_type_count:
			break;
	}
	return error (expr, "cannot convert to bool");
}

target_t ruamoko_target = {
	.value_too_large = ruamoko_value_too_large,
	.build_scope = ruamoko_build_scope,
	.build_code = ruamoko_build_code,
	.declare_sym = declare_def,
	.initialized_temp = initialized_temp_expr,
	.assign_vector = ruamoko_assign_vector,
	.proc_switch = ruamoko_proc_switch,
	.proc_caselabel = ruamoko_proc_caselabel,
	.proc_address = ruamoko_proc_address,
	.vector_compare = ruamoko_vector_compare,
	.shift_op = ruamoko_shift_op,
	.test_expr = ruamoko_test_expr,
};
