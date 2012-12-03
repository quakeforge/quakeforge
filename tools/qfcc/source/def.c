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

#include <QF/hash.h>
#include <QF/sys.h>
#include <QF/va.h>

#include "qfcc.h"
#include "def.h"
#include "defspace.h"
#include "diagnostic.h"
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "options.h"
#include "reloc.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"
#include "value.h"

static def_t *free_defs;

static void
set_storage_bits (def_t *def, storage_class_t storage)
{
	switch (storage) {
		case sc_system:
			def->system = 1;
			// fall through
		case sc_global:
			def->global = 1;
			def->external = 0;
			def->local = 0;
			def->param = 0;
			break;
		case sc_extern:
			def->global = 1;
			def->external = 1;
			def->local = 0;
			def->param = 0;
			break;
		case sc_static:
			def->external = 0;
			def->global = 0;
			def->local = 0;
			def->param = 0;
			break;
		case sc_local:
			def->external = 0;
			def->global = 0;
			def->local = 1;
			def->param = 0;
			break;
		case sc_param:
			def->external = 0;
			def->global = 0;
			def->local = 1;
			def->param = 1;
			break;
	}
	def->initialized = 0;
}

def_t *
new_def (const char *name, type_t *type, defspace_t *space,
		 storage_class_t storage)
{
	def_t      *def;

	ALLOC (16384, def_t, defs, def);

	def->return_addr = __builtin_return_address (0);

	def->name = name ? save_string (name) : 0;
	def->type = type;

	def->file = pr.source_file;
	def->line = pr.source_line;

	set_storage_bits (def, storage);

	if (space) {
		def->space = space;
		*space->def_tail = def;
		space->def_tail = &def->next;
	}

	if (!type)
		return def;

	if (!space && storage != sc_extern)
		internal_error (0, "non-external def with no storage space");

	if (storage != sc_extern) {
		int         size = type_size (type);
		if (!size) {
			error (0, "%s has incomplete type", name);
			size = 1;
		}
		def->offset = defspace_alloc_loc (space, size);
	}

	return def;
}

def_t *
alias_def (def_t *def, type_t *type)
{
	def_t      *alias;

	if (def->alias) {
		expr_t      e;
		e.file = def->file;
		e.line = def->line;
		internal_error (&e, "aliasing an alias def");
	}
	if (type_size (type) > type_size (def->type))
		internal_error (0, "aliasing a def to a larger type");
	ALLOC (16384, def_t, defs, alias);
	alias->return_addr = __builtin_return_address (0);
	alias->offset = def->offset;
	alias->type = type;
	alias->alias = def;
	alias->line = pr.source_line;
	alias->file = pr.source_file;
	return alias;
}

