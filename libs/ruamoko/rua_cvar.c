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
#include "QF/progs.h"
#include "QF/zone.h"

#include "rua_internal.h"

/*
    bi_Cvar_GetCvarString
    
    QC-Function for get a string from a cvar
*/      
static void
bi_Cvar_GetCvarString (progs_t *pr)
{
	const char *varname = P_GSTRING (pr, 0);

	RETURN_STRING (pr, Cvar_VariableString (varname));
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
	{"Cvar_GetCvarString",	bi_Cvar_GetCvarString,	-1},
	{"Cvar_Toggle",			bi_Cvar_Toggle,			-1},
	{0}
};

void
RUA_Cvar_Init (progs_t *pr, int secure)
{
	PR_RegisterBuiltins (pr, builtins);
}
