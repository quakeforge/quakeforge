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
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/zone.h"
#include "QF/va.h"

#include "compat.h"

cvar_t		*pr_boundscheck;
cvar_t		*pr_deadbeef_ents;
cvar_t		*pr_deadbeef_locals;

int			pr_type_size[ev_type_count] = {
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

const char	*pr_type_name[ev_type_count] = {
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

/*
	ED_ClearEdict

	Sets everything to NULL
*/
void
ED_ClearEdict (progs_t *pr, edict_t *e, int val)
{
	unsigned int i;

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
edict_t *
ED_Alloc (progs_t *pr)
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
	ED_Free

	Marks the edict as free
	FIXME: walk all entities and NULL out references to this entity
*/
void
ED_Free (progs_t *pr, edict_t *ed)
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

	Returns a string describing *data in a type-specific manner
*/
static const char *
PR_ValueString (progs_t *pr, etype_t type, pr_type_t *val)
{
	static char	line[256];
	ddef_t		*def;
	int          ofs;
	dfunction_t	*f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
		case ev_string:
			if (PR_StringValid (pr, val->string_var))
				snprintf (line, sizeof (line), "%s",
						  PR_GetString (pr, val->string_var));
			else
				strcpy (line, "*** invalid ***");
			break;
		case ev_entity:
			snprintf (line, sizeof (line), "entity %i",
					  NUM_FOR_BAD_EDICT (pr, PROG_TO_EDICT (pr, val->entity_var)));
			break;
		case ev_func:
			if (val->func_var < 0 || val->func_var >= pr->progs->numfunctions)
				snprintf (line, sizeof (line), "INVALID:%d", val->func_var);
			else if (!val->func_var)
				return "NULL";
			else {
				f = pr->pr_functions + val->func_var;
				snprintf (line, sizeof (line), "%s()",
						  PR_GetString (pr, f->s_name));
			}
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
			snprintf (line, sizeof (line), "%g", val->float_var);
			break;
		case ev_vector:
			snprintf (line, sizeof (line), "'%g %g %g'",
					  val->vector_var[0], val->vector_var[1],
					  val->vector_var[2]);
			break;
		case ev_pointer:
			def = 0;
			ofs = val->integer_var;
			if (pr_debug->int_val && pr->debug)
				def = PR_Get_Local_Def (pr, ofs);
			if (!def)
				def = ED_GlobalAtOfs (pr, ofs);
			if (def)
				snprintf (line, sizeof (line), "&%s",
						  PR_GetString (pr, def->s_name));
			else
				snprintf (line, sizeof (line), "[$%x]", ofs);
			break;
		case ev_quaternion:
			snprintf (line, sizeof (line), "'%g %g %g %g'",
					  val->vector_var[0], val->vector_var[1],
					  val->vector_var[2], val->vector_var[3]);
			break;
		case ev_integer:
			snprintf (line, sizeof (line), "%d", val->integer_var);
			break;
		case ev_uinteger:
			snprintf (line, sizeof (line), "$%08x", val->uinteger_var);
			break;
		case ev_sel:
			snprintf (line, sizeof (line), "(SEL) %s",
					  PR_GetString (pr, val->string_var));
			break;
		default:
			snprintf (line, sizeof (line), "bad type %i", type);
			break;
	}

	return line;
}

/*
	PR_GlobalString

	Returns a string with a description and the contents of a global
*/
const char *
PR_GlobalString (progs_t *pr, int ofs, etype_t type)
{
	ddef_t				*def = NULL;
	static dstring_t	*line = NULL;
	const char			*s;

	if (!line)
		line = dstring_newstr();

	if (type == ev_short) {
		dsprintf (line, "%04x", (short) ofs);
		return line->str;
	}

	if (pr_debug->int_val && pr->debug)
		def = PR_Get_Local_Def (pr, ofs);
	if (!def)
		def = ED_GlobalAtOfs (pr, ofs);
	if (!def && type == ev_void)
		dsprintf (line, "[$%x]", ofs);
	else {
		const char *name = "?";
		const char *oi = "";
		if (def) {
			if (type == ev_void)
				type = def->type;
			name = PR_GetString (pr, def->s_name);
			if (type != (etype_t) (def->type & ~DEF_SAVEGLOBAL))
				oi = "?";
		}

		if (ofs > pr->globals_size)
			s = "Out of bounds";
		else
			s = PR_ValueString (pr, type, &pr->pr_globals[ofs]);

		if (strequal(name, "IMMEDIATE") || strequal(name, ".imm")) {
			if (type == ev_string)
				dsprintf (line, "\"%s\"", s);
			else
				dsprintf (line, "%s", s);
		} else if (strequal(name, "?"))
			dsprintf (line, "[$%x](%08x)%s", ofs,
					  pr->pr_globals[ofs].integer_var, s);
		else {
			if (type == ev_func)
				dsprintf (line, "%s%s", name, oi);
			else
				dsprintf (line, "%s%s(%s)", name, oi, s);
		}
	}
	return line->str;
}

const char *
PR_GlobalStringNoContents (progs_t *pr, int ofs, etype_t type)
{
	static dstring_t	*line = NULL;
	ddef_t				*def = NULL;

	if (!line)
		line = dstring_newstr();

	if (type == ev_short) {
		dsprintf (line, "%x", (short) ofs);
		return line->str;
	}

	if (pr_debug->int_val && pr->debug)
		def = PR_Get_Local_Def (pr, ofs);
	if (!def)
		def = ED_GlobalAtOfs (pr, ofs);
	if (!def)
		dsprintf (line, "[$%x]", ofs);
	else
		dsprintf (line, "%s", PR_GetString (pr, def->s_name));

	return line->str;
}


/*
	ED_Print

	For debugging
*/
void
ED_Print (progs_t *pr, edict_t *ed)
{
	int			l;
	unsigned int i;
	char		*name;
	int			type;
	ddef_t		*d;
	pr_type_t	*v;

	if (ed->free) {
		Sys_Printf ("FREE\n");
		return;
	}

	Sys_Printf ("\nEDICT %i:\n", NUM_FOR_BAD_EDICT (pr, ed));
	for (i = 0; i < pr->progs->numfielddefs; i++) {
		d = &pr->pr_fielddefs[i];
		if (!d->s_name)					// null field def (probably 1st)
			continue;
		name = PR_GetString (pr, d->s_name);
		if (name[strlen (name) - 2] == '_')
			continue;					// skip _x, _y, _z vars

		v = ed->v + d->ofs;

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;

		switch (type) {
			case ev_entity:
			case ev_integer:
			case ev_uinteger:
			case ev_pointer:
			case ev_func:
			case ev_field:
				if (!v->integer_var)
					continue;
				break;
			case ev_sel:
				if (!v[0].integer_var
					&& !PR_GetString (pr, v[1].string_var)[0])
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


void
ED_PrintNum (progs_t *pr, int ent)
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
	int		i;
	int		count;
	ddef_t	*def;

	def = ED_FindField(pr, "classname");

	if (fieldval && fieldval[0] && def) {
		count = 0;
		for (i = 0; i < *(pr)->num_edicts; i++)
			if (strequal(fieldval,
						 E_GSTRING (pr, EDICT_NUM(pr, i), def->ofs))) {
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
ED_Count (progs_t *pr)
{
	int			i;
	int			active, models, solid, step, zombie;
	ddef_t		*solid_def;
	ddef_t		*model_def;
	edict_t		*ent;

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

edict_t *
EDICT_NUM (progs_t *pr, int n)
{
	int offs = n * pr->pr_edict_size;
	if (offs < 0 || n >= pr->pr_edictareasize)
		PR_RunError (pr, "EDICT_NUM: bad number %i", n);
		
	return PROG_TO_EDICT (pr, offs);
}

int
NUM_FOR_BAD_EDICT (progs_t *pr, edict_t *e)
{
	int		b;

	b = (byte *) e - (byte *) * (pr)->edicts;
	b = b / pr->pr_edict_size;

	return b;
}

int
NUM_FOR_EDICT (progs_t *pr, edict_t *e)
{
	int		b;

	b = NUM_FOR_BAD_EDICT (pr, e);

	if (b && (b < 0 || b >= *(pr)->num_edicts))
		PR_RunError (pr, "NUM_FOR_EDICT: bad pointer %d %p %p", b, e, * (pr)->edicts);

	return b;
}

qboolean
PR_EdictValid (progs_t *pr, int e)
{
	if (e < 0 || e >= pr->pr_edictareasize)
		return false;
	if (e % pr->pr_edict_size)
		return false;
	return true;
}
