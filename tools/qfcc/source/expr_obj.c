/*
	expr_obj.c

	Objective-QuakeC expression construction and manipulations

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/06/15

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
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/idstuff.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

expr_t *
new_self_expr (void)
{
	symbol_t   *sym;

	sym = make_symbol (".self", &type_entity, pr.near_data, sc_extern);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	return new_symbol_expr (sym);
}

expr_t *
new_this_expr (void)
{
	symbol_t   *sym;

	sym = make_symbol (".this", field_type (&type_id), pr.near_data, sc_extern);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	return new_symbol_expr (sym);
}

const expr_t *
selector_expr (keywordarg_t *selector)
{
	dstring_t  *sel_id = dstring_newstr ();
	expr_t     *sel_ref;
	symbol_t   *sel_sym;
	symbol_t   *sel_table;
	int         index;

	selector = copy_keywordargs (selector);
	selector = (keywordarg_t *) reverse_params ((param_t *) selector);
	selector_name (sel_id, selector);
	index = selector_index (sel_id->str);
	index *= type_size (type_SEL.fldptr.type);
	sel_sym = make_symbol ("_OBJ_SELECTOR_TABLE_PTR", &type_SEL,
						   pr.near_data, sc_static);
	if (!sel_sym->table) {
		symtab_addsymbol (pr.symtab, sel_sym);
		sel_table = make_symbol ("_OBJ_SELECTOR_TABLE",
								 array_type (type_SEL.fldptr.type, 0),
								 pr.far_data, sc_extern);
		if (!sel_table->table)
			symtab_addsymbol (pr.symtab, sel_table);
		reloc_def_def (sel_table->def, sel_sym->def);
	}
	sel_ref = new_symbol_expr (sel_sym);
	sel_ref = new_address_expr (&type_selector, sel_ref,
								new_short_expr (index));

	expr_t     *sel = new_expr ();
	sel->type = ex_selector;
	sel->selector.sel_ref = sel_ref;
	sel->selector.sel = get_selector (sel_ref);
	dstring_delete (sel_id);
	return sel;
}

const expr_t *
protocol_expr (const char *protocol_name)
{
	protocol_t *protocol = get_protocol (protocol_name, 0);

	if (!protocol) {
		return error (0, "cannot find protocol declaration for `%s'",
					  protocol_name);
	}
	class_t    *proto_class = get_class (new_symbol ("Protocol"), 1);
	return new_pointer_expr (0, proto_class->type, protocol_def (protocol));
}

const expr_t *
super_expr (class_type_t *class_type)
{
	symbol_t   *sym;
	expr_t     *super;
	expr_t     *super_block;
	class_t    *class;

	if (!class_type)
		return error (0, "`super' used outside of class implementation");

	class = extract_class (class_type);

	if (!class->super_class)
		return error (0, "%s has no super class", class->name);

	sym = symtab_lookup (current_symtab, ".super");
	if (!sym || sym->table != current_symtab) {
		sym = new_symbol_type (".super", &type_super);
		initialize_def (sym, nullptr, current_symtab->space, sc_local,
						current_symtab, nullptr);
	}
	super = new_symbol_expr (sym);

	super_block = new_block_expr (0);

	const expr_t *e;
	e = assign_expr (field_expr (super, new_name_expr ("self")),
								 new_name_expr ("self"));
	append_expr (super_block, e);

	e = new_symbol_expr (class_pointer_symbol (class));
	e = assign_expr (field_expr (super, new_name_expr ("class")),
					 field_expr (e, new_name_expr ("super_class")));
	if (e->type == ex_error) {
		return e;
	}
	append_expr (super_block, e);

	e = address_expr (super, 0);
	super_block->block.result = e;
	return super_block;
}

const expr_t *
message_expr (const expr_t *receiver, keywordarg_t *message)
{
	const expr_t *selector = selector_expr (message);
	const expr_t *call;
	keywordarg_t *m;
	int         super = 0, class_msg = 0;
	const type_t *rec_type = nullptr;
	const type_t *return_type;
	const type_t *method_type = &type_IMP;
	method_t   *method;
	const expr_t *send_msg;

	if (receiver->type == ex_nil) {
		rec_type = &type_id;
		receiver = convert_nil (receiver, rec_type);
	} else if (receiver->type == ex_symbol) {
		if (strcmp (receiver->symbol->name, "self") == 0) {
			rec_type = get_type (receiver);
		} else if (strcmp (receiver->symbol->name, "super") == 0) {
			super = 1;

			receiver = super_expr (current_class);

			if (receiver->type == ex_error)
				return receiver;
			receiver = cast_expr (&type_id, receiver);	//FIXME better way?
			rec_type = extract_class (current_class)->type;
		} else if (receiver->symbol->sy_type == sy_class) {
			class_t    *class;
			rec_type = receiver->symbol->type;
			class = rec_type->class;
			class_msg = 1;
			receiver = new_symbol_expr (class_pointer_symbol (class));
		}
	}
	if (!rec_type) {
		rec_type = get_type (receiver);
	}
	if (!rec_type) {
		auto err = new_expr ();
		err->type = ex_error;
		return err;
	}

	if (receiver->type == ex_error)
		return receiver;

	return_type = &type_id;
	method = class_message_response (rec_type, class_msg, selector);
	if (method)
		return_type = method->type->func.ret_type;

	scoped_src_loc (receiver);
	expr_t     *args = new_list_expr (0);
	for (m = message; m; m = m->next) {
		if (m->expr && m->expr->list.head) {
			expr_append_list (args, &m->expr->list);
		}
	}
	expr_append_expr (args, selector);
	expr_append_expr (args, receiver);

	send_msg = send_message (super);
	if (method) {
		const expr_t *err;
		if ((err = method_check_params (method, args)))
			return err;
		method_type = method->type;
	}
	call = build_function_call (send_msg, method_type, args);

	if (call->type == ex_error)
		return receiver;

	if (!is_function_call (call)) {
		internal_error (call, "unexpected call expression type");
	}
	((expr_t *) call->block.result)->branch.ret_type = return_type;
	return call;
}
