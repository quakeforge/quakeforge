/*
	pr_edict.c

	entity dictionary

	Copyright (C) 1996-1997  Id Software, Inc.

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
#include <stdarg.h>
#include <stdio.h>

#include "QF/cmd.h"
#include "QF/crc.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/qdefs.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/zone.h"
#include "QF/va.h"
#include "QF/vfs.h"

#include "compat.h"

cvar_t     *pr_boundscheck;
cvar_t     *pr_deadbeef_ents;
cvar_t     *pr_deadbeef_locals;

int         pr_type_size[ev_type_count] = {
	1,			// ev_void
	1,			// ev_string
	1,			// ev_float
	3,			// ev_vector
	1,			// ev_entity
	1,			// ev_field
	1,			// ev_func
	1,			// ev_pointer
	4,			// ev_quaternion
	1,			// ev_integer
	1,			// ev_uinteger
	0,			// ev_short        value in opcode
	0,			// ev_struct       variable
	0,			// ev_object       variable
	0,			// ev_class        variable
	2,			// ev_sel
	0,			// ev_array        variable
};

const char *pr_type_name[ev_type_count] = {
	"void",
	"string",
	"float",
	"vector",
	"entity",
	"field",
	"function",
	"pointer",
	"quaternion",
	"integer",
	"uinteger",
	"short",
	"struct",
	"object",
	"Class",
	"SEL",
	"array",
};

ddef_t     *ED_FieldAtOfs (progs_t * pr, int ofs);
qboolean    ED_ParseEpair (progs_t * pr, pr_type_t *base, ddef_t *key, const char *s);

/*
	ED_ClearEdict

	Sets everything to NULL
*/
void
ED_ClearEdict (progs_t * pr, edict_t *e, int val)
{
	int i;

	if (NUM_FOR_EDICT(pr,e)<*pr->reserved_edicts)
		Sys_Printf("clearing reserved edict %d\n", NUM_FOR_EDICT(pr,e));
	for (i=0; i < pr->progs->entityfields; i++)
		e->v[i].integer_var = val;
	e->free = false;
}

/*
	ED_Alloc

	Either finds a free edict, or allocates a new one.
	Try to avoid reusing an entity that was recently freed, because it
	can cause the client to think the entity morphed into something else
	instead of being removed and recreated, which can cause interpolated
	angles and bad trails.
*/
edict_t    *
ED_Alloc (progs_t * pr)
{
	int         i;
	edict_t    *e;
	int         start = pr->reserved_edicts ? *pr->reserved_edicts : 0;
	int         max_edicts = pr->pr_edictareasize / pr->pr_edict_size;

	for (i = start + 1; i < *(pr)->num_edicts; i++) {
		e = EDICT_NUM (pr, i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && (e->freetime < 2 || *(pr)->time - e->freetime > 0.5)) {
			ED_ClearEdict (pr, e, 0);
			return e;
		}
	}

	if (i == max_edicts) {
		Sys_Printf ("WARNING: ED_Alloc: no free edicts\n");
		i--;							// step on whatever is the last edict
		e = EDICT_NUM (pr, i);
		if (pr->unlink)
			pr->unlink (e);
	} else
		(*(pr)->num_edicts)++;
	e = EDICT_NUM (pr, i);
	ED_ClearEdict (pr, e, 0);

	return e;
}

/*
	ED_FreeRefs

	NULLs all references to entity
*/
void
ED_FreeRefs (progs_t * pr, edict_t *ed)
{
	int i, j, k;
	ddef_t *def;
	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		def = &pr->pr_globaldefs[i];
		if ((def->type & ~DEF_SAVEGLOBAL) == ev_entity) {
			if (ed == G_EDICT (pr, def->ofs)) {
				Sys_Printf ("Reference found to free'd edict: %p\n", ed);
			}
		}
	}
	for (i = 0; i < pr->progs->numfielddefs; i++) {
		def = &pr->pr_fielddefs[i];
		if ((def->type & ~DEF_SAVEGLOBAL) == ev_entity) {
			for (j = 0; j < *pr->num_edicts; j++) {
				k = E_var (EDICT_NUM (pr, j), def->ofs, entity);
				if (ed == PROG_TO_EDICT (pr, k))
					Sys_Printf ("Reference found to free'd edict: %p\n", ed);
			}
		}
	}
}

