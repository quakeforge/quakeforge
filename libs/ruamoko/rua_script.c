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

typedef struct rua_script_s {
	struct rua_script_s *next;
	struct rua_script_s **prev;
	script_t    script;
	char       *text;	// only for Script_FromFile
	pr_string_t dstr;
	progs_t    *pr;
} rua_script_t;

typedef struct {
	PR_RESMAP(rua_script_t) script_map;
	rua_script_t *scripts;
} script_resources_t;

static rua_script_t *
script_new (script_resources_t *res)
{
	return PR_RESNEW (res->script_map);
}

static void
script_free (script_resources_t *res, rua_script_t *script)
{
	PR_RESFREE (res->script_map, script);
}

static void
script_reset (script_resources_t *res)
{
	PR_RESRESET (res->script_map);
}

static inline rua_script_t *
script_get (script_resources_t *res, int index)
{
	return PR_RESGET(res->script_map, index);
}

static inline int __attribute__((pure))
script_index (script_resources_t *res, rua_script_t *script)
{
	return PR_RESINDEX(res->script_map, script);
}

static void
bi_script_clear (progs_t *pr, void *_res)
{
	script_resources_t *res = (script_resources_t *) _res;
	for (rua_script_t *s = res->scripts; s; s = s->next) {
		free ((char *) s->script.single);
		free (s->text);
	}
	res->scripts = 0;
	script_reset (res);
}

static void
bi_script_destroy (progs_t *pr, void *_res)
{
	free (_res);
}

static void
bi_Script_New (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_new (res);

	if (!script)
		PR_RunError (pr, "out of memory");

	script->dstr = PR_NewMutableString (pr);
	script->script.token = PR_GetMutableString (pr, script->dstr);
	script->pr = pr;
	script->next = res->scripts;
	script->prev = &res->scripts;
	if (res->scripts) {
		res->scripts->prev = &script->next;
	}
	res->scripts = script;
	R_INT (pr) = script_index (res, script);
}

static void
bi_Script_Delete (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	PR_FreeString (pr, script->dstr);
	free ((char *) script->script.single);
	free (script->text);
	if (script->next) {
		script->next->prev = script->prev;
	}
	*script->prev = script->next;
	script_free (res, script);
}

static void
bi_Script_Start (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	Script_Start (&script->script, P_GSTRING (pr, 1), P_GSTRING (pr, 2));
	R_STRING (pr) = script->dstr;
}

static void
bi_Script_FromFile (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_get (res, P_INT (pr, 0));
	if (!script)
		PR_RunError (pr, "invalid script handle");
	QFile      *file = QFile_GetFile (pr, P_INT (pr, 2));
	long        offset;
	long        size;
	long		len;

	offset = Qtell (file);
	size = Qfilesize (file);
	len = size - offset;
	script->text = malloc (len + 1);
	Qread (file, script->text, len);
	script->text[len] = 0;

	Script_Start (&script->script, P_GSTRING (pr, 1), script->text);
	R_STRING (pr) = script->dstr;
}

static void
bi_Script_TokenAvailable (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	R_INT (pr) = Script_TokenAvailable (&script->script, P_INT (pr, 1));
}

static void
bi_Script_GetToken (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	R_INT (pr) = Script_GetToken (&script->script, P_INT (pr, 1));
}

static void
bi_Script_UngetToken (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	Script_UngetToken (&script->script);
}

static void
bi_Script_Error (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	RETURN_STRING (pr, script->script.error);
	script->script.error = 0;
}

static void
bi_Script_NoQuoteLines (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_get (res, P_INT (pr, 0));

	if (!script)
		PR_RunError (pr, "invalid script handle");
	R_INT (pr) = script->script.no_quote_lines;
	script->script.no_quote_lines = P_INT (pr, 1);
}

static void
bi_Script_SetSingle (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_get (res, P_INT (pr, 0));
	if (!script)
		PR_RunError (pr, "invalid script handle");
	free ((char *) script->script.single);
	script->script.single = 0;
	const char *single = P_GSTRING (pr, 1);
	if (single) {
		script->script.single = strdup (single);
	}
}

static void
bi_Script_GetLine (progs_t *pr, void *_res)
{
	script_resources_t *res = _res;
	rua_script_t *script = script_get (res, P_INT (pr, 0));
	if (!script)
		PR_RunError (pr, "invalid script handle");
	R_INT (pr) = script->script.line;
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(Script_New,            0),
	bi(Script_Delete,         1, p(ptr)),
	bi(Script_Start,          3, p(ptr), p(string), p(string)),
	bi(Script_FromFile,       3, p(ptr), p(string), p(ptr)),
	bi(Script_TokenAvailable, 2, p(ptr), p(int)),
	bi(Script_GetToken,       2, p(ptr), p(int)),
	bi(Script_UngetToken,     1, p(ptr)),
	bi(Script_Error,          1, p(ptr)),
	bi(Script_NoQuoteLines,   1, p(ptr)),
	bi(Script_SetSingle,      2, p(ptr), p(string)),
	bi(Script_GetLine,        1, p(ptr)),
	{0}
};

void
RUA_Script_Init (progs_t *pr, int secure)
{
	script_resources_t *res = calloc (1, sizeof (script_resources_t));

	PR_Resources_Register (pr, "Script", res, bi_script_clear,
						   bi_script_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
