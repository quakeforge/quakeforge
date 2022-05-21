/*
	pr_parse.c

	map and savegame parsing

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

#include "qfalloca.h"

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/plist.h"
#include "QF/progs.h"
#include "QF/script.h"
#include "QF/sys.h"

#include "compat.h"

/*
	PR_UglyValueString

	Returns a string describing *data in a type specific manner
	Easier to parse than PR_ValueString
*/
static const char *
PR_UglyValueString (progs_t *pr, etype_t type, pr_type_t *val, dstring_t *line)
{
	pr_def_t    *def;
	dfunction_t	*f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
		case ev_string:
			dsprintf (line, "%s", PR_GetString (pr, PR_PTR (string, val)));
			break;
		case ev_entity:
			dsprintf (line, "%d",
					  NUM_FOR_BAD_EDICT (pr, PROG_TO_EDICT (pr, PR_PTR (entity, val))));
			break;
		case ev_func:
			f = pr->pr_functions + PR_PTR (func, val);
			dsprintf (line, "%s", PR_GetString (pr, f->name));
			break;
		case ev_field:
			def = PR_FieldAtOfs (pr, PR_PTR (int, val));
			dsprintf (line, "%s", PR_GetString (pr, def->name));
			break;
		case ev_void:
			dstring_copystr (line, "void");
			break;
		case ev_float:
			dsprintf (line, "%.9g", PR_PTR (float, val));
			break;
		case ev_int:
			dsprintf (line, "%d", PR_PTR (int, val));
			break;
		case ev_vector:
			dsprintf (line, "%.9g %.9g %.9g", VectorExpand (&PR_PTR (float, val)));
			break;
		case ev_quaternion:
			dsprintf (line, "%.9g %.9g %.9g %.9g", QuatExpand (&PR_PTR (float, val)));
			break;
		default:
			dsprintf (line, "bad type %i", type);
			break;
	}

	return line->str;
}

VISIBLE plitem_t *
ED_EntityDict (progs_t *pr, edict_t *ed)
{
	dstring_t  *dstr = dstring_newstr ();
	plitem_t   *entity = PL_NewDictionary (pr->hashctx);
	pr_uint_t   i;
	int         j;
	int         type;
	const char *name;
	const char *value;
	pr_type_t  *v;

	if (!ed->free) {
		for (i = 0; i < pr->progs->fielddefs.count; i++) {
			pr_def_t   *d = &pr->pr_fielddefs[i];

			name = PR_GetString (pr, d->name);
			if (!name[0])
				continue;					// skip unnamed fields
			if (name[strlen (name) - 2] == '_')
				continue;					// skip _x, _y, _z vars

			v = &E_fld (ed, d->ofs);

			// if the value is still all 0, skip the field
			type = d->type & ~DEF_SAVEGLOBAL;
			for (j = 0; j < pr_type_size[type]; j++)
				if (v[j].value)
					break;
			if (j == pr_type_size[type])
				continue;

			value = PR_UglyValueString (pr, type, v, dstr);
			PL_D_AddObject (entity, name, PL_NewString (value));
		}
	}
	dstring_delete (dstr);
	return entity;
}

/*
	ARCHIVING GLOBALS

	FIXME: need to tag constants, doesn't really work
*/

VISIBLE plitem_t *
ED_GlobalsDict (progs_t *pr)
{
	dstring_t  *dstr = dstring_newstr ();
	plitem_t   *globals = PL_NewDictionary (pr->hashctx);
	pr_uint_t   i;
	const char *name;
	const char *value;
	pr_def_t   *def;
	int         type;

	for (i = 0; i < pr->progs->globaldefs.count; i++) {
		def = &pr->pr_globaldefs[i];
		type = def->type;
		if (!(def->type & DEF_SAVEGLOBAL))
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = PR_GetString (pr, def->name);
		value = PR_UglyValueString (pr, type, &pr->pr_globals[def->ofs], dstr);
		PL_D_AddObject (globals, name, PL_NewString (value));
	}
	dstring_delete (dstr);
	return globals;
}


static int
ED_NewString (progs_t *pr, const char *string)
{
	char		*new, *new_p;
	int			i, l;

	l = strlen (string) + 1;
	new = alloca (l);
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

	return PR_SetString (pr, new);
}


