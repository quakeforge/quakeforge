/*
	def.c

	def management and symbol tables

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>
	Copyright (C) 1996-1997  Id Software, Inc.

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/06/09

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
#include "QF/hash.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/evaluate.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

ALLOC_STATE (def_t, defs);

static void
set_storage_bits (def_t *def, storage_class_t storage)
{
	def->storage_bits = 0;
	switch (storage) {
		case sc_system:
			def->system = true;
			// fall through
		case sc_global:
			def->global = true;
			break;
		case sc_extern:
			def->global = true;
			def->external = true;
			break;
		case sc_static:
			break;
		case sc_local:
			def->local = true;
			break;
		case sc_inout:
		case sc_in:
		case sc_out:
		case sc_param:
			def->local = true;
			def->param = true;
			break;
		case sc_argument:
			def->local = true;
			def->argument = true;
			break;
		case sc_count:
			break;
	}
}

static bool
deferred_size (storage_class_t storage)
{
	if (storage >= sc_in) {
		return true;
	}
	return false;
}

def_t *
new_def (const char *name, const type_t *type, defspace_t *space,
		 storage_class_t storage)
{
	def_t      *def;

	ALLOC (16384, def_t, defs, def);

	*def = (def_t) {
		.type = type,
		.name = name ? save_string (name) : 0,
		.loc = pr.loc,
		.return_addr = __builtin_return_address (0),
	};

	set_storage_bits (def, storage);

	if (space) {
		if (space->type == ds_virtual && storage == sc_static)
			internal_error (0, "static in a virtual space");
		if (space->type != ds_virtual
			&& (storage == sc_param || storage == sc_local))
			internal_error (0, "param or local in a non-virtual space");
		def->space = space;
		*space->def_tail = def;
		space->def_tail = &def->next;
	}

	if (!type)
		return def;

	if (!space && storage != sc_extern)
		internal_error (0, "non-external def with no storage space");

	if (is_class (type)
		|| (is_array (type) && is_class(dereference_type (type)))) {
		error (0, "statically allocated instance of class %s",
			   type->class->name);
		return def;
	}

	if (storage != sc_extern) {
		int         size = type_size (type);
		int         alignment = type->alignment;

		if (!size && is_array (type) && !deferred_size (storage)) {
			error (0, "%s has incomplete type", name);
			size = 1;
			alignment = 1;
		}
		if (alignment < 1) {
			print_type (type);
			internal_error (0, "type has no alignment");
		}
		if (size) {
			def->offset = defspace_alloc_aligned_loc (space, size, alignment);
		}
	}

	return def;
}

def_t *
cover_alias_def (def_t *def, const type_t *type, int offset)
{
	def_t      *alias;

	if (offset + type_size (type) <= 0 || offset >= type_size (def->type))
		internal_error (0, "invalid cover offset");
	for (alias = def->alias_defs; alias; alias = alias->next) {
		if (alias->type == type && alias->offset == offset)
			return alias;
	}
	ALLOC (16384, def_t, defs, alias);
	alias->name = save_string (va ("[%s:%d]", def->name, offset));
	alias->return_addr = __builtin_return_address (0);
	alias->offset = offset;
	alias->offset_reloc = 1;
	alias->type = type;
	alias->alias = def;
	alias->loc = pr.loc;
	alias->next = def->alias_defs;
	alias->reg = def->reg;
	def->alias_defs = alias;
	return alias;
}

def_t *
alias_def (def_t *def, const type_t *type, int offset)
{
	if (def->alias) {
		expr_t      e = {
			.loc = def->loc,
		};
		internal_error (&e, "aliasing an alias def");
	}
	if (type_size (type) > type_size (def->type))
		internal_error (0, "aliasing a def to a larger type");
	if (offset < 0 || offset + type_size (type) > type_size (def->type))
		internal_error (0, "invalid alias offset");
	return cover_alias_def (def, type, offset);
}

def_t *
temp_def (const type_t *type)
{
	def_t      *temp;
	defspace_t *space = current_func->locals->space;
	int         size = type_size (type);
	int         alignment = type->alignment;

	if (size < 1 || size > MAX_DEF_SIZE) {
		internal_error (0, "%d invalid size for temp def", size);
	}
	if (alignment < 1) {
		internal_error (0, "temp type has no alignment");
	}
	if ((temp = current_func->temp_defs[size - 1])) {
		current_func->temp_defs[size - 1] = temp->temp_next;
		temp->temp_next = 0;
	} else {
		ALLOC (16384, def_t, defs, temp);
		temp->offset = defspace_alloc_aligned_loc (space, size, alignment);
		*space->def_tail = temp;
		space->def_tail = &temp->next;
		temp->name = save_string (va (".tmp%d", current_func->temp_num++));
	}
	temp->return_addr = __builtin_return_address (0);
	temp->type = type;
	temp->loc = pr.loc;
	set_storage_bits (temp, sc_local);
	temp->space = space;
	temp->reg = current_func->temp_reg;
	return temp;
}

void
free_temp_def (def_t *temp)
{
	int         size = type_size (temp->type) - 1;
	temp->temp_next = current_func->temp_defs[size];
	current_func->temp_defs[size] = temp;
}

void
free_def (def_t *def)
{
	if (!def->return_addr)
		internal_error (0, "double free of def");
	if (def->alias) {
		def_t     **a;

		// pull the alias out of the base def's list of alias defs to avoid
		// messing up the list when the alias def is freed.
		for (a = &def->alias->alias_defs; *a; a = &(*a)->next) {
			if (*a == def) {
				*a = def->next;
				break;
			}
		}
	} else if (def->space) {
		def_t     **d;

		for (d = &def->space->defs; *d && *d != def; d = &(*d)->next)
			;
		if (!*d)
			internal_error (0, "freeing unlinked def %s", def->name);
		*d = def->next;
		if (&def->next == def->space->def_tail)
			def->space->def_tail = d;
		if (!def->external)
			defspace_free_loc (def->space, def->offset, type_size (def->type));
	}
	def->return_addr = 0;
	def->free_addr = __builtin_return_address (0);
	FREE (defs, def);
}

void
def_to_ddef (def_t *def, ddef_t *ddef, int aux)
{
	const type_t *type = unalias_type (def->type);

	if (aux)
		type = type->fldptr.type;	// aux is true only for fields
	ddef->type = type->type;
	ddef->ofs = def->offset;
	ddef->name = ReuseString (def->name);
}

static int
zero_memory (expr_t *block, def_t *def, type_t *zero_type,
			 int init_size, int init_offset)
{
	int         zero_size = type_size (zero_type);
	const expr_t *zero = convert_nil (new_nil_expr (), zero_type);
	const expr_t *dst;

	for (; init_offset < init_size + 1 - zero_size; init_offset += zero_size) {
		dst = new_def_expr (def);
		dst = new_offset_alias_expr (zero_type, dst, init_offset);
		append_expr (block, assign_expr (dst, zero));
	}
	return init_offset;
}

static void
init_elements_nil (def_t *def, expr_t *block)
{
	if (def->local && block) {
		// memset to 0
		int         init_size = type_size (def->type);
		int         init_offset = 0;

		if (options.code.progsversion != PROG_ID_VERSION) {
			init_offset = zero_memory (block, def, &type_zero,
									   init_size, init_offset);
		}
		// probably won't happen any time soon, but who knows...
		if (options.code.progsversion != PROG_ID_VERSION
			&& init_size - init_offset >= type_size (&type_quaternion)) {
			init_offset = zero_memory (block, def, &type_quaternion,
									   init_size, init_offset);
		}
		if (init_size - init_offset >= type_size (&type_vector)) {
			init_offset = zero_memory (block, def, &type_vector,
									   init_size, init_offset);
		}
		if (options.code.progsversion != PROG_ID_VERSION
			&& init_size - init_offset >= type_size (&type_double)) {
			init_offset = zero_memory (block, def, &type_double,
									   init_size, init_offset);
		}
		if (init_size - init_offset >= type_size (type_default)) {
			zero_memory (block, def, type_default, init_size, init_offset);
		}
	}
	// it's a global, so already initialized to 0
}

static void
init_elements (struct def_s *def, const expr_t *eles, expr_t *block)
{
	const expr_t *c;
	pr_type_t  *g;
	element_chain_t element_chain;
	element_t  *element;

	if (eles->type == ex_nil) {
		init_elements_nil (def, block);
		return;
	}

	element_chain.head = 0;
	element_chain.tail = &element_chain.head;
	build_element_chain (&element_chain, def->type, eles, 0);

	if (def->local && block) {
		expr_t     *dst = new_def_expr (def);
		assign_elements (block, dst, &element_chain);
	} else {
		def_t       dummy = *def;
		for (element = element_chain.head; element; element = element->next) {
			if (!element->expr
				|| ((c = constant_expr (element->expr))->type == ex_nil)) {
				// nil is type agnostic 0 and defspaces are initialized to
				// 0 on creation
				continue;
			}
			if (c->type == ex_nil) {
				c = convert_nil (c, element->type);
			}
			dummy.offset = def->offset + element->offset;
			g = D_POINTER (pr_type_t, &dummy);
			if (c->type == ex_labelref) {
				// reloc_def_* use only the def's offset and space, so dummy
				// is ok
				reloc_def_op (c->labelref.label, &dummy);
				continue;
			} else if (c->type == ex_value) {
				auto ctype = get_type (c);
				if (ctype != element->type
					&& type_assignable (element->type, ctype)) {
					if (!c->implicit
						&& !type_promotes (element->type, ctype)) {
						warning (c, "initialization of %s with %s"
								 " (use a cast)\n)",
								 get_type_string (element->type),
								 get_type_string (ctype));
					}
					c = cast_expr (element->type, c);
				}
				if (get_type (c) != element->type) {
					error (c, "type mismatch in initializer");
					continue;
				}
			} else {
				if (!def->local || !block) {
					error (c, "non-constant initializer");
					continue;
				}
			}
			if (c->type != ex_value) {
				internal_error (c, "bogus expression type in init_elements()");
			}
			if (c->value->lltype == ev_string) {
				EMIT_STRING (def->space, *(pr_string_t *) g,
							 c->value->string_val);
			} else {
				memcpy (g, &c->value->raw_value, type_size (get_type (c)) * 4);
			}
		}
	}

	free_element_chain (&element_chain);
}

void
init_vector_components (symbol_t *vector_sym, int is_field, symtab_t *symtab)
{
	expr_t     *vector_expr;
	int         i;
	static const char *fields[] = { "x", "y", "z" };

	vector_expr = new_symbol_expr (vector_sym);
	for (i = 0; i < 3; i++) {
		const expr_t *expr = 0;
		symbol_t   *sym;
		const char *name;

		name = va ("%s_%s", vector_sym->name, fields[i]);
		sym = symtab_lookup (symtab, name);
		if (sym) {
			if (sym->table == symtab) {
				if (sym->sy_type != sy_expr) {
					error (0, "%s redefined", name);
					sym = 0;
				} else {
					expr = sym->expr;
					if (is_field) {
						if (expr->type != ex_value
							|| expr->value->lltype != ev_field) {
							error (0, "%s redefined", name);
							sym = 0;
						} else {
							expr->value->pointer.def = vector_sym->def;
							expr->value->pointer.val = i;
						}
					}
				}
			} else {
				sym = 0;
			}
		}
		if (!sym)
			sym = new_symbol (name);
		if (!expr) {
			if (is_field) {
				expr = new_deffield_expr (i, &type_float, vector_sym->def);
			} else {
				expr = field_expr (vector_expr,
								   new_symbol_expr (new_symbol (fields[i])));
			}
		}
		sym->sy_type = sy_expr;
		sym->expr = expr;
		if (!sym->table)
			symtab_addsymbol (symtab, sym);
	}
}

static const expr_t *
init_field_def (def_t *def, const expr_t *init, storage_class_t storage,
				symtab_t *symtab)
{
	const type_t *type = dereference_type (def->type);
	def_t      *field_def;
	symbol_t   *field_sym;
	reloc_t    *relocs = 0;

	if (!init)  {
		field_sym = symtab_lookup (pr.entity_fields, def->name);
		if (!field_sym)
			field_sym = new_symbol_type (def->name, type);
		if (field_sym->def && field_sym->def->external) {
			//FIXME this really is not the right way
			relocs = field_sym->def->relocs;
			free_def (field_sym->def);
			field_sym->def = 0;
		}
		if (!field_sym->def) {
			field_sym->def = new_def (def->name, type, pr.entity_data, storage);
			reloc_attach_relocs (relocs, &field_sym->def->relocs);
			field_sym->def->nosave = 1;
		}
		field_def = field_sym->def;
		if (!field_sym->table)
			symtab_addsymbol (pr.entity_fields, field_sym);
		if (storage != sc_extern) {
			D_INT (def) = field_def->offset;
			reloc_def_field (field_def, def);
			def->constant = 1;
			def->nosave = 1;
		}
		// no support for initialized field vector componets (yet?)
		if (is_vector(type) && options.code.vector_components)
			init_vector_components (field_sym, 1, symtab);
	} else if (init->type == ex_symbol) {
		symbol_t   *sym = init->symbol;
		symbol_t   *field = symtab_lookup (pr.entity_fields, sym->name);
		if (field) {
			scoped_src_loc (init);
			auto new = new_deffield_expr (0, field->type, field->def);
			if (new->type != ex_value) {
				internal_error (init, "expected value expression");
			}
			init = new;
		}
	}
	return init;
}

static int
num_elements (const expr_t *e)
{
	int         count = 0;
	for (auto ele = e->compound.head; ele; ele = ele->next) {
		count++;
	}
	return count;
}

void
initialize_def (symbol_t *sym, const expr_t *init, defspace_t *space,
				storage_class_t storage, symtab_t *symtab, expr_t *block)
{
	symbol_t   *check = symtab_lookup (symtab, sym->name);
	reloc_t    *relocs = 0;

	if (check && check->table == symtab) {
		if (check->sy_type != sy_def || !type_same (check->type, sym->type)) {
			error (0, "%s redefined", sym->name);
		} else {
			// is var and same type
			if (!check->def)
				internal_error (0, "half defined var");
			if (storage == sc_extern) {
				if (init)
					error (0, "initializing external variable");
				return;
			}
			if (init && check->def->initialized) {
				error (0, "%s redefined", sym->name);
				return;
			}
			sym = check;
		}
	}
	sym->sy_type = sy_def;
	if (!sym->table)
		symtab_addsymbol (symtab, sym);

	if (sym->def && sym->def->external) {
		//FIXME this really is not the right way
		relocs = sym->def->relocs;
		free_def (sym->def);
		sym->def = 0;
	}
	if (!sym->def) {
		if (is_array (sym->type) && !type_size (sym->type)
			&& init && init->type == ex_compound) {
			auto ele_type = dereference_type (sym->type);
			sym->type = array_type (ele_type, num_elements (init));
		}
		sym->type = auto_type (sym->type, init);
		sym->def = new_def (sym->name, sym->type, space, storage);
		reloc_attach_relocs (relocs, &sym->def->relocs);
	}
	sym->lvalue = !sym->def->readonly;
	if (is_vector(sym->type) && options.code.vector_components)
		init_vector_components (sym, 0, symtab);
	if (sym->type->type == ev_field && storage != sc_local
		&& storage != sc_param)
		init = init_field_def (sym->def, init, storage, symtab);
	if (storage == sc_extern) {
		if (init)
			error (0, "initializing external variable");
		return;
	}
	if (!init)
		return;
	if (init->type == ex_error)
		return;
	if ((is_structural (sym->type) || is_nonscalar (sym->type))
		&& (init->type == ex_compound || init->type == ex_nil)) {
		init_elements (sym->def, init, block);
		sym->def->initialized = 1;
	} else {
		if (is_nil (init)) {
			init = convert_nil (init, sym->type);
		}
		if (is_buffer_val (init)) {
			init = convert_buffer (init, sym->type);
		}
		auto init_type = get_type (init);
		if (!type_assignable (sym->type, init_type)) {
			error (init, "type mismatch in initializer: %s = %s",
				   get_type_string (sym->type), get_type_string (init_type));
			return;
		}
		if (storage == sc_local && block) {
			sym->def->initialized = 1;
			init = assign_expr (new_symbol_expr (sym), init);
			// fold_constants takes care of int/float conversions
			append_expr (block, fold_constants (init));
		} else if (!options.code.const_initializers && is_constexpr (init)) {
			init = assign_expr (new_symbol_expr (sym), init);
			add_ctor_expr (init);
		} else {
			if (!is_constant (init)) {
				error (init, "non-constant initializier");
				return;
			}
			while (init->type == ex_alias) {
				init = init->alias.expr;
			}
			if (init->type != ex_value) {	//FIXME enum etc
				internal_error (0, "initializier not a value");
				return;
			}
			if (init->value->lltype == ev_ptr
				|| init->value->lltype == ev_field) {
				// FIXME offset pointers
				D_INT (sym->def) = init->value->pointer.val;
				if (init->value->pointer.def)
					reloc_def_field (init->value->pointer.def, sym->def);
			} else {
				ex_value_t *v = init->value;
				if (!init->implicit
					&& is_double (init_type)
					&& (is_integral (sym->type) || is_float (sym->type))) {
					warning (init, "assigning double to %s in initializer "
							 "(use a cast)", sym->type->name);
				}
				if (!type_same (sym->type, init_type))
					v = convert_value (v, sym->type);
				if (v->lltype == ev_string) {
					EMIT_STRING (sym->def->space, D_STRING (sym->def),
								 v->string_val);
				} else {
					memcpy (D_POINTER (pr_type_t, sym->def), &v->raw_value,
							type_size (sym->type) * sizeof (pr_type_t));
				}
			}
			sym->def->initialized = 1;
			if (options.code.const_initializers) {
				sym->def->constant = 1;
				sym->def->nosave = 1;
			}
		}
	}
	sym->def->initializer = init;
}

static void
set_def_attributes (def_t *def, attribute_t *attr_list)
{
	for (attribute_t *attr = attr_list; attr; attr = attr->next) {
		if (!strcmp (attr->name, "nosave")) {
			def->nosave = true;
		} else {
			warning (0, "skipping unknown attribute '%s'", attr->name);
		}
	}
}

void
declare_def (specifier_t spec, const expr_t *init, symtab_t *symtab,
			 expr_t *block)
{
	symbol_t   *sym = spec.sym;
	defspace_t *space = symtab->space;

	if (spec.storage == sc_static) {
		space = pr.near_data;
	}

	initialize_def (sym, init, space, spec.storage, symtab, block);
	set_def_attributes (sym->def, spec.attributes);
}

static int
def_overlap (def_t *d, int offset, int size)
{
	int         d_offset = d->offset;
	int         d_size = type_size (d->type);

	if (d_offset >= offset && d_offset + d_size <= offset + size)
		return 2;	// d is fully overlapped by the range
	if (d_offset < offset + size && offset < d_offset + d_size)
		return 1;	// d is partially overlapped by the range range
	return 0;
}

int
def_offset (def_t *def)
{
	int         offset = def->offset;
	if (def->alias)
		offset += def->alias->offset;
	return offset;
}

int
def_size (def_t *def)
{
	return type_size (def->type);
}

int
def_visit_overlaps (def_t *def, int offset, int size, int overlap, def_t *skip,
					int (*visit) (def_t *, void *), void *data)
{
	int         ret;

	for (def = def->alias_defs; def; def = def->next) {
		if (def == skip)
			continue;
		if (overlap && def_overlap (def, offset, size) < overlap)
			continue;
		if ((ret = visit (def, data)))
			return ret;
	}
	return 0;
}

int
def_visit_all (def_t *def, int overlap,
			   int (*visit) (def_t *, void *), void *data)
{
	def_t      *start_def = def;
	int         ret;

	if ((ret = visit (def, data)))
		return ret;
	if (def->alias) {
		def = def->alias;
		if (!(overlap & 4) && (ret = visit (def, data)))
			return ret;
		overlap &= ~4;
	} else {
		overlap = 0;
	}
	int         offset = start_def->offset;
	int         size = start_def->type ? type_size (start_def->type) : 0;
	return def_visit_overlaps (def, offset, size, overlap, start_def,
							   visit, data);
}
