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
#include "QF/hash.h"

#include "qfcc.h"

static hashtab_t  *string_imm_defs;
static hashtab_t  *float_imm_defs;
static hashtab_t  *vector_imm_defs;

static char *
string_imm_get_key (void *_def, void *unused)
{
	def_t       *def = (def_t*)_def;
	return G_STRING (def->ofs);
}

static char *
float_imm_get_key (void *_def, void *unused)
{
	def_t       *def = (def_t*)_def;
	static char rep[20];
	sprintf (rep, "\001float:%08X\001", G_INT(def->ofs));
	return rep;
}

static char *
vector_imm_get_key (void *_def, void *unused)
{
	def_t       *def = (def_t*)_def;
	static char rep[60];
	sprintf (rep, "\001vector:%08X\001%08X\001%08X\001",
			 G_INT(def->ofs), G_INT(def->ofs+1), G_INT(def->ofs+2));
	return rep;
}

/*
	PR_ParseImmediate

	Looks for a preexisting constant
*/
def_t *
PR_ParseImmediate (def_t *def)
{
	def_t	*cn = 0;
	char rep[60];
	hashtab_t *tab = 0;

	if (!string_imm_defs) {
		string_imm_defs = Hash_NewTable (16381, string_imm_get_key, 0, 0);
		float_imm_defs = Hash_NewTable (16381, float_imm_get_key, 0, 0);
		vector_imm_defs = Hash_NewTable (16381, vector_imm_get_key, 0, 0);
	}
	if (pr_immediate_type == &type_string) {
		cn = (def_t*) Hash_Find (string_imm_defs, pr_immediate_string);
		tab = string_imm_defs;
		//printf ("%s\n",pr_immediate_string);
	} else if (pr_immediate_type == &type_float) {
		sprintf (rep, "\001float:%08X\001", *(int*)&pr_immediate._float);
		cn = (def_t*) Hash_Find (float_imm_defs, rep);
		tab = float_imm_defs;
		//printf ("%f\n",pr_immediate._float);
	} else if (pr_immediate_type == &type_vector) {
		sprintf (rep, "\001vector:%08X\001%08X\001%08X\001",
				 *(int*)&pr_immediate.vector[0],
				 *(int*)&pr_immediate.vector[1],
				 *(int*)&pr_immediate.vector[2]);
		cn = (def_t*) Hash_Find (vector_imm_defs, rep);
		tab = vector_imm_defs;
		//printf ("%f %f %f\n",pr_immediate.vector[0], pr_immediate.vector[1], pr_immediate.vector[2]);
	} else {
		PR_ParseError ("weird immediate type");
	}
	if (cn) {
		if (cn->type != pr_immediate_type) printf("urk\n");
		//printf("found\n");
		PR_Lex ();
		return cn;
	}

	// allocate a new one
	// always share immediates
	if (def) {
		cn = def;
	} else {
		cn = PR_NewDef (pr_immediate_type, "IMMEDIATE", 0);
		cn->ofs = numpr_globals;
		pr_global_defs[cn->ofs] = cn;
		numpr_globals += type_size[pr_immediate_type->type];
	}
	cn->initialized = 1;
	// copy the immediate to the global area
	if (pr_immediate_type == &type_string)
		pr_immediate.string = CopyString (pr_immediate_string);

	memcpy (pr_globals + cn->ofs, &pr_immediate,
			4 * type_size[pr_immediate_type->type]);

	Hash_Add (tab, cn);

	PR_Lex ();

	return cn;
}
