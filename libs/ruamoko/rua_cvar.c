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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/va.h"

#include "rua_internal.h"

static void
bi_Cvar_SetString (progs_t *pr)
{
	const char *varname = P_GSTRING (pr, 0);
	const char *val = P_GSTRING (pr, 1);
	cvar_t     *var = Cvar_FindVar (varname);

	if (var)
		Cvar_Set (var, val);
}

static void
bi_Cvar_SetInteger (progs_t *pr)
{
	const char *varname = P_GSTRING (pr, 0);
	int         val = P_INT (pr, 1);
	cvar_t     *var = Cvar_FindVar (varname);

	if (var)
		Cvar_Set (var, va ("%d", val));
}

static void
bi_Cvar_SetFloat (progs_t *pr)
{
	const char *varname = P_GSTRING (pr, 0);
	float       val = P_FLOAT (pr, 1);
	cvar_t     *var = Cvar_FindVar (varname);

	if (var)
		Cvar_Set (var, va ("%g", val));
}

static void
bi_Cvar_SetVector (progs_t *pr)
{
	const char *varname = P_GSTRING (pr, 0);
	float      *val = P_VECTOR (pr, 1);
	cvar_t     *var = Cvar_FindVar (varname);

	if (var)
		Cvar_Set (var, va ("%g %g %g", val[0], val[1], val[2]));
}

static void
bi_Cvar_GetString (progs_t *pr)
{
	const char *varname = P_GSTRING (pr, 0);
	cvar_t     *var = Cvar_FindVar (varname);

	if (var)
		RETURN_STRING (pr, var->string);
	else
		RETURN_STRING (pr, "");
}

static void
bi_Cvar_GetInteger (progs_t *pr)
{
	const char *varname = P_GSTRING (pr, 0);
	cvar_t     *var = Cvar_FindVar (varname);

	R_INT (pr) = var ? var->int_val : 0;
}

static void
bi_Cvar_GetFloat (progs_t *pr)
{
	const char *varname = P_GSTRING (pr, 0);
	cvar_t     *var = Cvar_FindVar (varname);

	R_FLOAT (pr) = var ? var->value : 0;
}

static void
bi_Cvar_GetVector (progs_t *pr)
{
	const char *varname = P_GSTRING (pr, 0);
	cvar_t     *var = Cvar_FindVar (varname);

	if (var)
		RETURN_VECTOR (pr, var->vec);
	else
		VectorZero (R_VECTOR (pr));
}

static void
bi_Cvar_Toggle (progs_t *pr)
{
	const char *varname = P_GSTRING (pr, 0);
	cvar_t     *var;

	var = Cvar_FindVar (varname);
	if (!var)
		var = Cvar_FindAlias (varname);
	if (var)
		Cvar_Set (var, var->int_val ? "0" : "1");
}

static builtin_t builtins[] = {
	{"Cvar_SetFloat",	bi_Cvar_SetFloat,	-1},
	{"Cvar_SetInteger",	bi_Cvar_SetInteger,	-1},
	{"Cvar_SetVector",	bi_Cvar_SetVector,	-1},
	{"Cvar_SetString",	bi_Cvar_SetString,	-1},
	{"Cvar_GetFloat",	bi_Cvar_GetFloat,	-1},
	{"Cvar_GetInteger",	bi_Cvar_GetInteger,	-1},
	{"Cvar_GetVector",	bi_Cvar_GetVector,	-1},
	{"Cvar_GetString",	bi_Cvar_GetString,	-1},
	{"Cvar_Toggle",		bi_Cvar_Toggle,		-1},
	{0}
};

void
RUA_Cvar_Init (progs_t *pr, int secure)
{
	PR_RegisterBuiltins (pr, builtins);
}
