/*
	cvar.c

	dynamic variable tracking

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "QF/compat.h"
#include "QF/console.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/qargs.h"

cvar_t          *developer;
cvar_t			*cvar_vars;
char			*cvar_null_string = "";
extern cvar_t	*developer;
cvar_alias_t	*calias_vars;
hashtab_t		*cvar_hash;
hashtab_t		*calias_hash;

/*
	Cvar_FindVar
*/
cvar_t *
Cvar_FindVar (char *var_name)
{
	return (cvar_t*) Hash_Find (cvar_hash, var_name);
}

cvar_t *
Cvar_FindAlias (char *alias_name)
{
	cvar_alias_t *alias;

	alias = (cvar_alias_t*) Hash_Find (calias_hash, alias_name);
	if (alias)
		return alias->cvar;
	return 0;
}

void
Cvar_Alias_Get (char *name, cvar_t *cvar)
{
	cvar_alias_t	*alias;
	cvar_t			*var;

	if (Cmd_Exists (name)) {
		Con_Printf ("CAlias_Get: %s is a command\n", name);
		return;
	}
	if (Cvar_FindVar (name)) {
		Con_Printf ("CAlias_Get: tried to alias used cvar name %s\n", name);
		return;
	}
	var = Cvar_FindAlias (name);
	if (!var) {
		alias = (cvar_alias_t *) calloc (1, sizeof (cvar_alias_t));

		alias->next = calias_vars;
		calias_vars = alias;
		alias->name = strdup (name);
		alias->cvar = cvar;
		Hash_Add (calias_hash, alias);
	}
}

/*
	Cvar_VariableValue
*/
float
Cvar_VariableValue (char *var_name)
{
	cvar_t     *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		var = Cvar_FindAlias (var_name);
	if (!var)
		return 0;
	return atof (var->string);
}


/*
	Cvar_VariableString
*/
char *
Cvar_VariableString (char *var_name)
{
	cvar_t     *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		var = Cvar_FindAlias (var_name);
	if (!var)
		return cvar_null_string;
	return var->string;
}


/*
	Cvar_CompleteVariable
*/
char *
Cvar_CompleteVariable (char *partial)
{
	cvar_t     *cvar;
	cvar_alias_t *alias;
	int         len;

	len = strlen (partial);

	if (!len)
		return NULL;

	// check exact match
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strcasecmp (partial, cvar->name))
			return cvar->name;

	// check aliases too :)
	for (alias = calias_vars; alias; alias = alias->next)
		if (!strcasecmp (partial, alias->name))
			return alias->name;

	// check partial match
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncasecmp (partial, cvar->name, len))
			return cvar->name;

	// check aliases too :)
	for (alias = calias_vars; alias; alias = alias->next)
		if (!strncasecmp (partial, alias->name, len))
			return alias->name;

	return NULL;
}


/*
	Cvar_Set
*/
void
Cvar_Set (cvar_t *var, char *value)
{
	int     changed;

	if (!var)
		return;

	if (var->flags & CVAR_ROM) {
		Con_DPrintf ("Cvar \"%s\" is read-only, cannot modify\n", var->name);
		return;
	}

	changed = !strequal (var->string, value);
	free (var->string);					// free the old value string

	var->string = strdup (value);
	var->value = atof (var->string);
	var->int_val = atoi (var->string);
	sscanf (var->string, "%f %f %f", &var->vec[0], &var->vec[1], &var->vec[2]);

	if (changed && var->callback)
		var->callback (var);
}


/*
	Cvar_SetROM

	doesn't check for CVAR_ROM flag
*/
void
Cvar_SetROM (cvar_t *var, char *value)
{
	int     changed;

	if (!var)
		return;

	changed = !strequal (var->string, value);
	free (var->string);					// free the old value string

	var->string = strdup (value);
	var->value = atof (var->string);
	var->int_val = atoi (var->string);
	sscanf (var->string, "%f %f %f", &var->vec[0], &var->vec[1], &var->vec[2]);

	if (changed && var->callback)
		var->callback (var);
}

