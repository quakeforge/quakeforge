/*
	bi_cmd.c

	Command api for ruamoko

	Copyright (C) 2002 Bill Currie

	Author: Bill Currie
	Date: 2002/4/12

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
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/sys.h"

#include "rua_internal.h"

typedef struct bi_cmd_s {
	struct bi_cmd_s *next;
	char       *name;
	progs_t    *pr;
	pr_func_t   func;
} bi_cmd_t;

typedef struct {
	bi_cmd_t  *cmds;
} cmd_resources_t;

static hashtab_t *bi_cmds;
static int bi_cmds_refs;

static const char *
bi_cmd_get_key (const void *c, void *unused)
{
	return ((bi_cmd_t *)c)->name;
}

static void
bi_cmd_free (void *_c, void *unused)
{
	bi_cmd_t   *c = (bi_cmd_t *) _c;

	free (c->name);
	free (c);
}

static void
bi_cmd_f (void)
{
	bi_cmd_t   *cmd = Hash_Find (bi_cmds, Cmd_Argv (0));

	if (!cmd)
		Sys_Error ("bi_cmd_f: unexpected call %s", Cmd_Argv (0));
	PR_ExecuteProgram (cmd->pr, cmd->func);
}

static void
bi_Cmd_AddCommand (progs_t *pr, void *_res)
{
	__auto_type res = (cmd_resources_t *) _res;
	bi_cmd_t   *cmd = malloc (sizeof (bi_cmd_t));
	char       *name = strdup (P_GSTRING (pr, 0));
	pr_func_t   func = P_FUNCTION (pr, 1);

	if (!cmd || !name || !Cmd_AddCommand (name, bi_cmd_f, "CSQC command")) {
		if (name)
			free (name);
		if (cmd)
			free (cmd);
		R_INT (pr) = 0;
		return;
	}
	cmd->name = name;
	cmd->func = func;
	cmd->pr = pr;
	Hash_Add (bi_cmds, cmd);
	cmd->next = res->cmds;
	res->cmds = cmd;
	R_INT (pr) = 1;
}

static void
bi_cmd_clear (progs_t *pr, void *data)
{
	cmd_resources_t *res = (cmd_resources_t *)data;
	bi_cmd_t   *cmd;

	while ((cmd = res->cmds)) {
		Cmd_RemoveCommand (cmd->name);
		Hash_Del (bi_cmds, cmd->name);
		res->cmds = cmd->next;
		bi_cmd_free (cmd, 0);
	}
}

static void
bi_cmd_destroy (progs_t *pr, void *data)
{
	if (!--bi_cmds_refs) {
		Hash_DelTable (bi_cmds);
	}
}

static void
bi_Cmd_Argc (progs_t *pr, void *data)
{
	R_INT (pr) = Cmd_Argc ();
}

static void
bi_Cmd_Argv (progs_t *pr, void *data)
{
	RETURN_STRING (pr, Cmd_Argv (P_INT (pr, 0)));
}

static void
bi_Cmd_Args (progs_t *pr, void *data)
{
	RETURN_STRING (pr, Cmd_Args (P_INT (pr, 0)));
}

//Cmd_TokenizeString
//Cmd_ExecuteString
//Cmd_ForwardToServer

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(Cmd_AddCommand, 2, p(string), p(func)),
	bi(Cmd_Argc,       0),
	bi(Cmd_Argv,       1, p(int)),
	bi(Cmd_Args,       1, p(int)),
	{0}
};

void
RUA_Cmd_Init (progs_t *pr, int secure)
{
	cmd_resources_t *res = calloc (1, sizeof (cmd_resources_t));

	res->cmds = 0;

	if (!bi_cmds) {
		bi_cmds = Hash_NewTable (1021, bi_cmd_get_key, bi_cmd_free, 0,
								 pr->hashctx);
	}
	bi_cmds_refs++;

	PR_Resources_Register (pr, "Cmd", res, bi_cmd_clear, bi_cmd_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