/*
	ED_Free

	Marks the edict as free
	FIXME: walk all entities and NULL out references to this entity
*/
void
ED_Free (progs_t * pr, edict_t *ed)
{
	if (pr->unlink)
		pr->unlink (ed);				// unlink from world bsp

	if (pr_deadbeef_ents->int_val) {
		ED_ClearEdict (pr, ed, 0xdeadbeef);
	} else {
		if (pr->free_edict)
			pr->free_edict (pr, ed);
		else
			ED_ClearEdict (pr, ed, 0);
	}
	ed->free = true;
	ed->freetime = *(pr)->time;
}

//===========================================================================

/*
	PR_ValueString

	Returns a string describing *data in a type specific manner
*/
char       *
PR_ValueString (progs_t * pr, etype_t type, pr_type_t *val)
{
	static char line[256];
	ddef_t     *def;
	dfunction_t *f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
		case ev_string:
			snprintf (line, sizeof (line), "%s",
					  PR_GetString (pr, val->string_var));
			break;
		case ev_entity:
			snprintf (line, sizeof (line), "entity %i",
					  NUM_FOR_BAD_EDICT (pr, PROG_TO_EDICT (pr, val->entity_var)));
			break;
		case ev_func:
			f = pr->pr_functions + val->func_var;
			snprintf (line, sizeof (line), "%s()",
					  PR_GetString (pr, f->s_name));
			break;
		case ev_field:
			def = ED_FieldAtOfs (pr, val->integer_var);
			snprintf (line, sizeof (line), ".%s",
					  PR_GetString (pr, def->s_name));
			break;
		case ev_void:
			strcpy (line, "void");
			break;
		case ev_float:
			snprintf (line, sizeof (line), "%5.1f", val->float_var);
			break;
		case ev_vector:
			snprintf (line, sizeof (line), "'%5.1f %5.1f %5.1f'",
					  val->vector_var[0], val->vector_var[1],
					  val->vector_var[2]);
			break;
		case ev_pointer:
			snprintf (line, sizeof (line), "[%d]", val->integer_var);
			break;
		case ev_quaternion:
			snprintf (line, sizeof (line), "'%5.1f %5.1f %5.1f %5.1f'",
					  val->vector_var[0], val->vector_var[1],
					  val->vector_var[2], val->vector_var[3]);
			break;
		case ev_integer:
			snprintf (line, sizeof (line), "%d", val->integer_var);
			break;
		case ev_uinteger:
			snprintf (line, sizeof (line), "%u", val->uinteger_var);
			break;
		default:
			snprintf (line, sizeof (line), "bad type %i", type);
			break;
	}

	return line;
}

/*
	PR_UglyValueString

	Returns a string describing *data in a type specific manner
	Easier to parse than PR_ValueString
*/
char       *
PR_UglyValueString (progs_t * pr, etype_t type, pr_type_t *val)
{
	static char line[256];
	ddef_t     *def;
	dfunction_t *f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
		case ev_string:
			snprintf (line, sizeof (line), "%s",
					  PR_GetString (pr, val->string_var));
			break;
		case ev_entity:
			snprintf (line, sizeof (line), "%i",
					  NUM_FOR_BAD_EDICT (pr, PROG_TO_EDICT (pr, val->entity_var)));
			break;
		case ev_func:
			f = pr->pr_functions + val->func_var;
			snprintf (line, sizeof (line), "%s", PR_GetString (pr, f->s_name));
			break;
		case ev_field:
			def = ED_FieldAtOfs (pr, val->integer_var);
			snprintf (line, sizeof (line), "%s",
					  PR_GetString (pr, def->s_name));
			break;
		case ev_void:
			strcpy (line, "void");
			break;
		case ev_float:
			snprintf (line, sizeof (line), "%f", val->float_var);
			break;
		case ev_vector:
			snprintf (line, sizeof (line), "%f %f %f", val->vector_var[0],
					  val->vector_var[1], val->vector_var[2]);
			break;
		default:
			snprintf (line, sizeof (line), "bad type %i", type);
			break;
	}

	return line;
}

