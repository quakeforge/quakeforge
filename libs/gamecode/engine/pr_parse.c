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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdarg.h>
#include <stdio.h>

#include "QF/cbuf.h"
#include "QF/crc.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/idparse.h"
#include "QF/progs.h"
#include "QF/qdefs.h"
#include "QF/qfplist.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/zone.h"
#include "QF/va.h"

#include "compat.h"

/*
	PR_UglyValueString

	Returns a string describing *data in a type specific manner
	Easier to parse than PR_ValueString
*/
static char *
PR_UglyValueString (progs_t *pr, etype_t type, pr_type_t *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;

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
			def = PR_FieldAtOfs (pr, val->integer_var);
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
	ED_Write

	For savegames
*/
void
ED_Write (progs_t *pr, QFile *f, edict_t *ed)
{
	unsigned int i;
	int         j;
	int         type;
	const char *name;
	ddef_t     *d;
	pr_type_t  *v;

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

/*
	ARCHIVING GLOBALS

	FIXME: need to tag constants, doesn't really work
*/

void
ED_WriteGlobals (progs_t *pr, QFile *f)
{
	ddef_t     *def;
	unsigned int i;
	const char *name;
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
static qboolean
ED_ParseEpair (progs_t *pr, pr_type_t *base, ddef_t *key, const char *s)
{
	int			i;
	char		*string;
	ddef_t		*def;
	char		*v, *w;
	pr_type_t	*d;
	dfunction_t	*func;

	d = &base[key->ofs];

	switch (key->type & ~DEF_SAVEGLOBAL) {
		case ev_string:
			d->string_var = ED_NewString (pr, s);
			break;

		case ev_float:
			d->float_var = atof (s);
			break;

		case ev_vector:
			string = strdup (s);
			v = string;
			w = string;
			for (i = 0; i < 3; i++) {
				while (*v && *v != ' ')
					v++;
				*v = 0;
				d->vector_var[i] = atof (w);
				w = v = v + 1;
			}
			free (string);
			break;

		case ev_entity:
			d->entity_var = EDICT_TO_PROG (pr, EDICT_NUM (pr, atoi (s)));
			break;

		case ev_field:
			def = PR_FindField (pr, s);
			if (!def) {
				Sys_Printf ("Can't find field %s\n", s);
				return false;
			}
			d->integer_var = G_INT (pr, def->ofs);
			break;

		case ev_func:
			func = PR_FindFunction (pr, s);
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
	ent should be a properly initialized empty edict.
	Used for initial level load and for savegames.
*/
const char *
ED_ParseEdict (progs_t *pr, const char *data, edict_t *ent)
{
	ddef_t		*key;
	qboolean	anglehack;
	qboolean	init = false;
	dstring_t  *keyname = dstring_new ();
	const char	*token;
	int			n;

	// clear it
	if (ent != *(pr)->edicts)			// hack
		memset (&ent->v, 0, pr->progs->entityfields * 4);

	while (1) {	// go through all the dictionary pairs
		// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		token = com_token;
		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp (token, "angle")) {
			token = "angles";
			anglehack = true;
		} else
			anglehack = false;

		if (!strcmp (token, "light"))
			token = "light_lev";	// hack for single light def

		dstring_copystr (keyname, token);

		// another hack to fix heynames with trailing spaces
		n = strlen (keyname->str);
		while (n && keyname->str[n - 1] == ' ') {
			keyname->str[n - 1] = 0;
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
		if (keyname->str[0] == '_')
			continue;

		key = PR_FindField (pr, keyname->str);
		if (!key) {
			if (!pr->parse_field
				|| !pr->parse_field (pr, keyname->str, com_token)) {
				Sys_Printf ("'%s' is not a field\n", keyname->str);
				continue;
			}
		} else {
			int         ret;

			if (anglehack) {
				ret = ED_ParseEpair (pr, ent->v, key, va ("0 %s 0", token));
			} else {
				ret = ED_ParseEpair (pr, ent->v, key, token);
			}
			if (!ret)
				PR_Error (pr, "ED_ParseEdict: parse error");
		}
	}

	if (!init)
		ent->free = true;

	dstring_delete (keyname);
	return data;
}

void
ED_ParseGlobals (progs_t *pr, script_t *script)
{
	dstring_t   *keyname = dstring_new ();
	ddef_t		*key;

	while (1) {
		// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		dstring_copystr (keyname, com_token);

		// parse value  
		data = COM_Parse (data);
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			PR_Error (pr, "ED_ParseEntity: closing brace without data");

		key = PR_FindGlobal (pr, keyname->str);
		if (!key) {
			Sys_Printf ("'%s' is not a global\n", keyname->str);
			continue;
		}

		if (!ED_ParseEpair (pr, pr->pr_globals, key, com_token))
			PR_Error (pr, "ED_ParseGlobals: parse error");
	}
	dstring_delete (keyname);
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
static void
ED_ParseOld (progs_t *pr, const char *data)
{
	edict_t		*ent = NULL;
	int			inhibit = 0;
	dfunction_t	*func;
	pr_type_t	*classname;
	ddef_t		*def;

	while (1) {	// parse ents
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

		// immediately call spawn function
		def = PR_FindField (pr, "classname");
		if (!def) {
			Sys_Printf ("No classname for:\n");
			ED_Print (pr, ent);
			ED_Free (pr, ent);
			continue;
		}
		classname = &ent->v[def->ofs];

		// look for the spawn function
		func = PR_FindFunction (pr, PR_GetString (pr, classname->string_var));
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

void
ED_LoadFromFile (progs_t *pr, const char *data)
{
	if (*data == '(') {
		// new style (plist) entity data
		plitem_t   *plist = PL_GetPropertyList (data);
		plist = plist;
	} else {
		// oldstyle entity data
		ED_ParseOld (pr, data);
	}
}
