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
#include "expr.h"
#include "immediate.h"
#include "options.h"
#include "reloc.h"
#include "strpool.h"
#include "struct.h"
#include "type.h"

typedef struct locref_s {
	struct locref_s *next;
	int         ofs;
	int         size;
} locref_t;

def_t       def_void = { &type_void, "def void" };
def_t       def_function = { &type_function, "def function" };

static def_t *free_temps[4];			// indexted by type size
static int tempdef_counter;
static def_t temp_scope;
static def_t *free_defs;
static defspace_t *free_spaces;
static scope_t *free_scopes;
static locref_t *free_free_locs;

static hashtab_t *defs_by_name;
static hashtab_t *field_defs;

static const char *
defs_get_key (void *_def, void *unused)
{
	def_t      *def = (def_t *) _def;
	return def->name;
}

static def_t *
check_for_name (type_t *type, const char *name, scope_t *scope,
				storage_class_t storage)
{
	def_t      *def;

	if (!defs_by_name) {
		defs_by_name = Hash_NewTable (16381, defs_get_key, 0, 0);
		field_defs = Hash_NewTable (16381, defs_get_key, 0, 0);
	}
	if (!name)
		return 0;
	if (scope->type == sc_global && get_enum (name)) {
		error (0, "%s redeclared", name);
		return 0;
	}
	// see if the name is already in use
	def = (def_t *) Hash_Find (defs_by_name, name);
	if (def) {
		if (storage != st_none && scope == def->scope)
			if (type && def->type != type) {
				expr_t     *e = new_expr ();
				e->line = def->line;
				e->file = def->file;
				error (0, "Type mismatch on redeclaration of %s", name);
				error (e, "previous declaration");
			}
		if (storage == st_none || def->scope == scope)
			return def;
	}
	return 0;
}

static int
grow_space (defspace_t *space)
{
	space->max_size += 1024;
	return 1;
}

defspace_t *
new_defspace (void)
{
	defspace_t *space;

	ALLOC (1024, defspace_t, spaces, space);
	space->grow = grow_space;
	return space;
}

void
defspace_adddata (defspace_t *space, pr_type_t *data, int size)
{
	if (space->size + size > space->max_size) {
		space->max_size = (space->size + size + 1023) & ~1023;
		space->data = realloc (space->data,
							   space->max_size * sizeof (pr_type_t));
	}
	if (data)
		memcpy (space->data + space->size, data, size * sizeof (pr_type_t));
	space->size += size;
}

scope_t *
new_scope (scope_type type, defspace_t *space, scope_t *parent)
{
	scope_t    *scope;

	ALLOC (1024, scope_t, scopes, scope);
	scope->type = type;
	scope->space = space;
	scope->parent = parent;
	scope->tail = &scope->head;
	return scope;
}

