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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <QF/hash.h>

#include "qfcc.h"

typedef struct locref_s {
	struct locref_s *next;
	int ofs;
} locref_t;

def_t		*pr_global_defs[MAX_REGS];	// to find def for a global variable
static def_t *free_temps[4];	// indexted by type size
static def_t temp_scope;
static locref_t *free_locs[4];	// indexted by type size
static locref_t *free_free_locs;

static hashtab_t  *defs_by_name;

static const char *
defs_get_key (void *_def, void *_tab)
{
	def_t		*def = (def_t*)_def;
	hashtab_t	**tab = (hashtab_t**) _tab;

	if (tab == &defs_by_name) {
		return def->name;
	}
	return "";
}

/*
	PR_GetDef

	If type is NULL, it will match any type
	If allocate is true, a new def will be allocated if it can't be found
*/
def_t *
PR_GetDef (type_t *type, const char *name, def_t *scope, int *allocate)
{
	def_t	*def;
	char	element[MAX_NAME];

	if (!defs_by_name) {
		defs_by_name = Hash_NewTable (16381, defs_get_key, 0, &defs_by_name);
	}

	// see if the name is already in use
	def = (def_t*) Hash_Find (defs_by_name, name);
	if (def) {
		if (allocate && scope == def->scope)
			if (type && def->type != type)
				error (0, "Type mismatch on redeclaration of %s", name);
		if (!allocate || def->scope == scope)
			return def;
	}

	if (!allocate)
		return NULL;

	// allocate a new def
	def = PR_NewDef (type, name, scope);
	Hash_Add (defs_by_name, def);

	//FIXME: need to sort out location re-use
	def->ofs = *allocate;
	pr_global_defs[*allocate] = def;

	/*
		make automatic defs for the vectors elements
		.origin can be accessed as .origin_x, .origin_y, and .origin_z
	*/
	if (type->type == ev_vector) {
		sprintf (element, "%s_x", name);
		PR_GetDef (&type_float, element, scope, allocate);

		sprintf (element, "%s_y", name);
		PR_GetDef (&type_float, element, scope, allocate);

		sprintf (element, "%s_z", name);
		PR_GetDef (&type_float, element, scope, allocate);
	} else {
		*allocate += type_size[type->type];
	}

	if (type->type == ev_field) {
		*(int *) &pr_globals[def->ofs] = pr.size_fields;

		if (type->aux_type->type == ev_vector) {
			sprintf (element, "%s_x", name);
			PR_GetDef (&type_floatfield, element, scope, allocate);

			sprintf (element, "%s_y", name);
			PR_GetDef (&type_floatfield, element, scope, allocate);

			sprintf (element, "%s_z", name);
			PR_GetDef (&type_floatfield, element, scope, allocate);
		} else {
			pr.size_fields += type_size[type->aux_type->type];
		}
	}

	return def;
}

def_t *
PR_NewDef (type_t *type, const char *name, def_t *scope)
{
	def_t *def;

	def = calloc (1, sizeof (def_t));

	if (name) {
		pr.def_tail->def_next = def;
		pr.def_tail = def;
	}

	if (scope) {
		def->scope_next = scope->scope_next;
		scope->scope_next = def;
	}

	def->name = name ? strdup (name) : 0;
	def->type = type;

	def->scope = scope;

	return def;
}

int
PR_NewLocation (type_t *type)
{
	int size = type_size[type->type];
	locref_t *loc;

	if (free_locs[size]) {
		loc = free_locs[size];
		free_locs[size] = loc->next;

		loc->next = free_free_locs;
		free_free_locs = loc;

		return loc->ofs;
	}
	numpr_globals += size;
	return numpr_globals - size;
}

void
PR_FreeLocation (def_t *def)
{
	int size = type_size[def->type->type];
	locref_t *loc;

	if (!free_free_locs) {
		free_free_locs = malloc (256 * sizeof (locref_t));
		for (loc = free_free_locs; loc - free_free_locs < 255; loc++)
			loc->next = loc + 1;
		loc->next = 0;
	}
	loc = free_free_locs;
	free_free_locs = loc->next;
	loc->ofs = def->ofs;
	loc->next = free_locs[size];
	free_locs[size] = loc;
}

def_t *
PR_GetTempDef (type_t *type, def_t *scope)
{
	int size = type_size[type->type];
	def_t *def;
	if (free_temps[size]) {
		def = free_temps[size];
		free_temps[size] = def->next;
		def->type = type;
	} else {
		def = PR_NewDef (type, 0, scope);
		def->ofs = scope->num_locals;
		scope->num_locals += type_size[size];
	}
	def->users = 0;
	def->next = temp_scope.next;
	temp_scope.next = def;
	return def;
}

void
PR_FreeTempDefs (void)
{
	def_t **def, *d;
	int size;

	def = &temp_scope.next;
	while (*def) {
		if ((*def)->users <= 0) {
			d = *def;
			*def = d->next;

			size = type_size[d->type->type];
			if (d->expr)
				d->expr->e.temp.def = 0;

			d->next = free_temps[size];
			free_temps[size] = d;
		} else {
			def = &(*def)->next;
		}
	}
}

void
PR_ResetTempDefs (void)
{
	int i;
	def_t *d;

	for (i = 0; i < sizeof (free_temps) / sizeof (free_temps[0]); i++) {
		free_temps[i] = 0;
	}

	for (d = temp_scope.next; d; d = d->next)
		printf ("%3d %3d\n", d->ofs, d->users);
	temp_scope.next = 0;
}

void
PR_FlushScope (def_t *scope)
{
	def_t *def;

	for (def = scope->scope_next; def; def = def->scope_next) {
		if (def->name)
			Hash_Del (defs_by_name, def->name);
	}
}
