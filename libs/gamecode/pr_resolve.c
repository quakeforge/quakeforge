/*
	pr_resolve.c

	symbol resolution

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


ddef_t *
PR_GlobalAtOfs (progs_t * pr, pr_int_t ofs)
{
	ddef_t     *def;
	pr_uint_t   i;

	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		def = &pr->pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

VISIBLE ddef_t *
PR_FieldAtOfs (progs_t * pr, pr_int_t ofs)
{
	ddef_t     *def;
	pr_uint_t   i;

	for (i = 0; i < pr->progs->numfielddefs; i++) {
		def = &pr->pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

VISIBLE ddef_t *
PR_FindField (progs_t * pr, const char *name)
{
	return  Hash_Find (pr->field_hash, name);
}

VISIBLE ddef_t *
PR_FindGlobal (progs_t * pr, const char *name)
{
	return  Hash_Find (pr->global_hash, name);
}

VISIBLE dfunction_t *
PR_FindFunction (progs_t * pr, const char *name)
{
	return  Hash_Find (pr->function_hash, name);
}

VISIBLE void
PR_Undefined (progs_t *pr, const char *type, const char *name)
{
	PR_Error (pr, "undefined %s %s", type, name);
}

VISIBLE int
PR_ResolveGlobals (progs_t *pr)
{
	const char *sym;
	ddef_t     *def;
	int         i;

	if (pr->progs->version == PROG_ID_VERSION) {
		pr->pr_return = &pr->pr_globals[OFS_RETURN];
		pr->pr_params[0] = &pr->pr_globals[OFS_PARM0];
		pr->pr_params[1] = &pr->pr_globals[OFS_PARM1];
		pr->pr_params[2] = &pr->pr_globals[OFS_PARM2];
		pr->pr_params[3] = &pr->pr_globals[OFS_PARM3];
		pr->pr_params[4] = &pr->pr_globals[OFS_PARM4];
		pr->pr_params[5] = &pr->pr_globals[OFS_PARM5];
		pr->pr_params[6] = &pr->pr_globals[OFS_PARM6];
		pr->pr_params[7] = &pr->pr_globals[OFS_PARM7];
		pr->pr_param_size = OFS_PARM1 - OFS_PARM0;
		pr->pr_param_alignment = 0;	// log2
	} else {
		if (!(def = PR_FindGlobal (pr, sym = ".return")))
			goto error;
		pr->pr_return = &pr->pr_globals[def->ofs];
		for (i = 0; i < MAX_PARMS; i++) {
			if (!(def = PR_FindGlobal (pr, sym = va(".param_%d", i))))
				goto error;
			pr->pr_params[i] = &pr->pr_globals[def->ofs];
		}
		if (!(def = PR_FindGlobal (pr, sym = ".param_size")))
			goto error;
		pr->pr_param_size = G_INT (pr, def->ofs);
		if (!(def = PR_FindGlobal (pr, sym = ".param_alignment")))
			goto error;
		pr->pr_param_alignment = G_INT (pr, def->ofs);
	}
	if (pr->pr_saved_params)
		free (pr->pr_saved_params);
	pr->pr_saved_params = calloc (pr->pr_param_size * MAX_PARMS,
								  sizeof (pr_type_t));
	memcpy (pr->pr_real_params, pr->pr_params, sizeof (pr->pr_params));
	if (!pr->globals.time) {
		if ((def = PR_FindGlobal (pr, "time")))
			pr->globals.time = &G_FLOAT (pr, def->ofs);
	}
	if (!pr->globals.self) {
		if ((def = PR_FindGlobal (pr, ".self"))
			|| (def = PR_FindGlobal (pr, "self")))
			pr->globals.self = &G_INT (pr, def->ofs);
	}
	if (!pr->globals.stack) {
		if ((def = PR_FindGlobal (pr, ".stack"))
			|| (def = PR_FindGlobal (pr, "stack"))) {
			pr->globals.stack = &G_POINTER (pr, def->ofs);
			// the stack is at the very end of the progs memory map
			*pr->globals.stack = pr->globals_size;
		}
	}
	if (pr->fields.nextthink == -1)
		if ((def = PR_FindField (pr, "nextthink")))
			pr->fields.nextthink = def->ofs;
	if (pr->fields.frame == -1)
		if ((def = PR_FindField (pr, "frame")))
			pr->fields.frame = def->ofs;
	if (pr->fields.think == -1)
		if ((def = PR_FindField (pr, "think")))
			pr->fields.think = def->ofs;
	return 1;
error:
	Sys_Printf ("%s: undefined symbol: %s\n", pr->progs_name, sym);
	return 0;
}

int
PR_AccessField (progs_t *pr, const char *name, etype_t type,
				const char *file, int line)
{
	ddef_t *def = PR_FindField (pr, name);

	if (!def)
		PR_Error (pr, "undefined field %s accessed at %s:%d", name, file, line);
	if (def->type != type)
		PR_Error (pr, "bad type access to %s as %s (should be %s) at %s:%d",
				  name, pr_type_name[type], pr_type_name[def->type],
				  file, line);
	return def->ofs;
}