def_t *
temp_def (etype_t type, int size)
{
	def_t      *temp;
	defspace_t *space = current_func->symtab->space;

	if ((temp = current_func->temp_defs[size - 1])) {
		current_func->temp_defs[size - 1] = temp->temp_next;
		temp->temp_next = 0;
	} else {
		ALLOC (16384, def_t, defs, temp);
		temp->offset = defspace_alloc_loc (space, size);
		*space->def_tail = temp;
		space->def_tail = &temp->next;
		temp->name = save_string (va (".tmp%d", current_func->temp_num++));
	}
	temp->return_addr = __builtin_return_address (0);
	temp->type = ev_types[type];
	temp->file = pr.source_file;
	temp->line = pr.source_line;
	set_storage_bits (temp, sc_local);
	temp->space = space;
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
	if (!def->alias && def->space) {
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
	def->next = free_defs;
	free_defs = def;
}

void
def_to_ddef (def_t *def, ddef_t *ddef, int aux)
{
	type_t     *type = def->type;

	if (aux)
		type = type->t.fldptr.type;	// aux is true only for fields
	ddef->type = type->type;
	ddef->ofs = def->offset;
	ddef->s_name = ReuseString (def->name);
}

static void
init_elements (struct def_s *def, expr_t *eles)
{
	expr_t     *e, *c;
	int         count, i, num_elements, base_offset;
	pr_type_t  *g;
	def_t      *elements;

	base_offset = def->offset;
	if (def->local && local_expr)
		base_offset = 0;
	if (is_array (def->type)) {
		type_t     *array_type = def->type->t.array.type;
		int         array_size = def->type->t.array.size;
		elements = calloc (array_size, sizeof (def_t));
		for (i = 0; i < array_size; i++) {
			elements[i].type = array_type;
			elements[i].space = def->space;
			elements[i].offset = base_offset + i * type_size (array_type);
		}
		num_elements = i;
	} else if (is_struct (def->type)) {
		symtab_t   *symtab = def->type->t.symtab;
		symbol_t   *field;

		for (i = 0, field = symtab->symbols; field; field = field->next) {
			if (field->sy_type != sy_var)
				continue;
			i++;
		}
		elements = calloc (i, sizeof (def_t));
		for (i = 0, field = symtab->symbols; field; field = field->next) {
			if (field->sy_type != sy_var)
				continue;
			elements[i].type = field->type;
			elements[i].space = def->space;
			elements[i].offset = base_offset + field->s.offset;
			i++;
		}
		num_elements = i;
	} else {
		error (eles, "invalid initializer");
		return;
	}
	for (count = 0, e = eles->e.block.head; e; count++, e = e->next) {
		convert_name (e);
		if (e->type == ex_nil && count < num_elements)
			convert_nil (e, elements[count].type);
		if (e->type == ex_error) {
			free (elements);
			return;
		}
	}
	if (count > num_elements) {
		if (options.warnings.initializer)
			warning (eles, "excessive elements in initializer");
		count = num_elements;
	}
	for (i = 0, e = eles->e.block.head; i < count; i++, e = e->next) {
		g = D_POINTER (pr_type_t, &elements[i]);
		c = constant_expr (e);
		if (c->type == ex_block) {
			if (!is_array (elements[i].type)
				&& !is_struct (elements[i].type)) {
				error (e, "type mismatch in initializer");
				continue;
			}
			init_elements (&elements[i], c);
			continue;
		} else if (c->type == ex_labelref) {
			def_t       loc;
			loc.space = elements[i].space;
			loc.offset = elements[i].offset;
			reloc_def_op (c->e.labelref.label, &loc);
			continue;
		} else if (c->type == ex_value) {
			if (c->e.value->type == ev_integer
				&& elements[i].type->type == ev_float)
				convert_int (c);
			if (get_type (c) != elements[i].type) {
				error (e, "type mismatch in initializer");
				continue;
			}
		} else {
			if (!def->local || !local_expr) {
				error (e, "non-constant initializer");
				continue;
			}
		}
		if (def->local && local_expr) {
			int         offset = elements[i].offset;
			type_t     *type = elements[i].type;
			expr_t     *ptr = new_pointer_expr (offset, type, def);

			append_expr (local_expr, assign_expr (unary_expr ('.', ptr), c));
		} else {
			if (c->type != ex_value)
				internal_error (c, "bogus expression type in init_elements()");
			if (c->e.value->type == ev_string) {
				EMIT_STRING (def->space, g->string_var,
							 c->e.value->v.string_val);
			} else {
				memcpy (g, &c->e.value->v, type_size (get_type (c)) * 4);
			}
		}
	}
	free (elements);
}

static void
init_vector_components (symbol_t *vector_sym, int is_field)
{
	expr_t     *vector_expr;
	int         i;
	static const char *fields[] = { "x", "y", "z" };

	vector_expr = new_symbol_expr (vector_sym);
	for (i = 0; i < 3; i++) {
		expr_t     *expr = 0;
		symbol_t   *sym;
		const char *name;

		name = va ("%s_%s", vector_sym->name, fields[i]);
		sym = symtab_lookup (current_symtab, name);
		if (sym) {
			if (sym->table == current_symtab) {
				if (sym->sy_type != sy_expr) {
					error (0, "%s redefined", name);
					sym = 0;
				} else {
					expr = sym->s.expr;
					if (is_field) {
						if (expr->type != ex_value
							|| expr->e.value->type != ev_field) {
							error (0, "%s redefined", name);
							sym = 0;
						} else {
							expr->e.value->v.pointer.def = vector_sym->s.def;
							expr->e.value->v.pointer.val = i;
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
				expr = new_field_expr (i, &type_float, vector_sym->s.def);
			} else {
				expr = binary_expr ('.', vector_expr,
									new_symbol_expr (new_symbol (fields[i])));
			}
		}
		sym->sy_type = sy_expr;
		sym->s.expr = expr;
		if (!sym->table)
			symtab_addsymbol (current_symtab, sym);
	}
}

static void
init_field_def (def_t *def, expr_t *init, storage_class_t storage)
{
	type_t     *type = def->type->t.fldptr.type;
	def_t      *field_def;
	symbol_t   *field_sym;
	reloc_t    *relocs = 0;

	if (!init)  {
		field_sym = symtab_lookup (pr.entity_fields, def->name);
		if (!field_sym)
			field_sym = new_symbol_type (def->name, type);
		if (field_sym->s.def && field_sym->s.def->external) {
			//FIXME this really is not the right way
			relocs = field_sym->s.def->relocs;
			free_def (field_sym->s.def);
			field_sym->s.def = 0;
		}
		if (!field_sym->s.def) {
			field_sym->s.def = new_def (def->name, type, pr.entity_data, storage);
			reloc_attach_relocs (relocs, &field_sym->s.def->relocs);
			field_sym->s.def->nosave = 1;
		}
		field_def = field_sym->s.def;
		if (!field_sym->table)
			symtab_addsymbol (pr.entity_fields, field_sym);
		if (storage != sc_extern) {
			D_INT (def) = field_def->offset;
			reloc_def_field (field_def, def);
			def->constant = 1;
			def->nosave = 1;
		}
		// no support for initialized field vector componets (yet?)
		if (type == &type_vector && options.code.vector_components)
			init_vector_components (field_sym, 1);
	} else if (init->type == ex_symbol) {
		symbol_t   *sym = init->e.symbol;
		symbol_t   *field = symtab_lookup (pr.entity_fields, sym->name);
		if (field) {
			expr_t     *new = new_field_expr (0, field->type, field->s.def);
			init->type = new->type;
			init->e = new->e;
		}
	}
}

void
initialize_def (symbol_t *sym, type_t *type, expr_t *init, defspace_t *space,
				storage_class_t storage)
{
	symbol_t   *check = symtab_lookup (current_symtab, sym->name);
	reloc_t    *relocs = 0;

	if (!type) {
		warning (0, "type for %s defaults to %s", sym->name,
				 type_default->name);
		type = type_default;
	}
	if (check && check->table == current_symtab) {
		if (check->sy_type != sy_var || check->type != type) {
			error (0, "%s redefined", sym->name);
		} else {
			// is var and same type
			if (!check->s.def)
				internal_error (0, "half defined var");
			if (storage == sc_extern) {
				if (init)
					warning (0, "initializing external variable");
				return;
			}
			if (init && check->s.def->initialized) {
				error (0, "%s redefined", sym->name);
				return;
			}
			sym = check;
		}
	}
	sym->type = type;
	if (!sym->table)
		symtab_addsymbol (current_symtab, sym);
//	if (storage == sc_global && init && is_scalar (type)) {
//		sym->sy_type = sy_const;
//		memset (&sym->s.value, 0, sizeof (&sym->s.value));
//		if (init->type != ex_value) {	//FIXME arrays/structs
//			error (0, "non-constant initializier");
//		} else {
//			sym->s.value = init->e.value;
//			convert_value (&sym->s.value, sym->type);
//		}
//		return;
//	}
	if (sym->s.def && sym->s.def->external) {
		//FIXME this really is not the right way
		relocs = sym->s.def->relocs;
		free_def (sym->s.def);
		sym->s.def = 0;
	}
	if (!sym->s.def) {
		sym->s.def = new_def (sym->name, type, space, storage);
		reloc_attach_relocs (relocs, &sym->s.def->relocs);
	}
	if (type == &type_vector && options.code.vector_components)
		init_vector_components (sym, 0);
	if (type->type == ev_field && storage != sc_local && storage != sc_param)
		init_field_def (sym->s.def, init, storage);
	if (storage == sc_extern) {
		if (init)
			warning (0, "initializing external variable");
		return;
	}
	if (!init)
		return;
	convert_name (init);
	if (init->type == ex_error)
		return;
	if (init->type == ex_nil)
		convert_nil (init, type);
	if ((is_array (type) || is_struct (type))
		&& init->type == ex_block && !init->e.block.result) {
		init_elements (sym->s.def, init);
		sym->s.def->initialized = 1;
	} else {
		if (!type_assignable (type, get_type (init))) {
			error (init, "type mismatch in initializer");
			return;
		}
		if (local_expr) {
			sym->s.def->initialized = 1;
			init = assign_expr (new_symbol_expr (sym), init);
			// fold_constants takes care of int/float conversions
			append_expr (local_expr, fold_constants (init));
		} else {
			if (init->type != ex_value) {	//FIXME enum etc
				error (0, "non-constant initializier");
				return;
			}
			if (init->e.value->type == ev_pointer
				|| init->e.value->type == ev_field) {
				// FIXME offset pointers
				D_INT (sym->s.def) = init->e.value->v.pointer.val;
				if (init->e.value->v.pointer.def)
					reloc_def_field (init->e.value->v.pointer.def, sym->s.def);
			} else {
				ex_value_t  v = *init->e.value;
				if (is_scalar (sym->type))
					convert_value (&v, sym->type);
				if (v.type == ev_string) {
					EMIT_STRING (sym->s.def->space, D_STRING (sym->s.def),
								 v.v.string_val);
				} else {
					memcpy (D_POINTER (void, sym->s.def), &v.v,
							type_size (type) * sizeof (pr_type_t));
				}
			}
			sym->s.def->initialized = sym->s.def->constant = 1;
			sym->s.def->nosave = 1;
		}
	}
	sym->s.def->initializer = init;
}
