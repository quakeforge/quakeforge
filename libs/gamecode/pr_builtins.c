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
#include <stdarg.h>
#include <stdio.h>

#include "QF/cmd.h"
#include "QF/crc.h"
#include "QF/cvar.h"
#include "QF/darray.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/qdefs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"

typedef struct biblock_s DARRAY_TYPE (builtin_t *) biblock_t;

static const char *
builtin_get_key (const void *_bi, void *unused)
{
	builtin_t *bi = (builtin_t *)_bi;

	return bi->name;
}

static uintptr_t
builtin_get_hash (const void *_bi, void *unused)
{
	builtin_t *bi = (builtin_t *)_bi;

	return bi->binum;
}

static int
builtin_compare (const void *_bia, const void *_bib, void *unused)
{
	builtin_t *bia = (builtin_t *)_bia;
	builtin_t *bib = (builtin_t *)_bib;

	return bia->binum == bib->binum;
}

static int
builtin_next (progs_t *pr)
{
	if (!pr->bi_next)
		pr->bi_next = PR_AUTOBUILTIN;
	if (pr->bi_next == 0x80000000)
		PR_Error (pr, "too many auto-allocated builtins");
	return pr->bi_next++;
}

VISIBLE void
PR_RegisterBuiltins (progs_t *pr, builtin_t *builtins)
{
	builtin_t  *bi;
	int         count;

	if (!pr->builtin_hash) {
		pr->builtin_blocks = malloc (sizeof (biblock_t));
		DARRAY_INIT (pr->builtin_blocks, 16);
		pr->builtin_hash = Hash_NewTable (1021, builtin_get_key, 0, pr,
										  pr->hashlink_freelist);
		pr->builtin_num_hash = Hash_NewTable (1021, 0, 0, pr,
											  pr->hashlink_freelist);
		Hash_SetHashCompare (pr->builtin_num_hash, builtin_get_hash,
							 builtin_compare);
	}

	// count = 1 for terminator
	for (bi = builtins, count = 1; bi->name; bi++)
		count++;
	bi = malloc (count * sizeof (builtin_t));
	DARRAY_APPEND (pr->builtin_blocks, bi);
	memcpy (bi, builtins, count * sizeof (builtin_t));
	builtins = bi;

	while (builtins->name) {
		if (builtins->binum == 0 || builtins->binum >= PR_AUTOBUILTIN)
			PR_Error (pr, "bad builtin number: %s = #%d", builtins->name,
					  builtins->binum);

		if (builtins->binum < 0)
			builtins->binum = builtin_next (pr);

		if ((bi = Hash_Find (pr->builtin_hash, builtins->name))
			|| (bi = Hash_FindElement (pr->builtin_num_hash, builtins)))
			PR_Error (pr, "builtin %s = #%d already defined (%s = #%d)",
					  builtins->name, builtins->binum,
					  bi->name, bi->binum);

		Hash_Add (pr->builtin_hash, builtins);
		Hash_AddElement (pr->builtin_num_hash, builtins);

		builtins++;
	}
}

VISIBLE builtin_t *
PR_FindBuiltin (progs_t *pr, const char *name)
{
	return (builtin_t *) Hash_Find (pr->builtin_hash, name);
}

builtin_t *
PR_FindBuiltinNum (progs_t *pr, pr_int_t num)
{
	builtin_t   bi;

	bi.binum = num;
	return (builtin_t *) Hash_FindElement (pr->builtin_num_hash, &bi);
}

static void
bi_no_function (progs_t *pr)
{
	// no need for checking: the /only/ way to get here is via a function
	// descriptor with a bad builtin number
	dstatement_t *st = pr->pr_statements + pr->pr_xstatement;
	dfunction_t *desc = pr->pr_functions + G_FUNCTION (pr, st->a);
	const char *bi_name = PR_GetString (pr, desc->s_name);
	int         ind = -desc->first_statement;

	PR_RunError (pr, "Bad builtin called: %s = #%d", bi_name, ind);
}

VISIBLE int
PR_RelocateBuiltins (progs_t *pr)
{
	pr_uint_t   i;
	pr_int_t    ind;
	int         bad = 0;
	dfunction_t *desc;
	bfunction_t *func;
	builtin_t  *bi;
	builtin_proc proc;
	const char *bi_name;

	if (pr->function_table)
		free (pr->function_table);
	pr->function_table = calloc (pr->progs->numfunctions,
								 sizeof (bfunction_t));

	for (i = 1; i < pr->progs->numfunctions; i++) {
		desc = pr->pr_functions + i;
		func = pr->function_table + i;

		func->first_statement = desc->first_statement;
		func->parm_start = desc->parm_start;
		func->locals = desc->locals;
		func->numparms = desc->numparms;
		memcpy (func->parm_size, desc->parm_size, sizeof (func->parm_size));
		func->descriptor = desc;

		if (desc->first_statement > 0)
			continue;

		bi_name = PR_GetString (pr, desc->s_name);

		if (!desc->first_statement) {
			bi = PR_FindBuiltin (pr, bi_name);
			if (!bi) {
				Sys_Printf ("PR_RelocateBuiltins: %s: undefined builtin %s\n",
							pr->progs_name, bi_name);
				bad = 1;
				continue;
			}
			desc->first_statement = -bi->binum;
		}

		ind = -desc->first_statement;
		if (pr->bi_map)
			ind = pr->bi_map (pr, ind);
		bi = PR_FindBuiltinNum (pr, ind);
		if (!bi || !(proc = bi->proc)) {
			Sys_MaskPrintf (SYS_DEV,
							"WARNING: Bad builtin call number: %s = #%d\n",
							bi_name, -desc->first_statement);
			proc = bi_no_function;
		}
		if (!desc->s_name && bi) {
			desc->s_name = PR_SetString (pr, bi->name);
			Hash_Add (pr->function_hash, &pr->pr_functions[i]);
		}
		func->first_statement = desc->first_statement;
		func->func = proc;
	}
	return !bad;
}
