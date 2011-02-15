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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

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
#include "immediate.h"
#include "options.h"
#include "reloc.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"

static def_t *free_defs;

static void
set_storage_bits (def_t *def, storage_class_t storage)
{
	switch (storage) {
		case st_system:
			def->system = 1;
			// fall through
		case st_global:
			def->global = 1;
			def->external = 0;
			def->local = 0;
			break;
		case st_extern:
			def->global = 1;
			def->external = 1;
			def->local = 0;
			break;
		case st_static:
			def->external = 0;
			def->global = 0;
			def->local = 0;
			break;
		case st_local:
			def->external = 0;
			def->global = 0;
			def->local = 1;
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

	if (!space && storage != st_extern)
		internal_error (0, "non-external def with no storage space");
	if (!type)
		internal_error (0, "attempt to create def '%s' with a null type",
						name);

	def->return_addr = __builtin_return_address (0);

	def->name = name ? save_string (name) : 0;
	def->type = type;

	if (storage != st_extern) {
		int         size = type_size (type);
		if (!size) {
			error (0, "%s has incomplete type", name);
			size = 1;
		}
		def->offset = defspace_new_loc (space, size);
	}
	if (space) {
		def->space = space;
		*space->def_tail = def;
		space->def_tail = &def->next;
	}

	def->file = pr.source_file;
	def->line = pr.source_line;

	set_storage_bits (def, storage);

	return def;
}

def_t *
alias_def (def_t *def, type_t *type)
{
	def_t      *alias;

	ALLOC (16384, def_t, defs, alias);
	alias->offset = def->offset;
	alias->type = type;
	alias->alias = def;
	return alias;
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
	int         count, i, num_params, base_offset;
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
		num_params = i;
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
		num_params = i;
	} else {
		error (eles, "invalid initializer");
		return;
	}
	for (count = 0, e = eles->e.block.head; e; count++, e = e->next) {
		convert_name (e);
		if (e->type == ex_nil && count < num_params)
			convert_nil (e, elements[count].type);
		if (e->type == ex_error) {
			free (elements);
			return;
		}
	}
	if (count > num_params) {
		if (options.warnings.initializer)
			warning (eles, "excessive elements in initializer");
		count = num_params;
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
		} else if (c->type == ex_value) {
			if (c->e.value.type == ev_integer
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
			if (c->e.value.type == ev_string) {
				EMIT_STRING (def->space, g->string_var,
							 c->e.value.v.string_val);
			} else {
				memcpy (g, &c->e, type_size (get_type (c)) * 4);
			}
		}
	}
	free (elements);
}

void
initialize_def (symbol_t *sym, type_t *type, expr_t *init, defspace_t *space,
				storage_class_t storage)
{
	symbol_t   *check = symtab_lookup (current_symtab, sym->name);
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
			if (storage == st_extern) {
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
	if (storage == st_global && init && is_scalar (type)) {
		sym->sy_type = sy_const;
		memset (&sym->s.value, 0, sizeof (&sym->s.value));
		if (init->type != ex_value) {	//FIXME arrays/structs
			error (0, "non-constant initializier");
		} else {
			sym->s.value = init->e.value;
		}
		return;
	}
	if (sym->s.def && sym->s.def->external) {
		free_def (sym->s.def);
		sym->s.def = 0;
	}
	if (!sym->s.def)
		sym->s.def = new_def (sym->name, type, space, storage);
	if (storage == st_extern) {
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
	if (is_array (type) || is_struct (type)) {
		init_elements (sym->s.def, init);
	} else {
		if (!type_assignable (type, get_type (init))) {
			error (init, "type mismatch in initializer");
			return;
		}
		if (local_expr) {
			append_expr (local_expr,
						 assign_expr (new_symbol_expr (sym), init));
		} else {
			if (init->type != ex_value) {	//FIXME enum etc
				error (0, "non-constant initializier");
				return;
			}
			memcpy (D_POINTER (void, sym->s.def), &init->e.value,
					type_size (type) * sizeof (pr_type_t));
		}
	}
}