/*
	PR_GlobalString

	Returns a string with a description and the contents of a global,
	padded to 20 field width
*/
char       *
PR_GlobalString (progs_t * pr, int ofs, etype_t type)
{
	char       *s;
	int         i;
	ddef_t     *def = 0;
	static char line[128];

	if (type == ev_short) {
		snprintf (line, sizeof (line), "%-20d", (short) ofs);
		return line;
	}
	if (pr_debug->int_val && pr->debug)
		def = PR_Get_Local_Def (pr, ofs);
	if (!def)
		def = ED_GlobalAtOfs (pr, ofs);
	if (!def && type == ev_void)
		snprintf (line, sizeof (line), "%i(?)", ofs);
	else {
		char *name = "?";
		char *oi = "";
		if (def) {
			if (type == ev_void)
				type = def->type;
			name = PR_GetString (pr, def->s_name);
			if (type != (def->type & ~DEF_SAVEGLOBAL))
				oi = "!";
		}
		if (ofs > pr->globals_size)
			s = "Out of bounds";
		else
			s = PR_ValueString (pr, type, &pr->pr_globals[ofs]);
		snprintf (line, sizeof (line), "%i(%s%s)%s", ofs, oi, name, s);
	}

	i = strlen (line);
	for (; i < 20; i++)
		strncat (line, " ", sizeof (line) - strlen (line));
	strncat (line, " ", sizeof (line) - strlen (line));

	return line;
}

char       *
PR_GlobalStringNoContents (progs_t * pr, int ofs, etype_t type)
{
	int         i;
	ddef_t     *def = 0;
	static char line[128];

	if (type == ev_short) {
		snprintf (line, sizeof (line), "%-20d", (short) ofs);
		return line;
	}
	if (pr_debug->int_val && pr->debug)
		def = PR_Get_Local_Def (pr, ofs);
	if (!def)
		def = ED_GlobalAtOfs (pr, ofs);
	if (!def)
		snprintf (line, sizeof (line), "%i(?)", ofs);
	else
		snprintf (line, sizeof (line), "%i(%s)", ofs,
				  PR_GetString (pr, def->s_name));

	i = strlen (line);
	for (; i < 20; i++)
		strncat (line, " ", sizeof (line) - strlen (line));
	strncat (line, " ", sizeof (line) - strlen (line));

	return line;
}


/*
	ED_Print

	For debugging
*/
void
ED_Print (progs_t * pr, edict_t *ed)
{
	int         l;
	ddef_t     *d;
	pr_type_t  *v;
	int         i;
	char       *name;
	int         type;

	if (ed->free) {
		Sys_Printf ("FREE\n");
		return;
	}

	Sys_Printf ("\nEDICT %i:\n", NUM_FOR_BAD_EDICT (pr, ed));
	for (i = 1; i < pr->progs->numfielddefs; i++) {
		d = &pr->pr_fielddefs[i];
		name = PR_GetString (pr, d->s_name);
		if (name[strlen (name) - 2] == '_')
			continue;					// skip _x, _y, _z vars

		v = ed->v + d->ofs;

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;

		switch (type) {
			case ev_entity:
			case ev_integer:
			case ev_func:
			case ev_field:
				if (!v->integer_var)
					continue;
				break;
			case ev_string:
				if (!PR_GetString (pr, v->string_var)[0])
					continue;
				break;
			case ev_float:
				if (!v->float_var)
					continue;
				break;
			case ev_vector:
				if (!v[0].float_var && !v[1].float_var && !v[2].float_var)
					continue;
				break;
			default:
				PR_Error (pr, "ED_Print: Unhandled type %d", type);
		}

		Sys_Printf ("%s", name);
		l = strlen (name);
		while (l++ < 15)
			Sys_Printf (" ");

		Sys_Printf ("%s\n", PR_ValueString (pr, d->type, v));
	}
}

