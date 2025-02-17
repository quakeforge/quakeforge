/*
	glsl-block.c

	GLSL specific block handling

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
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

ALLOC_STATE (glsl_block_t, blocks);

static hashtab_t *interfaces[glsl_num_interfaces];

static const char *
block_get_key (const void *_b, void *)
{
	auto b = (const glsl_block_t *) _b;
	return b->name->name;
}

void
glsl_block_clear (void)
{
	if (interfaces[glsl_in]) {
		for (auto i = glsl_in; i < glsl_num_interfaces; i++) {
			Hash_FlushTable (interfaces[i]);
		}
	} else {
		for (auto i = glsl_in; i < glsl_num_interfaces; i++) {
			interfaces[i] = Hash_NewTable (127, block_get_key, 0, 0, 0);
		}
	}
	for (auto i = glsl_in; i < glsl_num_interfaces; i++) {
		auto name = glsl_interface_names[i];
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
	glsl_block_t *block = sym->table->data;
	auto block_sym = block->instance_name;
	auto expr = new_field_expr (new_symbol_expr (block_sym),
								new_symbol_expr (sym));
	expr->field.type = sym->type;
	return expr;
}

static int
glsl_block_alloc_loc (defspace_t *space, int size, int alignment)
{
	// XXX
	return -1;
}

glsl_block_t *
glsl_create_block (specifier_t spec, symbol_t *block_sym)
{
	auto interface = glsl_iftype_from_sc(spec.storage);
	hashtab_t *block_tab = nullptr;
	if (interface < glsl_num_interfaces) {
		block_tab = interfaces[interface];
	}
	if (!block_tab) {
		error (0, "invalid interface for block: %d", spec.storage);
		return nullptr;
	}
	glsl_block_t *block;
	ALLOC (64, glsl_block_t, blocks, block);
	*block = (glsl_block_t) {
		.name = new_symbol (block_sym->name),
		.interface = interface,
		.attributes = glsl_optimize_attributes (spec.attributes),
		.members = new_symtab (current_symtab, stab_block),
		.space = defspace_new (ds_backed),
	};
	block->members->name = save_string (block_sym->name);
	block->members->storage = spec.storage;
	block->members->data = block;
	block->space->alloc_aligned = glsl_block_alloc_loc;
	Hash_Add (block_tab, block);
	return block;
}

static void
add_attribute (attribute_t **attributes, attribute_t *attr)
{
	attr->next = *attributes;
	*attributes = attr;
}

static const type_t *
glsl_matrix_type (const type_t *type)
{
	while (is_array (type)) {
		type = dereference_type (type);
	}
	if (is_matrix (type)) {
		return type;
	}
	return nullptr;
}

static const type_t *
glsl_block_type (const type_t *type, const char *pre_tag)
{
	unsigned uint = sizeof (uint32_t);
	if (is_array (type)) {
		type = unalias_type (type);
		auto ele_type = glsl_block_type (type->array.type, pre_tag);
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
		};
		int stride = type_aligned_size (ele_type) * uint;
		add_attribute (&new.attributes,
					   new_attribute ("ArrayStride", new_uint_expr (stride)));
		return find_type (&new);
	}
	// union not supported
	if (is_struct (type)) {
		type = unalias_type (type);
		auto name = type->name + 4;	// skip over "tag "
		auto tag = name;
		if (pre_tag) {
			tag = save_string (va (0, "%s.%s", pre_tag, name));
		}
		type_t new = {
			.type = ev_invalid,
			.name = save_string (va (0, "%.3s %s", type->name, tag)),
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
			auto ftype = glsl_block_type (s->type, tag);
			auto sym = new_symbol_type (s->name, ftype);
			sym->sy_type = sy_offset;
			sym->offset = -1;
			sym->id = s->id;
			symtab_addsymbol (nt->symtab, sym);
			if (type_align (ftype) > alignment) {
				alignment = type_align (ftype);
			}
			offset = RUP (offset, type_align (ftype) * uint);
			add_attribute (&sym->attributes,
						   new_attribute ("Offset", new_uint_expr (offset)));
			offset += type_size (ftype) * uint;

			auto mt = glsl_matrix_type (ftype);
			if (mt) {
				int stride = type_size (column_type (mt)) * uint;
				add_attribute (&sym->attributes,
							   new_attribute ("MatrixStride",
											  new_uint_expr (stride)));
				//FIXME
				add_attribute (&sym->attributes,
							   new_attribute ("ColMajor", nullptr));
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
glsl_finish_block (glsl_block_t *block, specifier_t spec)
{
	spec.sym = block->name;
	spec.block = block;
	int index = 0;
	for (auto s = block->members->symbols; s; s = s->next) {
		s->id = index++;
	}
	glsl_apply_attributes (block->attributes, spec);
}

void
glsl_declare_block_instance (glsl_block_t *block, symbol_t *instance_name)
{
	if (!block) {
		// error recovery
		return;
	}
	auto interface_name = glsl_interface_names[block->interface];
	auto interface_sym = symtab_lookup (current_symtab, interface_name);
	if (!interface_sym) {
		internal_error (0, "%s interface not defined", interface_name);
	}
	auto interface = interface_sym->namespace;
	bool transparent = false;

	if (!instance_name) {
		instance_name = new_symbol (va (0, ".%s", block->name->name));
		transparent = true;
	}
	block->instance_name = instance_name;
	type_t type = {
		.type = ev_invalid,
		.name = save_string (va (0, "blk %s", block->name->name)),
		.alignment = 4,
		.width = 1,
		.columns = 1,
		.meta = ty_struct,
		.symtab = block->members,
		.attributes = new_attribute ("Block", nullptr),
	};
	auto inst_type = glsl_block_type (&type, nullptr);
	block->members = inst_type->symtab;
	specifier_t spec = {
		.sym = instance_name,
		.storage = glsl_sc_from_iftype (block->interface),
	};
	if (spec.sym->type) {
		inst_type = append_type (spec.sym->type, inst_type);
		inst_type = find_type (inst_type);
	}
	spec.sym->type = inst_type;
	auto symtab = current_symtab;// FIXME
	current_target.declare_sym (spec, nullptr, symtab, nullptr);

	auto block_sym = symtab_lookup (interface, block->name->name);
	if (block_sym) {
		error (0, "%s block %s redeclared", interface_name,
			   block_sym->name);
	} else {
		block_sym = block->name;
		block_sym->sy_type = sy_namespace;
		block_sym->namespace = block->members;
		symtab_addsymbol (interface, block->name);
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

glsl_block_t *
glsl_get_block (const char *name, glsl_interface_t interface)
{
	if (interface >= glsl_num_interfaces) {
		internal_error (0, "invalid interface");
	}
	return Hash_Find (interfaces[interface], name);
}
