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


def_t		*pr_global_defs[MAX_REGS];	// to find def for a global variable
int 		pr_edict_size;
static def_t *free_temps[ev_type_count];
static def_t temp_scope;

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
PR_GetDef (type_t *type, const char *name, def_t *scope, qboolean allocate)
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
				PR_ParseError ("Type mismatch on redeclaration of %s", name);
		if (!allocate || def->scope == scope)
			return def;
	}

	if (!allocate)
		return NULL;

	// allocate a new def
	def = PR_NewDef (type, name, scope);
	Hash_Add (defs_by_name, def);

	def->ofs = numpr_globals;
	pr_global_defs[numpr_globals] = def;

	/*
		make automatic defs for the vectors elements
		.origin can be accessed as .origin_x, .origin_y, and .origin_z
	*/
	if (type->type == ev_vector) {
		sprintf (element, "%s_x", name);
		PR_GetDef (&type_float, element, scope, true);

		sprintf (element, "%s_y", name);
		PR_GetDef (&type_float, element, scope, true);

		sprintf (element, "%s_z", name);
		PR_GetDef (&type_float, element, scope, true);
	} else {
		numpr_globals += type_size[type->type];
	}

	if (type->type == ev_field) {
		*(int *) &pr_globals[def->ofs] = pr.size_fields;

		if (type->aux_type->type == ev_vector) {
			sprintf (element, "%s_x", name);
			PR_GetDef (&type_floatfield, element, scope, true);

			sprintf (element, "%s_y", name);
			PR_GetDef (&type_floatfield, element, scope, true);

			sprintf (element, "%s_z", name);
			PR_GetDef (&type_floatfield, element, scope, true);
		} else {
			pr.size_fields += type_size[type->aux_type->type];
		}
	}

//	if (pr_dumpasm)
//		PR_PrintOfs (def->ofs);

	return def;
}

def_t *
PR_NewDef (type_t *type, const char *name, def_t *scope)
{
	def_t *def;

	def = calloc (1, sizeof (def_t));

	if (name) {
		pr.def_tail->next = def;
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

def_t *
PR_GetTempDef (type_t *type)
{
	int t = type->type;
	def_t *d;
	if (free_temps[t]) {
		d = free_temps[t];
		free_temps[t] = d->next;
	} else {
		d = PR_NewDef (type, 0, 0);
	}
	if (!d->ofs) {
		d->ofs = numpr_globals;
		numpr_globals += type_size[t];
	}
	d->scope_next = temp_scope.scope_next;
	temp_scope.scope_next = d;
	return d;
}

void
PR_FreeTempDefs (void)
{
	def_t *def;
	etype_t type;

	for (def = temp_scope.scope_next; def; def = def->scope_next) {
		type = def->type->type;
		def->next = free_temps[type];
		free_temps[type] = def;
	}
	temp_scope.scope_next = 0;
}

void
PR_ResetTempDefs (void)
{
	int i;
	def_t *d;

	for (i = 0; i < ev_type_count; i++) {
		for (d = free_temps[i]; d && d->ofs; d = d->next) {
			d->ofs = 0;
		}
	}
}

void
PR_FlushScope (def_t *scope)
{
	def_t *def;

	for (def = scope->scope_next; def; def = def->scope_next) {
		Hash_Del (defs_by_name, def->name);
	}
}
