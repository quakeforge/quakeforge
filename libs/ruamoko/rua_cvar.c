/*
	bi_cvar.c

	CSQC cvar builtins

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
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/va.h"

#include "QF/simd/types.h"

#include "rua_internal.h"

typedef struct bi_alias_s {
	struct bi_alias_s *next;
	char       *name;
} bi_alias_t;

typedef struct {
	bi_alias_t *aliases;
} cvar_resources_t;

static void
bi_alias_free (void *_a, void *unused)
{
	bi_alias_t *a = (bi_alias_t *) _a;

	free (a->name);
	free (a);
}

static void
bi_cvar_clear (progs_t *pr, void *_res)
{
	cvar_resources_t *res = (cvar_resources_t *) _res;
	bi_alias_t *alias;

	while ((alias = res->aliases)) {
		Cvar_RemoveAlias (alias->name);
		res->aliases = alias->next;
		bi_alias_free (alias, 0);
	}
}

static void
bi_cvar_destroy (progs_t *pr, void *_res)
{
}

static void
bi_Cvar_MakeAlias (progs_t *pr, void *_res)
{
	__auto_type res = (cvar_resources_t *) _res;
	const char *alias_name = P_GSTRING (pr, 0);
	const char *cvar_name = P_GSTRING (pr, 1);
	cvar_t     *cvar = Cvar_FindVar (cvar_name);
	bi_alias_t *alias;

	R_INT (pr) = 0;	// assume failure

	if (!cvar)		// allow aliasing of aliases
		cvar = Cvar_FindAlias (cvar_name);

	if (cvar && Cvar_MakeAlias (alias_name, cvar)) {
		alias = malloc (sizeof (bi_alias_t));
		alias->name = strdup (alias_name);
		alias->next = res->aliases;
		res->aliases = alias;

		R_INT (pr) = 1;
	}
}

static void
bi_Cvar_RemoveAlias (progs_t *pr, void *_res)
{
	__auto_type res = (cvar_resources_t *) _res;
	const char *alias_name = P_GSTRING (pr, 0);
	bi_alias_t **a;

	R_INT (pr) = 0;	// assume failure
	for (a = &res->aliases; *a; a = &(*a)->next) {
		bi_alias_t *alias = *a;
		if (!strcmp (alias_name, alias->name)) {
			*a = (*a)->next;
			if (Cvar_RemoveAlias (alias->name))
				R_INT (pr) = 1;
			bi_alias_free (alias, 0);
		}
	}
}

static void
bi_Cvar_SetString (progs_t *pr, void *_res)
{
	const char *varname = P_GSTRING (pr, 0);
	const char *val = P_GSTRING (pr, 1);
	cvar_t     *var = Cvar_FindVar (varname);

	if (!var)
		var = Cvar_FindAlias (varname);
	if (var)
		Cvar_SetVar (var, val);
}

static void
bi_Cvar_SetInteger (progs_t *pr, void *_res)
{
	const char *varname = P_GSTRING (pr, 0);
	int         val = P_INT (pr, 1);
	cvar_t     *var = Cvar_FindVar (varname);

	if (!var)
		var = Cvar_FindAlias (varname);
	if (var)
		Cvar_SetVar (var, va (0, "%d", val));
}

static void
bi_Cvar_SetFloat (progs_t *pr, void *_res)
{
	const char *varname = P_GSTRING (pr, 0);
	float       val = P_FLOAT (pr, 1);
	cvar_t     *var = Cvar_FindVar (varname);

	if (!var)
		var = Cvar_FindAlias (varname);
	if (var)
		Cvar_SetVar (var, va (0, "%g", val));
}

static void
bi_Cvar_SetVector (progs_t *pr, void *_res)
{
	const char *varname = P_GSTRING (pr, 0);
	float      *val = P_VECTOR (pr, 1);
	cvar_t     *var = Cvar_FindVar (varname);

	if (!var)
		var = Cvar_FindAlias (varname);
	if (var)
		Cvar_SetVar (var, va (0, "%g %g %g", val[0], val[1], val[2]));
}

static void
bi_Cvar_GetString (progs_t *pr, void *_res)
{
	const char *varname = P_GSTRING (pr, 0);
	cvar_t     *var = Cvar_FindVar (varname);

	if (!var)
		var = Cvar_FindAlias (varname);
	if (var)
		RETURN_STRING (pr, Cvar_VarString (var));
	else
		RETURN_STRING (pr, "");
}

static void
bi_Cvar_GetInteger (progs_t *pr, void *_res)
{
	const char *varname = P_GSTRING (pr, 0);
	cvar_t     *var = Cvar_FindVar (varname);

	R_INT (pr) = 0;
	if (!var)
		var = Cvar_FindAlias (varname);
	if (!var || var->value.type != &cexpr_int)
		return;

	R_INT (pr) = *(int *) var->value.value;
}

static void
bi_Cvar_GetFloat (progs_t *pr, void *_res)
{
	const char *varname = P_GSTRING (pr, 0);
	cvar_t     *var = Cvar_FindVar (varname);

	R_FLOAT (pr) = 0;
	if (!var)
		var = Cvar_FindAlias (varname);
	if (!var || var->value.type != &cexpr_float)
		return;
	R_INT (pr) = *(float *) var->value.value;
}

static void
bi_Cvar_GetVector (progs_t *pr, void *_res)
{
	const char *varname = P_GSTRING (pr, 0);
	cvar_t     *var = Cvar_FindVar (varname);

	if (!var)
		var = Cvar_FindAlias (varname);
	if (var && var->value.type == &cexpr_vector)
		RETURN_VECTOR (pr, *(vec4f_t *) var->value.value);
	else
		VectorZero (R_VECTOR (pr));
}

static void
bi_Cvar_Toggle (progs_t *pr, void *_res)
{
	const char *varname = P_GSTRING (pr, 0);
	cvar_t     *var;

	var = Cvar_FindVar (varname);
	if (!var)
		var = Cvar_FindAlias (varname);
	if (var && var->value.type == &cexpr_int) {
		*(int *) var->value.value = !*(int *) var->value.value;
	}
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Cvar_MakeAlias,   2, p(string), p(string)),
	bi(Cvar_RemoveAlias, 1, p(string)),
	bi(Cvar_SetFloat,    2, p(string), p(float)),
	bi(Cvar_SetInteger,  2, p(string), p(int)),
	bi(Cvar_SetVector,   2, p(string), p(vector)),
	bi(Cvar_SetString,   2, p(string), p(string)),
	bi(Cvar_GetFloat,    1, p(string)),
	bi(Cvar_GetInteger,  1, p(string)),
	bi(Cvar_GetVector,   1, p(string)),
	bi(Cvar_GetString,   1, p(string)),
	bi(Cvar_Toggle,      1, p(string)),
	{0}
};

void
RUA_Cvar_Init (progs_t *pr, int secure)
{
	cvar_resources_t *res = calloc (1, sizeof (cvar_resources_t));

	res->aliases = 0;
	PR_Resources_Register (pr, "Cvar", res, bi_cvar_clear, bi_cvar_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
