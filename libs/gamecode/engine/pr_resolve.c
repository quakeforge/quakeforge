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

#include "QF/cmd.h"
#include "QF/crc.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/qdefs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/zone.h"
#include "QF/va.h"

#include "compat.h"


/*
	ED_GlobalAtOfs
*/
ddef_t     *
ED_GlobalAtOfs (progs_t * pr, int ofs)
{
	ddef_t     *def;
	int         i;

	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		def = &pr->pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
	ED_FieldAtOfs
*/
ddef_t     *
ED_FieldAtOfs (progs_t * pr, int ofs)
{
	ddef_t     *def;
	int         i;

	for (i = 0; i < pr->progs->numfielddefs; i++) {
		def = &pr->pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
	ED_FindField
*/
ddef_t     *
ED_FindField (progs_t * pr, const char *name)
{
	return  Hash_Find (pr->field_hash, name);
}

int
ED_GetFieldIndex (progs_t *pr, const char *name)
{
	ddef_t     *def;

	def = ED_FindField (pr, name);
	if (def)
		return def->ofs;
	return -1;
}

/*
	PR_FindGlobal
*/
ddef_t     *
PR_FindGlobal (progs_t * pr, const char *name)
{
	return  Hash_Find (pr->global_hash, name);
}

pr_type_t *
PR_GetGlobalPointer (progs_t *pr, const char *name)
{
	ddef_t     *def;

	def = PR_FindGlobal (pr, name);
	if (def)
		return &pr->pr_globals[def->ofs];
	PR_Error (pr, "undefined global %s", name);
	return 0;
}

func_t
PR_GetFunctionIndex (progs_t *pr, const char *name)
{
	dfunction_t *func = ED_FindFunction (pr, name);
	if (func)
		return func - pr->pr_functions;
	PR_Error (pr, "undefined function %s", name);
	return -1;
}

int
PR_GetFieldOffset (progs_t *pr, const char *name)
{
	ddef_t *def = ED_FindField (pr, name);
	if (def)
		return def->ofs;
	PR_Error (pr, "undefined field %s", name);
	return -1;
}

/*
	ED_FindFunction
*/
dfunction_t *
ED_FindFunction (progs_t * pr, const char *name)
{
	return  Hash_Find (pr->function_hash, name);
}

int
PR_ResolveGlobals (progs_t *pr)
{
	const char *sym;
	ddef_t     *def;

	if (!(def = PR_FindGlobal (pr, sym = "time")))
		goto error;
	pr->globals.time = &G_FLOAT (pr, def->ofs);
	if (!(def = PR_FindGlobal (pr, ".self")))
		if (!(def = PR_FindGlobal (pr, "self"))) {
		    	sym = "self";
			goto error;
		}
	pr->globals.self = &G_INT (pr, def->ofs);
	if ((pr->fields.nextthink = ED_GetFieldIndex (pr, sym = "nextthink")) == -1)
		goto error;
	if ((pr->fields.frame = ED_GetFieldIndex (pr, sym = "frame")) == -1)
		goto error;
	if ((pr->fields.think = ED_GetFieldIndex (pr, sym = "think")) == -1)
		goto error;
	return 1;
error:
	Sys_Printf ("%s: undefined symbol: %s\n", pr->progs_name, sym);
	return 0;
}

int
PR_AccessField (progs_t *pr, const char *name, etype_t type,
				const char *file, int line)
{
	ddef_t *def = ED_FindField (pr, name);

	if (!def)
		PR_Error (pr, "undefined field %s accessed at %s:%d", name, file, line);
	if (def->type != type)
		PR_Error (pr, "bad type access to %s as %s (should be %s) at %s:%d",
				  name, pr_type_name[type], pr_type_name[def->type],
				  file, line);
	return def->ofs;
}