/*
	ED_Write

	For savegames
*/
void
ED_Write (progs_t * pr, VFile *f, edict_t *ed)
{
	ddef_t     *d;
	pr_type_t  *v;
	int         i, j;
	char       *name;
	int         type;

	Qprintf (f, "{\n");

	if (ed->free) {
		Qprintf (f, "}\n");
		return;
	}

	for (i = 1; i < pr->progs->numfielddefs; i++) {
		d = &pr->pr_fielddefs[i];
		name = PR_GetString (pr, d->s_name);
		if (name[strlen (name) - 2] == '_')
			continue;					// skip _x, _y, _z vars

		v = &ed->v[d->ofs];

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j = 0; j < pr_type_size[type]; j++)
			if (v[j].integer_var)
				break;
		if (j == pr_type_size[type])
			continue;

		Qprintf (f, "\"%s\" ", name);
		Qprintf (f, "\"%s\"\n", PR_UglyValueString (pr, d->type, v));
	}

	Qprintf (f, "}\n");
}

void
ED_PrintNum (progs_t * pr, int ent)
{
	ED_Print (pr, EDICT_NUM (pr, ent));
}

/*
	ED_PrintEdicts

	For debugging, prints all the entities in the current server
*/
void
ED_PrintEdicts (progs_t *pr, const char *fieldval)
{
	int     i;
	int		count;
	ddef_t *def;

	def = ED_FindField(pr, "classname");

	if (fieldval && fieldval[0] && def) {
		count = 0;
		for (i = 0; i < *(pr)->num_edicts; i++)
			if (strequal(fieldval, E_STRING (pr, EDICT_NUM(pr, i), def->ofs))) {
				ED_PrintNum (pr, i);
				count++;
			}
		Sys_Printf ("%i entities\n", count);
	} else {
		for (i = 0; i < *(pr)->num_edicts; i++)
			ED_PrintNum (pr, i);
		Sys_Printf ("%i entities\n", *(pr)->num_edicts);
	}
}

/*
	ED_Count

	For debugging
*/
void
ED_Count (progs_t * pr)
{
	int         i;
	edict_t    *ent;
	int         active, models, solid, step, zombie;
	ddef_t     *solid_def;
	ddef_t     *model_def;

	solid_def = ED_FindField (pr, "solid");
	model_def = ED_FindField (pr, "model");
	active = models = solid = step = zombie = 0;
	for (i = 0; i < *(pr)->num_edicts; i++) {
		ent = EDICT_NUM (pr, i);
		if (ent->free) {
			if (*(pr)->time - ent->freetime <= 0.5)
				zombie++;
			continue;
		}
		active++;
		if (solid_def && ent->v[solid_def->ofs].float_var)
			solid++;
		if (model_def && ent->v[model_def->ofs].float_var)
			models++;
	}

	Sys_Printf ("num_edicts:%3i\n", *(pr)->num_edicts);
	Sys_Printf ("active    :%3i\n", active);
	Sys_Printf ("view      :%3i\n", models);
	Sys_Printf ("touch     :%3i\n", solid);
	Sys_Printf ("zombie    :%3i\n", zombie);
}

/*
	ARCHIVING GLOBALS

	FIXME: need to tag constants, doesn't really work
*/

/*
	ED_WriteGlobals
*/
void
ED_WriteGlobals (progs_t * pr, VFile *f)
{
	ddef_t     *def;
	int         i;
	char       *name;
	int         type;

	Qprintf (f, "{\n");
	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		def = &pr->pr_globaldefs[i];
		type = def->type;
		if (!(def->type & DEF_SAVEGLOBAL))
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = PR_GetString (pr, def->s_name);
		Qprintf (f, "\"%s\" ", name);
		Qprintf (f, "\"%s\"\n",
				 PR_UglyValueString (pr, type, &pr->pr_globals[def->ofs]));
	}
	Qprintf (f, "}\n");
}

