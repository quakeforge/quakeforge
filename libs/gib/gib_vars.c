/*
	gib_vars.c

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
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/gib.h"

#include "gib_stack.h"


gib_var_t  *
GIB_Var_FindLocal (char *key)
{
	gib_var_t  *var;

	if (!(GIB_LOCALS))
		return 0;
	for (var = GIB_LOCALS; strcmp (key, var->key); var = var->next)
		if (!(var->next))
			return 0;
	return var;
}

gib_var_t  *
GIB_Var_FindGlobal (char *key)
{
	gib_var_t  *var;

	if (!(GIB_CURRENTMOD->vars))
		return 0;
	for (var = GIB_CURRENTMOD->vars; strcmp (key, var->key); var = var->next)
		if (!(var->next))
			return 0;
	return var;
}

void
GIB_Var_Set (char *key, char *value)
{
	gib_var_t  *var;

	if ((var = GIB_Var_FindLocal (key))) {
		free (var->value);
	} else {
		var = malloc (sizeof (gib_var_t));
		var->key = malloc (strlen (key) + 1);
		strcpy (var->key, key);
		var->next = GIB_LOCALS;
		GIB_LOCALS = var;
	}
	var->value = malloc (strlen (value) + 1);
	strcpy (var->value, value);
}

void
GIB_Var_FreeAll (gib_var_t * var)
{
	gib_var_t  *temp;

	for (; var; var = temp) {
		temp = var->next;
		free (var->key);
		free (var->value);
		free (var);
	}
}
