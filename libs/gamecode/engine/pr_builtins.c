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

static const char *
builtin_get_key (void *_bi, void *unused)
{
	builtin_t *bi = (builtin_t *)_bi;
	return bi->name;
}

#define PR_AUTOBUILTIN 120
void
PR_AddBuiltin (progs_t *pr, const char *name, builtin_proc builtin, int num)
{
	int i;

	if (!pr->builtin_hash)
		pr->builtin_hash = Hash_NewTable (1021, builtin_get_key, 0, pr);

	if (pr->numbuiltins == 0) {
		pr->builtins = calloc (PR_AUTOBUILTIN, sizeof (builtin_t*));
		pr->numbuiltins = PR_AUTOBUILTIN;
		if (!pr->builtins)
			PR_Error (pr, "PR_AddBuiltin: memory allocation error!");
	}

	if (num < 0) {
		for (i = PR_AUTOBUILTIN;
			 i < pr->numbuiltins && pr->builtins[i]; i++)
			;
		if (i >= pr->numbuiltins) {
			pr->numbuiltins++;
			pr->builtins = realloc (pr->builtins,
									pr->numbuiltins * sizeof (builtin_t*));
			if (!pr->builtins)
				PR_Error (pr, "PR_AddBuiltin: memory allocation error!");
		}
	} else {
		if (num >= PR_AUTOBUILTIN || num == 0)
			PR_Error (pr, "PR_AddBuiltin: invalid builtin number.");
		if (pr->builtins[num])
			PR_Error (pr, "PR_AddBuiltin: builtin number already exists.");
		i = num;
	}
	pr->builtins[i] = malloc (sizeof (builtin_t));
	pr->builtins[i]->proc = builtin;
	pr->builtins[i]->name = name;
	pr->builtins[i]->binum = i;
	Hash_Add (pr->builtin_hash, pr->builtins[i]);
}

builtin_t *
PR_FindBuiltin (progs_t *pr, const char *name)
{
	return (builtin_t *) Hash_Find (pr->builtin_hash, name);
}

static void
bi_no_function (progs_t *pr)
{
	// no need for checking: the /only/ way to get here is via a function
	// descriptor with a bad builtin number
	dstatement_t *st = pr->pr_statements + pr->pr_xstatement;
	dfunction_t *func = pr->pr_functions + G_FUNCTION (pr, st->a);
	const char *bi_name = PR_GetString (pr, func->s_name);
	int         ind = -func->first_statement;

	PR_RunError (pr, "Bad builtin called: %s = #%d", bi_name, ind);
}

int
PR_RelocateBuiltins (progs_t *pr)
{
	int         i, ind;
	int         bad = 0;
	dfunction_t *func;
	builtin_t  *bi;
	builtin_proc proc;
	const char *bi_name;

	for (i = 1; i < pr->progs->numfunctions; i++) {
		func = pr->pr_functions + i;

		if (func->first_statement > 0)
			continue;

		bi_name = PR_GetString (pr, func->s_name);

		if (!func->first_statement) {
			bi = PR_FindBuiltin (pr, bi_name);
			if (!bi) {
				Sys_Printf ("PR_RelocateBuiltins: %s: undefined builtin %s\n",
							pr->progs_name, bi_name);
				bad = 1;
				continue;
			}
			func->first_statement = -bi->binum;
		}

		ind = -func->first_statement;
		if (ind >= pr->numbuiltins || !(bi = pr->builtins[ind])
			|| !(proc = bi->proc)) {
			Sys_DPrintf ("WARNING: Bad builtin call number: %s = #%d\n",
						 bi_name, ind);
			proc = bi_no_function;
		}
		((bfunction_t *) func)->func = proc;
	}
	return !bad;
}