/*
	ED_ParseEval

	Can parse either fields or globals
	returns false if error
*/
VISIBLE qboolean
ED_ParseEpair (progs_t *pr, pr_type_t *base, pr_def_t *key, const char *s)
{
	pr_def_t    *def;
	pr_type_t	*d;
	dfunction_t	*func;

	d = &base[key->ofs];

	switch (key->type & ~DEF_SAVEGLOBAL) {
		case ev_string:
			PR_PTR (string, d) = ED_NewString (pr, s);
			break;

		case ev_float:
			PR_PTR (float, d) = atof (s);
			break;

		case ev_vector:
			vec3_t      vec = {};
			char       *str = alloca (strlen (s) + 1);
			strcpy (str, s);
			for (char *v = str; *v; v++) {
				if (*v == ',') {
					*v = ' ';
				}
			}
			if (sscanf (s, "%f %f %f", VectorExpandAddr (vec)) != 3) {
				Sys_Printf ("Malformed vector %s\n", s);
			}
			VectorCopy (vec, PR_PTR (vector, d));
			break;

		case ev_entity:
			PR_PTR (entity, d) = EDICT_TO_PROG (pr, EDICT_NUM (pr, atoi (s)));
			break;

		case ev_field:
			def = PR_FindField (pr, s);
			if (!def) {
				Sys_Printf ("Can't find field %s\n", s);
				return false;
			}
			PR_PTR (int, d) = G_INT (pr, def->ofs);
			break;

		case ev_func:
			func = PR_FindFunction (pr, s);
			if (!func) {
				Sys_Printf ("Can't find function %s\n", s);
				return false;
			}
			PR_PTR (func, d) = func - pr->pr_functions;
			break;

		default:
			break;
	}
	return true;
}

/*
	ED_ParseOld

	The entities are directly placed in the array, rather than allocated with
	ED_Alloc, because otherwise an error loading the map would have entity
	number references out of order.

	Creates a server's entity / program execution context by
	parsing textual entity definitions out of an ent file.

	Used for both fresh maps and savegame loads.  A fresh map would also need
	to call ED_CallSpawnFunctions () to let the objects initialize themselves.
*/

VISIBLE plitem_t *
ED_ConvertToPlist (script_t *script, int nohack, struct hashctx_s **hashctx)
{
	dstring_t  *dstr = dstring_newstr ();
	plitem_t   *plist = PL_NewArray ();
	plitem_t   *ent;
	plitem_t   *key;
	plitem_t   *value;
	char       *token;
	int         anglehack;
	const char *msg = "";

	while (Script_GetToken (script, 1)) {
		token = script->token->str;
		if (!strequal (token, "{")) {
			msg = "EOF without closing brace";
			goto parse_error;
		}
		ent = PL_NewDictionary (hashctx);
		while (1) {
			int         n;

			if (!Script_GetToken (script, 1)) {
				msg = "EOF without closing brace";
				goto parse_error;
			}
			token = script->token->str;
			if (strequal (token, "}"))
				break;
			// hack to take care of trailing spaces in field names
			// (looking at you, Rogue)
			for (n = strlen (token); n && token[n - 1] == ' '; n--) {
				token[n - 1] = 0;
			}
			anglehack = 0;
			if (!nohack && strequal (token, "angle")) {
				key = PL_NewString ("angles");
				anglehack = 1;
			} else if (!nohack && strequal (token, "light")) {
				key = PL_NewString ("light_lev");
			} else {
				key = PL_NewString (token);
			}
			if (!Script_TokenAvailable (script, 0)) {
				msg = "EOL without value";
				goto parse_error;
			}
			Script_GetToken (script, 0);
			token = script->token->str;
			if (strequal (token, "}")) {
				msg = "closing brace without data";
				goto parse_error;
			}
			if (anglehack) {
				dsprintf (dstr, "0 %s 0", token);
				value = PL_NewString (dstr->str);
			} else {
				value = PL_NewString (token);
			}
			PL_D_AddObject (ent, PL_String (key), value);
			PL_Free (key);
		}
		PL_A_AddObject (plist, ent);
	}
	dstring_delete (dstr);
	return plist;
parse_error:
	Sys_Printf ("%s:%d: %s", script->file, script->line, msg);
	dstring_delete (dstr);
	PL_Free (plist);
	return 0;
}


VISIBLE void
ED_InitGlobals (progs_t *pr, plitem_t *globals)
{
	pr_def_t    vector_def;
	pr_def_t   *global;
	plitem_t   *keys;
	int         count;
	const char *global_name;
	const char *value;

	keys = PL_D_AllKeys (globals);
	count = PL_A_NumObjects (keys);
	while (count--) {
		global_name = PL_String (PL_ObjectAtIndex (keys, count));
		value = PL_String (PL_ObjectForKey (globals, global_name));
		global = PR_FindGlobal (pr, global_name);
		//FIXME should this be here?
		//This is a hardcoded fix for a design mistake in the original qcc
		//(saving global vector components rather than the whole vector).
		if (!global) {
			int         len = strlen (global_name);
			const char *tag = global_name + len - 2;
			if (len > 2 && tag[0] == '_' && strchr ("xyz", tag[1])) {
				char       *vector_name = strdup (global_name);
				vector_name[len - 2] = 0;
				global = PR_FindGlobal (pr, vector_name);
				if (global) {
					if ((global->type & ~DEF_SAVEGLOBAL) == ev_vector) {
						vector_def = *global;
						vector_def.ofs += tag[1] - 'x';
						vector_def.type = ev_float;
						global = &vector_def;
					} else {
						global = 0;
					}
				}
			}
		}
		if (!global) {
			Sys_Printf ("'%s' is not a global\n", global_name);
			continue;
		}
		if (!ED_ParseEpair (pr, pr->pr_globals, global, value))
			PR_Error (pr, "ED_InitGlobals: parse error");
	}
	PL_Free (keys);
}