/*
	ED_ParseGlobals
*/
void
ED_ParseGlobals (progs_t * pr, const char *data)
{
	char        keyname[64];
	ddef_t     *key;

	while (1) {
		// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		strcpy (keyname, com_token);

		// parse value  
		data = COM_Parse (data);
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			PR_Error (pr, "ED_ParseEntity: closing brace without data");

		key = PR_FindGlobal (pr, keyname);
		if (!key) {
			Sys_Printf ("'%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair (pr, pr->pr_globals, key, com_token))
			PR_Error (pr, "ED_ParseGlobals: parse error");
	}
}

//============================================================================


/*
	ED_NewString
*/
char       *
ED_NewString (progs_t * pr, const char *string)
{
	char       *new, *new_p;
	int         i, l;

	l = strlen (string) + 1;
	new = Hunk_Alloc (l);
	new_p = new;

	for (i = 0; i < l; i++) {
		if (string[i] == '\\' && i < l - 1) {
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		} else
			*new_p++ = string[i];
	}

	return new;
}


/*
	ED_ParseEval

	Can parse either fields or globals
	returns false if error
*/
qboolean
ED_ParseEpair (progs_t * pr, pr_type_t *base, ddef_t *key, const char *s)
{
	int         i;
	char        string[128];
	ddef_t     *def;
	char       *v, *w;
	pr_type_t  *d;
	dfunction_t *func;

	d = &base[key->ofs];

	switch (key->type & ~DEF_SAVEGLOBAL) {
		case ev_string:
			d->string_var = PR_SetString (pr, ED_NewString (pr, s));
			break;

		case ev_float:
			d->float_var = atof (s);
			break;

		case ev_vector:
			strcpy (string, s);
			v = string;
			w = string;
			for (i = 0; i < 3; i++) {
				while (*v && *v != ' ')
					v++;
				*v = 0;
				d->vector_var[i] = atof (w);
				w = v = v + 1;
			}
			break;

		case ev_entity:
			d->entity_var = EDICT_TO_PROG (pr, EDICT_NUM (pr, atoi (s)));
			break;

		case ev_field:
			def = ED_FindField (pr, s);
			if (!def) {
				Sys_Printf ("Can't find field %s\n", s);
				return false;
			}
			d->integer_var = G_INT (pr, def->ofs);
			break;

		case ev_func:
			func = ED_FindFunction (pr, s);
			if (!func) {
				Sys_Printf ("Can't find function %s\n", s);
				return false;
			}
			d->func_var = func - pr->pr_functions;
			break;

		default:
			break;
	}
	return true;
}

/*
	ED_ParseEdict

	Parses an edict out of the given string, returning the new position
	ed should be a properly initialized empty edict.
	Used for initial level load and for savegames.
*/
const char       *
ED_ParseEdict (progs_t * pr, const char *data, edict_t *ent)
{
	ddef_t     *key;
	qboolean    anglehack;
	qboolean    init;
	char        keyname[256];
	int         n;

	init = false;

// clear it
	if (ent != *(pr)->edicts)			// hack
		memset (&ent->v, 0, pr->progs->entityfields * 4);

// go through all the dictionary pairs
	while (1) {
		// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp (com_token, "angle")) {
			strcpy (com_token, "angles");
			anglehack = true;
		} else
			anglehack = false;

		// FIXME: change light to _light to get rid of this hack
		if (!strcmp (com_token, "light"))
			strcpy (com_token, "light_lev");	// hack for single light def

		strcpy (keyname, com_token);

		// another hack to fix heynames with trailing spaces
		n = strlen (keyname);
		while (n && keyname[n - 1] == ' ') {
			keyname[n - 1] = 0;
			n--;
		}

		// parse value  
		data = COM_Parse (data);
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			PR_Error (pr, "ED_ParseEntity: closing brace without data");

		init = true;

// keynames with a leading underscore are used for utility comments,
// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		key = ED_FindField (pr, keyname);
		if (!key) {
			if (!pr->parse_field || !pr->parse_field (pr, keyname, com_token)) {
				Sys_Printf ("'%s' is not a field\n", keyname);
				continue;
			}
		} else {
			int         ret;

			if (anglehack) {
				ret = ED_ParseEpair (pr, ent->v, key, va ("0 %s 0", com_token));
			} else {
				ret = ED_ParseEpair (pr, ent->v, key, com_token);
			}
			if (!ret)
				PR_Error (pr, "ED_ParseEdict: parse error");
		}
	}

	if (!init)
		ent->free = true;

	return data;
}


/*
	ED_LoadFromFile

	The entities are directly placed in the array, rather than allocated with
	ED_Alloc, because otherwise an error loading the map would have entity
	number references out of order.

	Creates a server's entity / program execution context by
	parsing textual entity definitions out of an ent file.

	Used for both fresh maps and savegame loads.  A fresh map would also need
	to call ED_CallSpawnFunctions () to let the objects initialize themselves.
*/
void
ED_LoadFromFile (progs_t * pr, const char *data)
{
	edict_t    *ent;
	int         inhibit;
	dfunction_t *func;
	pr_type_t  *classname;
	ddef_t     *def;

	ent = NULL;
	inhibit = 0;

	*pr->globals.time = *(pr)->time;

	// parse ents
	while (1) {
		// parse the opening brace  
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			PR_Error (pr, "ED_LoadFromFile: found %s when expecting {", com_token);

		if (!ent)
			ent = EDICT_NUM (pr, 0);
		else
			ent = ED_Alloc (pr);
		data = ED_ParseEdict (pr, data, ent);

		// remove things from different skill levels or deathmatch
		if (pr->prune_edict && pr->prune_edict (pr, ent)) {
			ED_Free (pr, ent);
			inhibit++;
			continue;
		}
		//
		// immediately call spawn function
		//
		def = ED_FindField (pr, "classname");
		if (!def) {
			Sys_Printf ("No classname for:\n");
			ED_Print (pr, ent);
			ED_Free (pr, ent);
			continue;
		}
		classname = &ent->v[def->ofs];
		// look for the spawn function
		func = ED_FindFunction (pr, PR_GetString (pr, classname->string_var));

		if (!func) {
			Sys_Printf ("No spawn function for:\n");
			ED_Print (pr, ent);
			ED_Free (pr, ent);
			continue;
		}

		*pr->globals.self = EDICT_TO_PROG (pr, ent);
		PR_ExecuteProgram (pr, func - pr->pr_functions);
		if (pr->flush)
			pr->flush ();
	}

	Sys_DPrintf ("%i entities inhibited\n", inhibit);
}

edict_t *
PR_InitEdicts (progs_t *pr, int num_edicts)
{   
	edict_t *edicts;
	edict_t *e;
	int     i, j;
	pr->pr_edictareasize = pr->pr_edict_size * num_edicts;
	edicts = Hunk_AllocName (pr->pr_edictareasize, "edicts");
	(*pr->edicts) = edicts;
	if (pr_deadbeef_ents->int_val) {
		memset (edicts, 0, *pr->reserved_edicts * pr->pr_edict_size);
		for (j =  *pr->reserved_edicts; j < num_edicts; j++) {
			e = EDICT_NUM (pr, j);
			for (i=0; i < pr->progs->entityfields; i++)
				e->v[i].integer_var = 0xdeadbeef;
		}
	} else {
		memset (edicts, 0, pr->pr_edictareasize);
	}
	return edicts;
}

edict_t    *
EDICT_NUM (progs_t * pr, int n)
{
	int offs = n * pr->pr_edict_size;
	if (offs < 0 || n >= pr->pr_edictareasize)
		PR_RunError (pr, "EDICT_NUM: bad number %i", n);
		
	return PROG_TO_EDICT (pr, offs);
}

int
NUM_FOR_BAD_EDICT (progs_t *pr, edict_t *e)
{
	int         b;

	b = (byte *) e - (byte *) * (pr)->edicts;
	b = b / pr->pr_edict_size;

	return b;
}

int
NUM_FOR_EDICT (progs_t *pr, edict_t *e)
{
	int b;

	b = NUM_FOR_BAD_EDICT (pr, e);

	if (b && (b < 0 || b >= *(pr)->num_edicts))
		PR_RunError (pr, "NUM_FOR_EDICT: bad pointer %d %p %p", b, e, * (pr)->edicts);

	return b;
}
