/*
	gib_instructions.c

	#DESCRIPTION#

	Copyright (C) 2000       #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ctype.h>
#include <stdlib.h>

#include "QF/console.h"
#include "QF/cmd.h"
#include "QF/gib.h"

#include "gib_error.h"
#include "gib_instructions.h"
#include "gib_interpret.h"
#include "gib_parse.h"
#include "gib_vars.h"

static gib_inst_t *gibinstructions;

char       *gib_subret;


void
GIB_AddInstruction (const char *name, gib_func_t func)
{
	gib_inst_t *new;

	new = malloc (sizeof (gib_inst_t));
	new->name = malloc (strlen (name) + 1);
	new->func = func;
	strcpy (new->name, name);
	new->next = gibinstructions;
	gibinstructions = new;
}

gib_inst_t *
GIB_Find_Instruction (const char *name)
{
	gib_inst_t *inst;

	if (!(gibinstructions))
		return 0;
	for (inst = gibinstructions; strcmp (inst->name, name); inst = inst->next)
		if (!(inst->next))
			return 0;
	return inst;
}

void
GIB_Init_Instructions (void)
{
	GIB_AddInstruction ("echo", GIB_Echo_f);
	GIB_AddInstruction ("call", GIB_Call_f);
	GIB_AddInstruction ("return", GIB_Return_f);
	GIB_AddInstruction ("con", GIB_Con_f);
	GIB_AddInstruction ("listfetch", GIB_ListFetch_f);
}

int
GIB_Echo_f (void)
{
	Con_Printf (GIB_Argv (1));
	return 0;
}

int
GIB_Call_f (void)
{
	int				ret, i;
	gib_module_t   *mod;
	gib_sub_t	   *sub;

	mod = GIB_Get_ModSub_Mod (GIB_Argv (1));
	if (!mod)
		return GIB_E_NOSUB;
	sub = GIB_Get_ModSub_Sub (GIB_Argv (1));
	if (!sub)
		return GIB_E_NOSUB;
	gib_subargc = GIB_Argc () - 1;
	gib_subargv[0] = sub->name;
	for (i = 1; i <= gib_subargc; i++)
		gib_subargv[i] = GIB_Argv (i + 1);
	ret = GIB_Run_Sub (mod, sub);
	if (gib_subret) {
		GIB_Var_Set ("retval", gib_subret);
		free (gib_subret);
	}
	gib_subret = 0;
	return ret;
}

int
GIB_VarPrint_f (void)
{
	int			i;
	gib_var_t  *var;

	for (i = 1; i <= GIB_Argc (); i++) {
		var = GIB_Var_FindLocal (GIB_Argv (i));
		if (!var)
			return GIB_E_NOVAR;
		Con_Printf ("%s", var->value);
	}
	Con_Printf ("\n");
	return 0;
}

int
GIB_Return_f (void)
{
	if (GIB_Argc () != 1)
		return GIB_E_NUMARGS;
	gib_subret = malloc (strlen (GIB_Argv (1)) + 1);
	strcpy (gib_subret, GIB_Argv (1));
	return GIB_E_RETURN;				// Signal to block executor to return 
										// 
	// 
	// immediately
}

int
GIB_ListFetch_f (void)
{
	char	   *element;
	int			i, m, n;
	gib_var_t  *var;

	if (GIB_Argc () != 2)
		return GIB_E_NUMARGS;

	if (!(var = GIB_Var_FindLocal (GIB_Argv (1))))
		return GIB_E_NOVAR;
	for (i = 0; isspace (var->value[i]); i++);
	for (n = 1; n < atoi (GIB_Argv (2)); n++) {
		if ((m = GIB_Get_Arg (var->value + i)) < 1)
			return GIB_E_ULIMIT;
		i += m;
		for (; isspace (var->value[i]); i++);
	}
	if ((m = GIB_Get_Arg (var->value + i)) < 1)
		return GIB_E_ULIMIT;
	element = malloc (m + 1);
	strncpy (element, var->value + i, m);
	element[m] = 0;
	GIB_Var_Set ("retval", element);
	return 0;
}

int
GIB_Con_f (void)
{
	if (GIB_Argc () != 1)
		return GIB_E_NUMARGS;

	Cmd_ExecuteString (GIB_Argv (1), src_command);

	return 0;
}

int
GIB_ExpandVars (const char *source, char *buffer, int buffersize)
{
	char		varname[256];
	int			i, m, n;
	gib_var_t  *var;

	for (i = 0, n = 0; i <= strlen (source); i++) {
		if (source[i] == '$') {
			m = 0;
			while (isalnum (source[++i]))
				varname[m++] = source[i];

			varname[m++] = 0;
			if (!(var = GIB_Var_FindLocal (varname)))
				return GIB_E_NOVAR;
			if (n + strlen (var->value) >= buffersize)
				return GIB_E_BUFFER;
			memcpy (buffer + n, var->value, strlen (var->value));

			n += strlen (var->value);
			i--;
		} else {
			if (n >= buffersize + 1)
				return GIB_E_BUFFER;
			buffer[n++] = source[i];
		}
	}
	return 0;
}

int
GIB_ExpandBackticks (const char *source, char *buffer, int buffersize)
{
	char		tick[256];
	int			ret, i, m, n;
	gib_var_t  *var;

	for (i = 0, n = 0; i <= strlen (source); i++) {
		if (source[i] == '`') {
			m = 0;
			while (source[++i] != '`')
				tick[m++] = source[i];

			tick[m++] = 0;
			ret = GIB_Run_Inst (tick);
			if (ret) {
				return ret;
			}
			if (!(var = GIB_Var_FindLocal ("retval")))
				return GIB_E_NOVAR;
			if (n + strlen (var->value) >= buffersize)
				return GIB_E_BUFFER;
			memcpy (buffer + n, var->value, strlen (var->value));

			n += strlen (var->value);
		} else {
			if (n >= buffersize + 1)
				return GIB_E_BUFFER;
			buffer[n++] = source[i];
		}
	}
	return 0;
}