VISIBLE void
ED_InitEntity (progs_t *pr, plitem_t *entity, edict_t *ent)
{
	pr_def_t   *field;
	plitem_t   *keys;
	const char *field_name;
	const char *value;
	int         count;
	int         init = 0;

	keys = PL_D_AllKeys (entity);
	count = PL_A_NumObjects (keys);
	while (count--) {
		field_name = PL_String (PL_ObjectAtIndex (keys, count));
		value = PL_String (PL_ObjectForKey (entity, field_name));
		field = PR_FindField (pr, field_name);
		if (!field) {
			if (!pr->parse_field
				|| !pr->parse_field (pr, field_name, value)) {
				Sys_Printf ("'%s' is not a field\n", field_name);
				continue;
			}
		} else {
			if (!ED_ParseEpair (pr, &E_fld (ent, 0), field, value))
				PR_Error (pr, "ED_InitEntity: parse error");
		}
		init = 1;
	}
	PL_Free (keys);
	if (!init)
		ent->free = 1;
}

static void
ED_SpawnEntities (progs_t *pr, plitem_t *entity_list)
{
	edict_t    *ent;
	int         inhibit = 0;
	plitem_t   *entity;
	plitem_t   *item;
	int         i;
	int         count;
	const char *classname;
	dfunction_t *func;
	pr_int_t    max_edicts = pr->pr_edict_area_size / pr->pr_edict_size;

	max_edicts -= *pr->num_edicts;
	count = PL_A_NumObjects (entity_list);
	for (i = 0; i < count; i++) {
		entity = PL_ObjectAtIndex (entity_list, i);

		item = PL_ObjectForKey (entity, "classname");
		if (!item)
			PR_Error (pr, "no classname for entity %d", i);
		classname = PL_String (item);
		if (strequal (classname, "worldspawn"))
			ent = EDICT_NUM (pr, 0);
		else
			ent = ED_Alloc (pr);

		// don't allow the last edict to be used, as otherwise we can't detect
		// too many edicts
		if (NUM_FOR_EDICT (pr, ent) >= pr->max_edicts - 1)
			PR_Error (pr, "too many entities: %d > %d", count, max_edicts);

		ED_InitEntity (pr, entity, ent);

		// remove things from different skill levels or deathmatch
		if (pr->prune_edict && pr->prune_edict (pr, ent)) {
			ED_Free (pr, ent);
			inhibit++;
			continue;
		}

		//XXX should the field be checked instead of going direct?
		func = PR_FindFunction (pr, classname);
		if (!func) {
			Sys_Printf ("No spawn function for :\n");
			ED_Print (pr, ent, 0);
			ED_Free (pr, ent);
			continue;
		}

		*pr->globals.self = EDICT_TO_PROG (pr, ent);
		PR_ExecuteProgram (pr, func - pr->pr_functions);
		if (pr->flush)
			pr->flush ();
	}
}

VISIBLE plitem_t *
ED_Parse (progs_t *pr, const char *data)
{
	script_t	*script;
	plitem_t    *entity_list = 0;

	script = Script_New ();
	Script_Start (script, "ent data", data);

	if (Script_GetToken (script, 1)) {
		if (strequal (script->token->str, "(")) {
			// new style (plist) entity data
			entity_list = PL_GetPropertyList (data, pr->hashctx);
		} else {
			// old style entity data
			Script_UngetToken (script);
			entity_list = ED_ConvertToPlist (script, 0, pr->hashctx);
		}
	}
	Script_Delete (script);
	return entity_list;
}

VISIBLE void
ED_LoadFromFile (progs_t *pr, const char *data)
{
	plitem_t    *entity_list;

	if (pr->edict_parse) {
		PR_PushFrame (pr);
		PR_RESET_PARAMS (pr);
		P_INT (pr, 0) = PR_SetTempString (pr, data);
		pr->pr_argc = 1;
		PR_ExecuteProgram (pr, pr->edict_parse);
		PR_PopFrame (pr);
		return;
	}
	entity_list = ED_Parse (pr, data);
	if (entity_list) {
		ED_SpawnEntities (pr, entity_list);
		PL_Free (entity_list);
	}
}

VISIBLE void
ED_EntityParseFunction (progs_t *pr, void *data)
{
	pr->edict_parse = P_FUNCTION (pr, 0);
}