/*
	Cvar_SetValue
*/
// 1999-09-07 weird cvar zeros fix by Maddes
void
Cvar_SetValue (cvar_t *var, float value)
{
	char        val[32];
	int         i;

	snprintf (val, sizeof (val), "%f", value);
	for (i = strlen (val) - 1; i > 0 && val[i] == '0' && val[i - 1] != '.'; i--) {
		val[i] = 0;
	}
	Cvar_Set (var, val);
}

/*
	Cvar_Command

	Handles variable inspection and changing from the console
*/
qboolean
Cvar_Command (void)
{
	cvar_t     *v;

	// check variables
	v = Cvar_FindVar (Cmd_Argv (0));
	if (!v)
		v = Cvar_FindAlias (Cmd_Argv (0));
	if (!v)
		return false;

// perform a variable print or set
	if (Cmd_Argc () == 1) {
		Con_Printf ("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	Cvar_Set (v, Cmd_Argv (1));
	return true;
}


/*
	Cvar_WriteVariables

	Writes lines containing "set variable value" for all variables
	with the archive flag set to true.
*/
void
Cvar_WriteVariables (QFile *f)
{
	cvar_t     *var;

	for (var = cvar_vars; var; var = var->next)
		if (var->flags & CVAR_ARCHIVE)
			Qprintf (f, "%s \"%s\"\n", var->name, var->string);
}

void
Cvar_Set_f (void)
{
	cvar_t     *var;
	char       *value;
	char       *var_name;

	if (Cmd_Argc () != 3) {
		Con_Printf ("usage: set <cvar> <value>\n");
		return;
	}
	var_name = Cmd_Argv (1);
	value = Cmd_Argv (2);
	var = Cvar_FindVar (var_name);

	if (!var)
		var = Cvar_FindAlias (var_name);

	if (var) {
		if (var->flags & CVAR_ROM) {
			Con_DPrintf ("Cvar \"%s\" is read-only, cannot modify\n", var_name);
		} else {
			Cvar_Set (var, value);
		}
	} else {
		var = Cvar_Get (var_name, value, CVAR_USER_CREATED, NULL,
						"User-created cvar");
	}
}

void
Cvar_Setrom_f (void)
{
	cvar_t     *var;
	char       *value;
	char       *var_name;

	if (Cmd_Argc () != 3) {
		Con_Printf ("usage: setrom <cvar> <value>\n");
		return;
	}
	var_name = Cmd_Argv (1);
	value = Cmd_Argv (2);
	var = Cvar_FindVar (var_name);

	if (!var)
		var = Cvar_FindAlias (var_name);

	if (var) {
		if (var->flags & CVAR_ROM) {
			Con_DPrintf ("Cvar \"%s\" is read-only, cannot modify\n", var_name);
		} else {
			Cvar_Set (var, value);
			Cvar_SetFlags (var, var->flags | CVAR_ROM);
		}
	} else {
		var = Cvar_Get (var_name, value, CVAR_USER_CREATED | CVAR_ROM, NULL,
						"User-created READ-ONLY Cvar");
	}
}

void
Cvar_Toggle_f (void)
{
	cvar_t     *var;

	if (Cmd_Argc () != 2) {
		Con_Printf ("toggle <cvar> : toggle a cvar on/off\n");
		return;
	}

	var = Cvar_FindVar (Cmd_Argv (1));
	if (!var)
		var = Cvar_FindAlias (Cmd_Argv (1));
	if (!var) {
		Con_Printf ("Unknown variable \"%s\"\n", Cmd_Argv (1));
		return;
	}

	Cvar_Set (var, var->int_val ? "0" : "1");
}

void
Cvar_Help_f (void)
{
	char       *var_name;
	cvar_t     *var;

	if (Cmd_Argc () != 2) {
		Con_Printf ("usage: help <cvar>\n");
		return;
	}

	var_name = Cmd_Argv (1);
	var = Cvar_FindVar (var_name);
	if (!var)
		var = Cvar_FindAlias (var_name);
	if (var) {
		Con_Printf ("%s\n", var->description);
		return;
	}
	Con_Printf ("variable not found\n");
}

void
Cvar_CvarList_f (void)
{
	cvar_t     *var;
	int         i;
	int         showhelp = 0;

	if (Cmd_Argc () > 1)
		showhelp = 1;
	for (var = cvar_vars, i = 0; var; var = var->next, i++) {
		Con_Printf ("%c%c%c%c ",
					var->flags & CVAR_ROM ? 'r' : ' ',
					var->flags & CVAR_ARCHIVE ? '*' : ' ',
					var->flags & CVAR_USERINFO ? 'u' : ' ',
					var->flags & CVAR_SERVERINFO ? 's' : ' ');
		if (showhelp)
			Con_Printf ("%-20s : %s\n", var->name, var->description);
		else
			Con_Printf ("%s\n", var->name);
	}

	Con_Printf ("------------\n%d variables\n", i);
}

static void
cvar_free (void *c, void *unused)
{
	cvar_t *cvar = (cvar_t*)c;
	free (cvar->name);
	free (cvar->string);
	free (cvar);
}

static char *
cvar_get_key (void *c, void *unused)
{
	cvar_t *cvar = (cvar_t*)c;
	return cvar->name;
}

static void
calias_free (void *c, void *unused)
{
	cvar_alias_t *calias = (cvar_alias_t*)c;
	free (calias->name);
	free (calias);
}

static char *
calias_get_key (void *c, void *unused)
{
	cvar_alias_t *calias = (cvar_alias_t*)c;
	return calias->name;
}

void
Cvar_Init_Hash (void)
{
	cvar_hash = Hash_NewTable (1021, cvar_get_key, cvar_free, 0);
	calias_hash = Hash_NewTable (1021, calias_get_key, calias_free, 0);
}

void
Cvar_Init (void)
{
	developer = Cvar_Get ("developer", "0", CVAR_NONE, NULL,
			"set to enable extra debugging information");

	Cmd_AddCommand ("set", Cvar_Set_f, "Set the selected variable, useful on the command line (+set variablename setting)");
	Cmd_AddCommand ("setrom", Cvar_Setrom_f, "Set the selected variable and make it read only, useful on the command line.\n"
		"(+setrom variablename setting)");
	Cmd_AddCommand ("toggle", Cvar_Toggle_f, "Toggle a cvar on or off");
	Cmd_AddCommand ("help", Cvar_Help_f, "Display quake help");
	Cmd_AddCommand ("cvarlist", Cvar_CvarList_f, "List all cvars");
}

void
Cvar_Shutdown (void)
{
	cvar_t     *var, *next;
	cvar_alias_t *alias, *nextalias;

	// Free cvars
	var = cvar_vars;
	while (var) {
		next = var->next;
		free (var->string);
		free (var->name);
		free (var);
		var = next;
	}
	// Free aliases 
	alias = calias_vars;
	while (alias) {
		nextalias = alias->next;
		free (alias->name);
		free (alias);
		alias = nextalias;
	}
}


cvar_t *
Cvar_Get (char *name, char *string, int cvarflags, void (*callback)(cvar_t*),
		  char *description)
{

	cvar_t     *var;

	if (Cmd_Exists (name)) {
		Con_Printf ("Cvar_Get: %s is a command\n", name);
		return NULL;
	}
	var = Cvar_FindVar (name);
	if (!var) {
		cvar_t    **v;
		var = (cvar_t *) calloc (1, sizeof (cvar_t));

		// Cvar doesn't exist, so we create it
		var->name = strdup (name);
		var->string = strdup (string);
		var->flags = cvarflags;
		var->callback = callback;
		var->description = description;
		var->value = atof (var->string);
		var->int_val = atoi (var->string);
		sscanf (var->string, "%f %f %f",
				&var->vec[0], &var->vec[1], &var->vec[2]);
		Hash_Add (cvar_hash, var);

		for (v = &cvar_vars; *v; v = &(*v)->next)
			if (strcmp ((*v)->name, var->name) >= 0)
				break;
		var->next = *v;
		*v = var;
	} else {
		// Cvar does exist, so we update the flags and return.
		var->flags &= ~CVAR_USER_CREATED;
		var->flags |= cvarflags;
		var->callback = callback;
		var->description = description;
	}
	if (var->callback)
		var->callback (var);

	return var;
}

/*
	Cvar_SetFlags

	sets a Cvar's flags simply and easily
*/
void
Cvar_SetFlags (cvar_t *var, int cvarflags)
{
	if (var == NULL)
		return;

	var->flags = cvarflags;
}
