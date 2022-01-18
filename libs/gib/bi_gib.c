/*
	bi_gib.c

	GIB <-> Ruamoko interface

	Copyright (C) 2003 Brian Koropoff

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmd.h"
#include "QF/csqc.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/sys.h"
#include "QF/gib.h"
#include "gib_builtin.h"

typedef struct bi_gib_builtin_s {
	struct bi_gib_builtin_s *next;
	gib_builtin_t *builtin;
	progs_t    *pr;
	pr_func_t   func;
} bi_gib_builtin_t;

typedef struct bi_gib_resources_s {
	bi_gib_builtin_t *builtins;
} bi_gib_resources_t;

static hashtab_t *bi_gib_builtins;

static const char *
bi_gib_builtin_get_key (const void *c, void *unused)
{
	return ((bi_gib_builtin_t *)c)->builtin->name;
}

static void
bi_gib_builtin_free (void *_c, void *unused)
{
	bi_gib_builtin_t *c = (bi_gib_builtin_t *) _c;

	free (c);
}

static void
bi_gib_builtin_f (void)
{
	bi_gib_builtin_t *builtin = Hash_Find (bi_gib_builtins, GIB_Argv(0));
	pr_type_t *pr_list;
	int i;

	if (!builtin)
		Sys_Error ("bi_gib_builtin_f: unexpected call %s", GIB_Argv (0));

	PR_PushFrame (builtin->pr);
	pr_list = PR_Zone_Malloc (builtin->pr, GIB_Argc() * sizeof (pr_type_t));

	for (i = 0; i < GIB_Argc(); i++)
		pr_list[i].int_var = PR_SetTempString (builtin->pr, GIB_Argv(i));

	PR_RESET_PARAMS (builtin->pr);
	P_INT (builtin->pr, 0) = GIB_Argc();
	P_INT (builtin->pr, 1) = PR_SetPointer (builtin->pr, pr_list);
	builtin->pr->pr_argc = 2;
	PR_ExecuteProgram (builtin->pr, builtin->func);
	PR_PopFrame (builtin->pr);
	PR_Zone_Free (builtin->pr, pr_list);
}

static void
bi_gib_builtin_clear (progs_t *progs, void *data)
{
	bi_gib_resources_t *res = (bi_gib_resources_t *) data;
	bi_gib_builtin_t *cur;

	while ((cur = res->builtins)) {
		void *del = Hash_Del (bi_gib_builtins, cur->builtin->name);
		GIB_Builtin_Remove (cur->builtin->name);
		res->builtins = cur->next;
		Hash_Free (bi_gib_builtins, del);
	}
}

static void
bi_GIB_Builtin_Add (progs_t *pr)
{
	bi_gib_resources_t *res = PR_Resources_Find (pr, "GIB");
	bi_gib_builtin_t   *builtin;
	const char *name = P_GSTRING (pr, 0);
	pr_func_t   func = P_FUNCTION (pr, 1);

	if (GIB_Builtin_Exists (name)) {
		R_INT (pr) = 0;
		return;
	}

	builtin = malloc (sizeof (bi_gib_builtin_t));

	GIB_Builtin_Add (name, bi_gib_builtin_f);

	builtin->builtin = GIB_Builtin_Find (name);
	builtin->pr = pr;
	builtin->func = func;
	builtin->next = res->builtins;
	res->builtins = builtin;
	Hash_Add (bi_gib_builtins, builtin);
	R_INT (pr) = 1;
}

static void
bi_GIB_Return (progs_t *pr)
{
	const char *str = P_GSTRING(pr, 0);

	if (str)
		GIB_Return (str);
	R_INT (pr) = GIB_CanReturn () ? 1 : 0;
}

static void
bi_GIB_Handle_New (progs_t *pr)
{
	//long *qcptr = malloc (sizeof (long));
	//*qcptr = P_POINTER (pr, 0);
	//R_INT (pr) = GIB_Handle_New (qcptr);
}

static void
bi_GIB_Handle_Free (progs_t *pr)
{
	//unsigned long int hand = P_INT (pr, 0);
	//long *qcptr = GIB_Handle_Get (hand);

	//free (qcptr);
	//GIB_Handle_Free (hand);
}

static void
bi_GIB_Handle_Get (progs_t *pr)
{
	//long *hand = GIB_Handle_Get (P_INT (pr, 0));
	//if (hand)
	//	R_INT (pr) = *hand;
	//else
	//	R_INT (pr) = 0;
}

static builtin_t builtins[] = {
	{"GIB_Builtin_Add",	bi_GIB_Builtin_Add,	-1},
	{"GIB_Return",		bi_GIB_Return,		-1},
	{"GIB_Handle_New",	bi_GIB_Handle_New,	-1},
	{"GIB_Handle_Free",	bi_GIB_Handle_Free,	-1},
	{"GIB_Handle_Get",	bi_GIB_Handle_Get,	-1},
	{0}
};

VISIBLE void
GIB_Progs_Init (progs_t *pr)
{
	bi_gib_resources_t *res = malloc (sizeof (bi_gib_resources_t));
	res->builtins = 0;

	PR_Resources_Register (pr, "GIB", res, bi_gib_builtin_clear);

	bi_gib_builtins = Hash_NewTable (1021, bi_gib_builtin_get_key,
									 bi_gib_builtin_free, 0,
									 pr->hashlink_freelist);

	PR_RegisterBuiltins (pr, builtins);
}
