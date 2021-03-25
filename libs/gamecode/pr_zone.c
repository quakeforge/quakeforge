/*
	pr_zone.c

	(description)

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

#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"

static void
pr_zone_error (void *_pr, const char *msg)
{
	progs_t    *pr = (progs_t *) _pr;
	Z_Print (pr->zone);
	PR_RunError (pr, "%s", msg);
}

void
PR_Zone_Init (progs_t *pr)
{
	Z_ClearZone (pr->zone, pr->zone_size,
				 (pr_type_t *)pr->zone - pr->pr_globals, sizeof (pr_type_t));
	Z_SetError (pr->zone, pr_zone_error, pr);
}

VISIBLE void
PR_Zone_Free (progs_t *pr, void *ptr)
{
	Z_Free (pr->zone, ptr);
}

VISIBLE void *
PR_Zone_Malloc (progs_t *pr, pr_int_t size)
{
	if (size <= 0)
		PR_RunError (pr, "attempt to allocate less than 1 byte");
	return Z_Malloc (pr->zone, size);
}

VISIBLE void *
PR_Zone_TagMalloc (progs_t *pr, int size, int tag)
{
	if (size <= 0)
		PR_RunError (pr, "attempt to allocate less than 1 byte");
	return Z_TagMalloc (pr->zone, size, tag);
}

VISIBLE void *
PR_Zone_Realloc (progs_t *pr, void *ptr, pr_int_t size)
{
	if (ptr && !size) {
		Z_Free (pr->zone, ptr);
		return 0;
	}
	if (size <= 0)
		PR_RunError (pr, "attempt to allocate less than 1 byte");
	return Z_Realloc (pr->zone, ptr, size);
}
