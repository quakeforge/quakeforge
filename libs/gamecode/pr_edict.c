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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/progs.h"
#include "QF/sys.h"

#include "compat.h"

/*
	ED_ClearEdict

	Sets everything to NULL
*/
VISIBLE void
ED_ClearEdict (progs_t *pr, edict_t *e, int val)
{
	pr_uint_t   i;

	if (pr->reserved_edicts && NUM_FOR_EDICT (pr, e) < *pr->reserved_edicts)
		Sys_Printf ("clearing reserved edict %d\n", NUM_FOR_EDICT (pr, e));
	for (i=0; i < pr->progs->entityfields; i++)
		E_INT (e, i) = val;
	e->free = false;
}

/*
	Either finds a free edict, or allocates a new one.
	Try to avoid reusing an entity that was recently freed, because it
	can cause the client to think the entity morphed into something else
	instead of being removed and recreated, which can cause interpolated
	angles and bad trails.
*/
VISIBLE edict_t *
ED_Alloc (progs_t *pr)
{
	pr_uint_t   i;
	edict_t    *e;
	int         start = pr->reserved_edicts ? *pr->reserved_edicts : 0;

	if (!pr->num_edicts) {
		PR_RunError (pr, "Edicts not supported in this VM\n");
	}
	for (i = start + 1; i < *pr->num_edicts; i++) {
		e = EDICT_NUM (pr, i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && (!pr->globals.ftime//FIXME double time
						|| e->freetime < 2
						|| *pr->globals.ftime - e->freetime > 0.5)) {
			ED_ClearEdict (pr, e, 0);
			return e;
		}
	}

	if (i == pr->max_edicts) {
		Sys_Printf ("WARNING: ED_Alloc: no free edicts\n");
		PR_DumpState (pr);
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
VISIBLE void
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
	if (pr->globals.ftime)//FIXME double time
		ed->freetime = *pr->globals.ftime;
}

//===========================================================================



VISIBLE void
ED_PrintNum (progs_t *pr, pr_int_t ent, const char *fieldname)
{
	if (!pr->num_edicts) {
		Sys_Printf ("Edicts not supported in this VM\n");
		return;
	}
	ED_Print (pr, EDICT_NUM (pr, ent), fieldname);
}

/*
	ED_PrintEdicts

	For debugging, prints all the entities in the current server
*/
VISIBLE void
ED_PrintEdicts (progs_t *pr, const char *fieldval)
{
	pr_uint_t   i;
	int         count;
	pr_def_t   *def;

	def = PR_FindField(pr, "classname");

	if (!pr->num_edicts) {
		Sys_Printf ("Edicts not supported in this VM\n");
		return;
	}
	if (fieldval && fieldval[0] && def) {
		count = 0;
		for (i = 0; i < *pr->num_edicts; i++)
			if (strequal(fieldval,
						 E_GSTRING (pr, EDICT_NUM(pr, i), def->ofs))) {
				ED_PrintNum (pr, i, 0);
				count++;
			}
		Sys_Printf ("%i entities\n", count);
	} else {
		for (i = 0; i < *pr->num_edicts; i++)
			ED_PrintNum (pr, i, 0);
		Sys_Printf ("%i entities\n", *pr->num_edicts);
	}
}

/*
	ED_Count

	For debugging
*/
VISIBLE void
ED_Count (progs_t *pr)
{
	int         active, models, solid, step, zombie;
	pr_def_t   *solid_def;
	pr_def_t   *model_def;
	edict_t    *ent;

	if (!pr->num_edicts) {
		Sys_Printf ("Edicts not supported in this VM\n");
		return;
	}
	solid_def = PR_FindField (pr, "solid");
	model_def = PR_FindField (pr, "model");
	active = models = solid = step = zombie = 0;
	for (pr_uint_t i = 0; i < *pr->num_edicts; i++) {
		ent = EDICT_NUM (pr, i);
		if (ent->free) {
			if (pr->globals.ftime && *pr->globals.ftime - ent->freetime <= 0.5)//FIXME double time
				zombie++;
			continue;
		}
		active++;
		if (solid_def && E_FLOAT (ent, solid_def->ofs))
			solid++;
		if (model_def && E_FLOAT (ent, model_def->ofs))
			models++;
	}

	Sys_Printf ("num_edicts:%3i\n", *pr->num_edicts);
	Sys_Printf ("active    :%3i\n", active);
	Sys_Printf ("view      :%3i\n", models);
	Sys_Printf ("touch     :%3i\n", solid);
	Sys_Printf ("zombie    :%3i\n", zombie);
}

edict_t *
ED_EdictNum (progs_t *pr, pr_uint_t n)
{
	if (n >= *pr->num_edicts)
		PR_RunError (pr, "EDICT_NUM: bad number %d", n);

	return PR_edicts(pr) + n;
}

pr_uint_t
ED_NumForEdict (progs_t *pr, edict_t *e)
{
	pr_uint_t    b;

	b = NUM_FOR_BAD_EDICT (pr, e);

	if (b && b >= *pr->num_edicts)
		PR_RunError (pr, "NUM_FOR_EDICT: bad pointer %d %p %p", b, e,
					 pr->pr_edicts);

	return b;
}

qboolean
PR_EdictValid (progs_t *pr, pr_uint_t e)
{
	if (!pr->num_edicts) {
		return false;
	}
	if (e >= pr->pr_edict_area_size)
		return false;
	if (e % pr->pr_edict_size)
		return false;
	return true;
}
