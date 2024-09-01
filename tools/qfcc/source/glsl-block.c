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

#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

ALLOC_STATE (glsl_block_t, blocks);

static hashtab_t *input_blocks;
static hashtab_t *output_blocks;
static hashtab_t *uniform_blocks;
static hashtab_t *buffer_blocks;
static hashtab_t *shared_blocks;

static const char *
block_get_key (const void *_b, void *)
{
	auto b = (const glsl_block_t *) _b;
	return b->name;
}

void
glsl_block_clear (void)
{
	if (input_blocks) {
		Hash_FlushTable (input_blocks);
		Hash_FlushTable (output_blocks);
		Hash_FlushTable (uniform_blocks);
		Hash_FlushTable (buffer_blocks);
		Hash_FlushTable (shared_blocks);
	} else {
		input_blocks = Hash_NewTable (127, block_get_key, 0, 0, 0);
		output_blocks = Hash_NewTable (127, block_get_key, 0, 0, 0);
		uniform_blocks = Hash_NewTable (127, block_get_key, 0, 0, 0);
		buffer_blocks = Hash_NewTable (127, block_get_key, 0, 0, 0);
		shared_blocks = Hash_NewTable (127, block_get_key, 0, 0, 0);
	}
}

static const expr_t *
block_sym_ref (symbol_t *sym, void *data)
{
	return new_symbol_expr (data);
}

void
glsl_declare_block (specifier_t spec, symbol_t *block_sym,
					symbol_t *instance_name)
{
	hashtab_t *block_tab = nullptr;
	switch (spec.storage) {
		case sc_in:
			block_tab = input_blocks;
			break;
		case sc_out:
			block_tab = output_blocks;
			break;
		case sc_uniform:
			block_tab = uniform_blocks;
			break;
		case sc_buffer:
			block_tab = buffer_blocks;
			break;
		case sc_shared:
			block_tab = shared_blocks;
			break;
		case sc_global:
		case sc_system:
		case sc_extern:
		case sc_static:
		case sc_param:
		case sc_local:
		case sc_argument:
			break;
	}
	if (!block_tab) {
		error (0, "invalid storage for block: %d", spec.storage);
		return;
	}
	glsl_block_t *block;
	ALLOC (64, glsl_block_t, blocks, block);
	*block = (glsl_block_t) {
		.name = save_string (block_sym->name),
		.members = block_sym->namespace,
		.instance_name = instance_name,
	};
	Hash_Add (block_tab, block);
	for (auto sym = block->members->symbols; sym; sym = sym->next) {
		auto def = new_def (sym->name, nullptr, nullptr, spec.storage);
		def->type = sym->type;
		sym->sy_type = sy_var;
		sym->def = def;
	}
	if (instance_name) {
		auto type = new_type ();
		*type = (type_t) {
			.type = ev_invalid,
			.name = save_string (block_sym->name),
			.alignment = 4,
			.width = 1,
			.columns = 1,
			.meta = ty_struct,
			.symtab = block->members,
		};
		instance_name->type = append_type (instance_name->type, type);
		instance_name->type = find_type (instance_name->type);
		auto space = current_symtab->space;// FIXME
		initialize_def (instance_name, nullptr, space, spec.storage,
						current_symtab);
	} else {
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