void
set_storage_bits (def_t *def, storage_class_t storage)
{
	switch (storage) {
		case st_none:
			break;
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

static const char *vector_component_names[] = {"%s_x", "%s_y", "%s_z"};

static void
vector_component (int is_field, def_t *vec, int comp, scope_t *scope,
				  storage_class_t storage)
{
	def_t      *d;
	const char *name;

	name = save_string (va (vector_component_names[comp], vec->name));
	d = get_def (is_field ? &type_floatfield : &type_float, name, scope,
				 st_none);
	if (d && d->scope == scope) {
		if (vec->external) {
			error (0, "internal error");
			abort ();
		}
	} else {
		d = new_def (is_field ? &type_floatfield : &type_float, name, scope);
	}
	d->used = 1;
	d->parent = vec;
	d->ofs = vec->ofs + comp;
	set_storage_bits (d, storage);
	if (is_field && (storage == st_global || storage == st_static)) {
		G_INT (d->ofs) = G_INT (vec->ofs) + comp;
		reloc_def_field (d, d->ofs);
	}
	Hash_Add (defs_by_name, d);
	if (is_field)
		Hash_Add (field_defs, d);
}

def_t *
field_def (const char *name)
{
	return Hash_Find (field_defs, name);
}

/*
	get_def

	If type is NULL, it will match any type
	If allocate is true, a new def will be allocated if it can't be found
*/
def_t *
get_def (type_t *type, const char *name, scope_t *scope,
		 storage_class_t storage)
{
	def_t      *def = check_for_name (type, name, scope, storage);
	defspace_t *space = NULL;

	if (storage == st_none)
		return def;

	if (def) {
		if (storage != st_extern && !def->initialized) {
			def->file = pr.source_file;
			def->line = pr.source_line;
		}
		if (!def->external || storage == st_extern)
			return def;
	} else {
		// allocate a new def
		def = new_def (type, name, scope);
		if (name)
			Hash_Add (defs_by_name, def);
	}

	switch (storage) {
		case st_none:
		case st_global:
		case st_local:
		case st_system:
			space = scope->space;
			break;
		case st_extern:
			space = 0;
			break;
		case st_static:
			space = pr.near_data;
			break;
	}
	if (space) {
		if (type->type == ev_field && type->aux_type == &type_vector)
			def->ofs = new_location (type->aux_type, space);
		else
			def->ofs = new_location (type, space);
	}

	set_storage_bits (def, storage);

	if (name) {
		// make automatic defs for the vectors elements .origin can be accessed
		// as .origin_x, .origin_y, and .origin_z
		if (type->type == ev_vector) {
			vector_component (0, def, 0, scope, storage);
			vector_component (0, def, 1, scope, storage);
			vector_component (0, def, 2, scope, storage);
		}

		if (type->type == ev_field) {
			if (storage == st_global || storage == st_static) {
				G_INT (def->ofs) = new_location (type->aux_type,
												 pr.entity_data);
				reloc_def_field (def, def->ofs);
				def->constant = 1;
				def->nosave = 1;
			}

			if (type->aux_type->type == ev_vector) {
				vector_component (1, def, 0, scope, storage);
				vector_component (1, def, 1, scope, storage);
				vector_component (1, def, 2, scope, storage);
			}
			Hash_Add (field_defs, def);
		}
	}

	return def;
}

def_t *
new_def (type_t *type, const char *name, scope_t *scope)
{
	def_t      *def;

	ALLOC (16384, def_t, defs, def);

	if (scope) {
		*scope->tail = def;
		scope->tail = &def->def_next;
		scope->num_defs++;
	}

	def->return_addr = __builtin_return_address (0);

	def->name = name ? save_string (name) : 0;
	def->type = type;

	if (scope) {
		def->scope = scope;
		def->space = scope->space;
	}

	def->file = pr.source_file;
	def->line = pr.source_line;

	return def;
}

int
new_location_size (int size, defspace_t *space)
{
	int         ofs;
	locref_t   *loc;
	locref_t  **l = &space->free_locs;

	while (*l && (*l)->size < size)
		l = &(*l)->next;
	if ((loc = *l)) {
		ofs = loc->ofs;
		if (loc->size == size) {
			*l = loc->next;
		} else {
			loc->ofs += size;
			loc->size -= size;
		}
		return ofs;
	}
	ofs = space->size;
	space->size += size;
	if (space->size > space->max_size) {
		if (!space->grow) {
			error (0, "unable to allocate %d globals", size);
			exit (1);
		}
		space->grow (space);
	}
	return ofs;
}

int
new_location (type_t *type, defspace_t *space)
{
	return new_location_size (type_size (type), space);
}

void
free_location (def_t *def)
{
	int         size = type_size (def->type);
	locref_t   *loc;

	for (loc = def->space->free_locs; loc; loc = loc->next) {
		if (def->ofs + size == loc->ofs) {
			loc->size += size;
			loc->ofs = def->ofs;
			return;
		} else if (loc->ofs + loc->size == def->ofs) {
			loc->size += size;
			if (loc->next && loc->next->ofs == loc->ofs + loc->size) {
				loc->size += loc->next->size;
				loc->next = loc->next->next;
			}
			return;
		}
	}
	ALLOC (1024, locref_t, free_locs, loc);
	loc->ofs = def->ofs;
	loc->size = size;
	loc->next = def->space->free_locs;
	def->space->free_locs = loc;
}

def_t *
get_tempdef (type_t *type, scope_t *scope)
{
	int         size = type_size (type) - 1;
	def_t      *def;

	if (free_temps[size]) {
		def = free_temps[size];
		free_temps[size] = def->next;
		def->type = type;
	} else {
		def = new_def (type, va (".tmp%d", tempdef_counter++), scope);
		def->ofs = new_location (type, scope->space);
	}
	def->return_addr = __builtin_return_address (0);
	def->freed = def->removed = def->users = 0;
	def->next = temp_scope.next;
	set_storage_bits (def, st_local);
	temp_scope.next = def;
	return def;
}

void
free_tempdefs (void)
{
	def_t     **def, *d;
	int         size;

	def = &temp_scope.next;
	while (*def) {
		if ((*def)->users <= 0) {
			d = *def;
			*def = d->next;

			if (d->users < 0) {
				expr_t      e;
				e.file = d->file;
				e.line = d->line;
				notice (&e, "temp def over-freed:%s offs:%d users:%d "
							"managed:%d %s",
						pr_type_name[d->type->type],
						d->ofs, d->users, d->managed,
						d->name);
			}
			size = type_size (d->type) - 1;
			if (d->expr)
				d->expr->e.temp.def = 0;

			if (!d->freed) {
				d->next = free_temps[size];
				free_temps[size] = d;
				d->freed = 1;
			}
		} else {
			def = &(*def)->next;
		}
	}
}

void
reset_tempdefs (void)
{
	size_t      i;
	def_t      *d;

	tempdef_counter = 0;
	for (i = 0; i < sizeof (free_temps) / sizeof (free_temps[0]); i++) {
		free_temps[i] = 0;
	}

	for (d = temp_scope.next; d; d = d->next) {
		expr_t      e;
		e.file = d->file;
		e.line = d->line;
		notice (&e, "temp def under-freed:%s ofs:%d users:%d managed:%d %s",
				pr_type_name[d->type->type],
				d->ofs, d->users, d->managed, d->name);
	}
	temp_scope.next = 0;
}

void
flush_scope (scope_t *scope, int force_used)
{
	def_t      *def;

	for (def = scope->head; def; def = def->def_next) {
		if (def->name) {
			if (!force_used && !def->used) {
				expr_t      e;

				e.line = def->line;
				e.file = def->file;
				if (options.warnings.unused)
					warning (&e, "unused variable `%s'", def->name);
			}
			if (!def->removed) {
				Hash_Del (defs_by_name, def->name);
				if (def->type->type == ev_field) {
					if (Hash_Find (field_defs, def->name) == def)
						Hash_Del (field_defs, def->name);
				}
				def->removed = 1;
			}
		}
	}
}

void
def_initialized (def_t *d)
{
	d->initialized = 1;
	if (d->type == &type_vector
		|| (d->type->type == ev_field && d->type->aux_type == &type_vector)) {
		d = d->def_next;
		d->initialized = 1;
		d = d->def_next;
		d->initialized = 1;
		d = d->def_next;
		d->initialized = 1;
	}
	if (d->parent) {
		d = d->parent;
		if (d->type == &type_vector
			&& d->def_next->initialized
			&& d->def_next->def_next->initialized
			&& d->def_next->def_next->def_next->initialized)
			d->initialized = 1;
	}
}

void
clear_defs (void)
{
	if (field_defs)
		Hash_FlushTable (field_defs);
	if (defs_by_name)
		Hash_FlushTable (defs_by_name);
}

void
def_to_ddef (def_t *def, ddef_t *ddef, int aux)
{
	type_t     *type = def->type;

	if (aux)
		type = type->aux_type;
	ddef->type = type->type;
	ddef->ofs = def->ofs;
	ddef->s_name = ReuseString (def->name);
}
