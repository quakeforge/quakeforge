/*
	iface_block.c

	Shader interface block support

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

#include "QF/alloc.h"
#include "QF/hash.h"
#include "QF/va.h"

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/iface_block.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

const char *interface_names[iface_num_interfaces] = {
	"in",
	"out",
	"uniform",
	"buffer",
	"shared",
	"push_constant",
};

ALLOC_STATE (iface_block_t, iface_blocks);

static hashtab_t *interfaces[iface_num_interfaces];

static const char *
block_get_key (const void *_b, void *)
{
	auto b = (const iface_block_t *) _b;
	return b->name->name;
}

void
block_clear (void)
{
	if (interfaces[iface_in]) {
		for (auto i = iface_in; i < iface_num_interfaces; i++) {
			Hash_FlushTable (interfaces[i]);
		}
	} else {
		for (auto i = iface_in; i < iface_num_interfaces; i++) {
			interfaces[i] = Hash_NewTable (127, block_get_key, 0, 0, 0);
		}
	}
	for (auto i = iface_in; i < iface_num_interfaces; i++) {
		auto name = interface_names[i];
		if (symtab_lookup (current_symtab, name)) {
			internal_error (0, "%s already declared", name);
		}
		auto sym = new_symbol (name);
		sym->sy_type = sy_namespace;
		sym->namespace = new_symtab (nullptr, stab_block);
		symtab_addsymbol (current_symtab, sym);
	}
}

static const expr_t *
block_sym_ref (symbol_t *sym, void *data)
{
	// data is set to the symbol in the table (saves having to find it)
	sym = data;
	iface_block_t *block = sym->table->data;
	auto block_sym = block->instance_name;
	auto expr = new_field_expr (new_symbol_expr (block_sym),
								new_symbol_expr (sym));
	expr->field.type = sym->type;
	return expr;
}

static int
block_alloc_loc (defspace_t *space, int size, int alignment)
{
	// XXX
	return -1;
}

iface_block_t *
create_block (symbol_t *block_sym)
{
	iface_block_t *block;
	ALLOC (64, iface_block_t, iface_blocks, block);
	*block = (iface_block_t) {
		.name = new_symbol (block_sym->name),
		.members = new_symtab (current_symtab, stab_block),
		.space = defspace_new (ds_backed),
	};
	block->members->name = save_string (block_sym->name);
	block->members->data = block;
	block->space->alloc_aligned = block_alloc_loc;
	return block;
}

static void
add_attribute (attribute_t **attributes, attribute_t *attr)
{
	attr->next = *attributes;
	*attributes = attr;
}

static const type_t * __attribute__((pure))
block_matrix_type (const type_t *type)
{
	while (is_array (type)) {
		type = dereference_type (type);
	}
	if (is_matrix (type)) {
		return type;
	}
	return nullptr;
}

static const expr_t *
iface_block_array_count (const type_t *block_type, rua_ctx_t *ctx)
{
	auto sym = new_symbol (".array_count");
	auto offset = new_param (nullptr, &type_uint, "offset");
	auto block = new_param (nullptr, block_type, "block");
	block->next = offset;
	auto param = block;
	auto type = find_type (parse_params (&type_uint, param));
	specifier_t spec = { .type = type, .sym = sym, .params = param };
	auto symtab = current_symtab;
	current_symtab = pr.symtab;//FIXME clean up current_symtab
	sym = function_symbol (spec, ctx);
	if (!sym->metafunc->expr) {
		spec.sym = sym;
		auto opcode = new_name_expr ("OpArrayLength");
		auto intrinsic = new_intrinsic_expr (new_list_expr (opcode));
		sym = build_intrinsic_function (spec, intrinsic, ctx);
	}
	current_symtab = symtab;
	return new_symbol_expr (sym);
}

static const expr_t *
iface_block_array_property (const type_t *type, const attribute_t *attr,
							rua_ctx_t *ctx)
{
	if (ctx->language->array_count
		&& strcmp (attr->name, ctx->language->array_count) != 0) {
		return array_property (type, attr, ctx);
	}
	if (type_count (type)) {
		// fixed-size array so constant
		return array_property (type, attr, ctx);
	}
	if (attr->params->type != ex_field) {
		internal_error (attr->params, "not a block access");
	}
	auto member = attr->params->field.member;
	if (member->type != ex_symbol) {
		internal_error (attr->params, "not a symbol");
	}
	auto offset = new_offset_expr (member);
	const expr_t *array_args[] = {
		edag_add_expr (offset),
		attr->params->field.object,
	};
	auto args = new_list_expr (nullptr);
	list_gather (&args->list, array_args, 2);

	auto block_type = get_type (attr->params->field.object);
	auto expr = new_expr ();
	expr->type = ex_functor;
	expr->functor.func = iface_block_array_count (block_type, ctx);
	expr->functor.args = args;
	return expr;
}

static const type_t *
block_block_type (const type_t *type, const char *pre_tag)
{
	unsigned uint = sizeof (uint32_t);
	if (is_array (type)) {
		type = unalias_type (type);
		auto ele_type = block_block_type (type->array.type, pre_tag);
		type_t new = {
			.type = ev_invalid,
			.meta = ty_array,
			.alignment = type->alignment,
			.array = {
				.type = ele_type,
				.base = type->array.base,
				.count = type->array.count,
			},
			.attributes = type->attributes,
			.property = iface_block_array_property,
		};
		int stride = type_aligned_size (ele_type) * uint;
		add_attribute (&new.attributes,
					   new_attrfunc ("ArrayStride", new_uint_expr (stride)));
		return find_type (&new);
	}
	// union not supported
	if (is_struct (type)) {
		type = unalias_type (type);
		auto name = type->name + 4;	// skip over "tag "
		auto tag = name;
		if (pre_tag) {
			tag = save_string (va ("%s.%s", pre_tag, name));
		}
		type_t new = {
			.type = ev_invalid,
			.name = save_string (va ("%.3s %s", type->name, tag)),
			.meta = ty_struct,
			.attributes = type->attributes,
		};
		auto nt = find_type (&new);
		if (nt->symtab) {
			// already exists (multiple fields of the same struct in the
			// block)
			return nt;
		}
		((type_t *)nt)->symtab = new_symtab (type->symtab->parent, stab_struct);
		unsigned offset = 0;
		int alignment = 1;
		for (auto s = type->symtab->symbols; s; s = s->next) {
			auto ftype = block_block_type (s->type, tag);
			auto sym = new_symbol_type (s->name, ftype);
			sym->sy_type = sy_offset;
			sym->offset = -1;
			sym->id = s->id;
			sym->attributes = s->attributes;
			symtab_addsymbol (nt->symtab, sym);
			if (s->offset >= 0 && type->symtab->type == stab_block) {
				offset = s->offset;
			}
			if (type_align (ftype) > alignment) {
				alignment = type_align (ftype);
			}
			offset = RUP (offset, type_align (ftype) * uint);
			add_attribute (&sym->attributes,
						   new_attrfunc ("Offset", new_uint_expr (offset)));
			offset += type_size (ftype) * uint;

			auto mt = block_matrix_type (ftype);
			if (mt) {
				int stride = type_size (column_type (mt)) * uint;
				add_attribute (&sym->attributes,
							   new_attrfunc ("MatrixStride",
											  new_uint_expr (stride)));
				//FIXME
				add_attribute (&sym->attributes,
							   new_attrfunc ("ColMajor", nullptr));
			}
		}
		nt->symtab->type = type->symtab->type;
		nt->symtab->count = type->symtab->count;
		nt->symtab->size = type->symtab->size;
		nt->symtab->data = type->symtab->data;
		((type_t *)nt)->alignment = alignment;
		return nt;
	}
	return type;
}

void
finish_block (iface_block_t *block)
{
	int index = 0;
	for (auto s = block->members->symbols; s; s = s->next) {
		s->id = index++;
	}
}

void
declare_block_instance (specifier_t spec, iface_block_t *block,
						symbol_t *instance_name, rua_ctx_t *ctx)
{
	if (!block) {
		// error recovery
		return;
	}

	bool transparent = false;
	if (!instance_name) {
		instance_name = new_symbol (va (".%s", block->name->name));
		transparent = true;
	}
	spec.sym = instance_name;
	if (ctx->language->var_attributes) {
		ctx->language->var_attributes (&spec, &spec.attributes, ctx);
	}

	auto interface = iftype_from_sc(spec.storage);
	hashtab_t *block_tab = nullptr;
	if (interface < iface_num_interfaces) {
		block_tab = interfaces[interface];
	}
	if (!block_tab) {
		error (0, "invalid interface for block: %d", spec.storage);
		return;
	}
	block->interface = interface,
	block->attributes = glsl_optimize_attributes (spec.attributes),
	block->members->storage = spec.storage;
	struct_process (block->members, block->member_decls, ctx);
	finish_block (block);

	Hash_Add (block_tab, block);
	auto interface_name = interface_names[block->interface];
	auto interface_sym = symtab_lookup (current_symtab, interface_name);
	if (!interface_sym) {
		internal_error (0, "%s interface not defined", interface_name);
	}

	const char *tag = "blk";
	if (block->interface == iface_in) {
		tag = "ibk";
	} else if (block->interface == iface_out) {
		tag = "obk";
	}

	block->instance_name = instance_name;
	type_t type = {
		.type = ev_invalid,
		.name = save_string (va ("%s %s", tag, block->name->name)),
		.alignment = 4,
		.width = 1,
		.columns = 1,
		.meta = ty_struct,
		.symtab = block->members,
		.attributes = new_attrfunc ("Block", nullptr),
	};
	auto inst_type = block_block_type (&type, nullptr);
	block->members = inst_type->symtab;
	spec = (specifier_t) {
		.sym = instance_name,
		.storage = sc_from_iftype (block->interface),
		.block = block,
	};
	if (spec.sym->type) {
		inst_type = append_type (spec.sym->type, inst_type);
		inst_type = find_type (inst_type);
	}
	glsl_apply_attributes (block->attributes, spec);
	spec.storage = sc_from_iftype (block->interface);
	spec.sym->type = inst_type;
	auto symtab = current_symtab;// FIXME
	current_target.declare_sym (spec, nullptr, symtab, nullptr);

	auto namespace = interface_sym->namespace;
	auto block_sym = symtab_lookup (namespace, block->name->name);
	if (block_sym) {
		error (0, "%s block %s redeclared", interface_name,
			   block_sym->name);
	} else {
		block_sym = block->name;
		block_sym->sy_type = sy_namespace;
		block_sym->namespace = block->members;
		symtab_addsymbol (namespace, block->name);
	}
	if (transparent) {
		for (auto sym = block->members->symbols; sym; sym = sym->next) {
			auto new = new_symbol (sym->name);
			new->sy_type = sy_convert;
			new->convert = (symconv_t) {
				.conv = block_sym_ref,
				.data = sym,
			};

			symtab_addsymbol (current_symtab, new);
		}
	}
}

iface_block_t *
get_block (const char *name, interface_t interface)
{
	if (interface >= iface_num_interfaces) {
		internal_error (0, "invalid interface");
	}
	return Hash_Find (interfaces[interface], name);
}
