/*
	rua_script.c

	Script api for ruamoko

	Copyright (C) 2004 Bill Currie

	Author: Bill Currie
	Date: 2002/11/11

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

#include "QF/progs.h"
#include "QF/script.h"

#include "rua_internal.h"

typedef struct {
	script_t    script;
	pr_string_t dstr;
	progs_t    *pr;
} rua_script_t;

typedef struct {
	PR_RESMAP(rua_script_t) scripts;
} script_resources_t;

static rua_script_t *
script_new (script_resources_t *res)
{
	return PR_RESNEW (res->scripts);
}

static void
script_free (script_resources_t *res, rua_script_t *script)
{
	PR_RESFREE (res->scripts, script);
}

static void
script_reset (script_resources_t *res)
{
	PR_RESRESET (res->scripts);
}

static inline rua_script_t *
script_get (script_resources_t *res, int index)
{
	return PR_RESGET(res->scripts, index);
}

static inline int __attribute__((pure))
script_index (script_resources_t *res, rua_script_t *script)
{
	return PR_RESINDEX(res->scripts, script);
}

static void
bi_script_clear (progs_t *pr, void *data)
{
	script_resources_t *res = (script_resources_t *) data;
	script_reset (res);
}

static void
bi_Script_New (progs_t *pr)
{
	script_resources_t *res = PR_Resources_Find (pr, "Script");
	rua_script_t *script = script_new (res);

	if (!script)
		PR_RunError (pr, "out of memory");

	script->dstr = PR_NewMutableString (pr);
	script->script.token = PR_GetMutableString (pr, script->dstr);
	script->pr = pr;
	R_INT (pr) = script_index (res, script);
}

static void
bi_Script_Delete (progs_t *pr)
{
	script_resources_t *res = PR_Resources_Find (pr, "Script");
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	PR_FreeString (pr, script->dstr);
	script_free (res, script);
}

static void
bi_Script_Start (progs_t *pr)
{
	script_resources_t *res = PR_Resources_Find (pr, "Script");
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	Script_Start (&script->script, P_GSTRING (pr, 1), P_GSTRING (pr, 2));
	R_STRING (pr) = script->dstr;
}

static void
bi_Script_TokenAvailable (progs_t *pr)
{
	script_resources_t *res = PR_Resources_Find (pr, "Script");
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	R_INT (pr) = Script_TokenAvailable (&script->script, P_INT (pr, 1));
}

static void
bi_Script_GetToken (progs_t *pr)
{
	script_resources_t *res = PR_Resources_Find (pr, "Script");
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	R_INT (pr) = Script_GetToken (&script->script, P_INT (pr, 1));
}

static void
bi_Script_UngetToken (progs_t *pr)
{
	script_resources_t *res = PR_Resources_Find (pr, "Script");
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	Script_UngetToken (&script->script);
}

static void
bi_Script_Error (progs_t *pr)
{
	script_resources_t *res = PR_Resources_Find (pr, "Script");
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	R_STRING (pr) = PR_SetString (pr, script->script.error);
	script->script.error = 0;
}

static void
bi_Script_NoQuoteLines (progs_t *pr)
{
	script_resources_t *res = PR_Resources_Find (pr, "Script");
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	R_INT (pr) = script->script.no_quote_lines;
	script->script.no_quote_lines = P_INT (pr, 1);
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(Script_New,            0),
	bi(Script_Delete,         1, p(ptr)),
	bi(Script_Start,          3, p(ptr), p(string), p(string)),
	bi(Script_TokenAvailable, 2, p(ptr), p(int)),
	bi(Script_GetToken,       2, p(ptr), p(int)),
	bi(Script_UngetToken,     1, p(ptr)),
	bi(Script_Error,          1, p(ptr)),
	bi(Script_NoQuoteLines,   1, p(ptr)),
	{0}
};

void
RUA_Script_Init (progs_t *pr, int secure)
{
	script_resources_t *res = calloc (1, sizeof (script_resources_t));

	PR_Resources_Register (pr, "Script", res, bi_script_clear);
	PR_RegisterBuiltins (pr, builtins, res);
}
