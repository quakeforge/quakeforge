/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/
static const char rcsid[] =
	"$Id$";

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
#include "expr.h"
#include "struct.h"
#include "type.h"

typedef struct locref_s {
	struct locref_s *next;
	int         ofs;
} locref_t;

def_t       def_void = { &type_void, "temp" };
def_t       def_function = { &type_function, "temp" };

def_t       def_ret, def_parms[MAX_PARMS];

static def_t *free_temps[4];			// indexted by type size
static def_t temp_scope;
static def_t *free_defs;
static defspace_t *free_spaces;
static scope_t *free_scopes;
static locref_t *free_locs[4];			// indexted by type size
static locref_t *free_free_locs;

static hashtab_t *defs_by_name;

static const char *
defs_get_key (void *_def, void *_tab)
{
	def_t      *def = (def_t *) _def;
	hashtab_t **tab = (hashtab_t **) _tab;

	if (tab == &defs_by_name) {
		return def->name;
	}
	return "";
}

static def_t *
check_for_name (type_t *type, const char *name, scope_t *scope, int allocate)
{
	def_t      *def;

	if (!defs_by_name) {
		defs_by_name = Hash_NewTable (16381, defs_get_key, 0, &defs_by_name);
	}
	if (!name)
		return 0;
	if (scope->type == sc_static && (find_struct (name) || get_enum (name))) {
		error (0, "%s redeclared", name);
		return 0;
	}
	// see if the name is already in use
	def = (def_t *) Hash_Find (defs_by_name, name);
	if (def) {
		if (allocate && scope == def->scope)
			if (type && def->type != type)
				error (0, "Type mismatch on redeclaration of %s", name);
		if (!allocate || def->scope == scope)
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

/*
	get_def

	If type is NULL, it will match any type
	If allocate is true, a new def will be allocated if it can't be found
*/
def_t *
get_def (type_t *type, const char *name, scope_t *scope, int allocate)
{
	def_t      *def = check_for_name (type, name, scope, allocate);
	int         size;

	if (def || !allocate)
		return def;

	// allocate a new def
	def = new_def (type, name, scope);
	if (name)
		Hash_Add (defs_by_name, def);

	// FIXME: need to sort out location re-use
	def->ofs = scope->space->size;

	/* 
		make automatic defs for the vectors elements .origin can be accessed
		as .origin_x, .origin_y, and .origin_z
	*/
	if (type->type == ev_vector && name) {
		def_t      *d;

		d = get_def (&type_float, va ("%s_x", name), scope, allocate);
		d->used = 1;
		d->parent = def;

		d = get_def (&type_float, va ("%s_y", name), scope, allocate);
		d->used = 1;
		d->parent = def;

		d = get_def (&type_float, va ("%s_z", name), scope, allocate);
		d->used = 1;
		d->parent = def;
	} else {
		scope->space->size += type_size (type);
	}

	if (type->type == ev_field) {
		G_INT (def->ofs) = pr.size_fields;

		if (type->aux_type->type == ev_vector) {
			def_t      *d;

			d = get_def (&type_floatfield,
						   va ("%s_x", name), scope, allocate);
			d->used = 1;				// always `used'
			d->parent = def;

			d = get_def (&type_floatfield,
						   va ("%s_y", name), scope, allocate);
			d->used = 1;				// always `used'
			d->parent = def;

			d = get_def (&type_floatfield,
						   va ("%s_z", name), scope, allocate);
			d->used = 1;				// always `used'
			d->parent = def;
		} else if (type->aux_type->type == ev_pointer) {
			//FIXME I don't think this is right for a field pointer
			size = type_size  (type->aux_type->aux_type);
			pr.size_fields += type->aux_type->num_parms * size;
		} else {
			size = type_size  (type->aux_type);
			pr.size_fields += size;
		}
	} else if (type->type == ev_pointer && type->num_parms) {
		int         ofs = scope->space->size;

		size = type_size  (type->aux_type);
		scope->space->size += type->num_parms * size;

		if (scope->type != sc_static) {
			expr_t     *e1 = new_expr ();
			expr_t     *e2 = new_expr ();

			e1->type = ex_def;
			e1->e.def = def;

			e2->type = ex_def;
			e2->e.def = new_def (type->aux_type, 0, scope);
			e2->e.def->ofs = ofs;

			append_expr (local_expr,
						 new_binary_expr ('=', e1, address_expr (e2, 0, 0)));
		} else {
			G_INT (def->ofs) = ofs;
			def->constant = 1;
		}
		def->initialized = 1;
	}

	return def;
}

def_t *
new_def (type_t *type, const char *name, scope_t *scope)
{
	def_t      *def;

	ALLOC (16384, def_t, defs, def);

	*scope->tail = def;
	scope->tail = &def->def_next;
	scope->num_defs++;

	def->return_addr = __builtin_return_address (0);

	def->name = name ? strdup (name) : 0;
	def->type = type;

	def->scope = scope;

	def->file = s_file;
	def->line = pr_source_line;

	return def;
}

int
new_location (type_t *type, defspace_t *space)
{
	int         size = type_size (type);
	locref_t   *loc;

	if (free_locs[size]) {
		loc = free_locs[size];
		free_locs[size] = loc->next;

		loc->next = free_free_locs;
		free_free_locs = loc;

		return loc->ofs;
	}
	space->size += size;
	return space->size - size;
}

void
free_location (def_t *def)
{
	int         size = type_size (def->type);
	locref_t   *loc;

	ALLOC (1024, locref_t, free_locs, loc);
	loc->ofs = def->ofs;
	loc->next = free_locs[size];
	free_locs[size] = loc;
}

def_t *
get_tempdef (type_t *type, scope_t *scope)
{
	int         size = type_size (type);
	def_t      *def;

	if (free_temps[size]) {
		def = free_temps[size];
		free_temps[size] = def->next;
		def->type = type;
	} else {
		def = new_def (type, 0, scope);
		def->ofs = scope->space->size;
		scope->space->size += size;
	}
	def->freed = def->removed = def->users = 0;
	def->next = temp_scope.next;
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

			if (d->users < 0)
				printf ("%s:%d: warning: %s %3d %3d %d\n", pr.strings + d->file,
						d->line, pr_type_name[d->type->type], d->ofs, d->users,
						d->managed);
			size = type_size (d->type);
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
	int         i;
	def_t      *d;

	for (i = 0; i < sizeof (free_temps) / sizeof (free_temps[0]); i++) {
		free_temps[i] = 0;
	}

	for (d = temp_scope.next; d; d = d->next)
		printf ("%s:%d: warning: %s %3d %3d %d\n", pr.strings + d->file, d->line,
				pr_type_name[d->type->type], d->ofs, d->users, d->managed);
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
				warning (&e, "unused variable `%s'", def->name);
			}
			if (!def->removed) {
				Hash_Del (defs_by_name, def->name);
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
