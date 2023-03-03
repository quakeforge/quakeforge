/*
	cmd.c

	script command processing module

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
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include "QF/cbuf.h"
#include "QF/idparse.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/zone.h"

typedef struct cmdalias_s {
	struct cmdalias_s *next;
	const char *name;
	const char *value;
} cmdalias_t;

typedef struct cmd_provider_s
{
	const char *name;
	cbuf_interpreter_t* interp;
} cmd_provider_t;

static cmdalias_t *cmd_alias;

VISIBLE int cmd_warncmd;
static cvar_t cmd_warncmd_cvar = {
	.name = "cmd_warncmd",
	.description =
		"Toggles the display of error messages for unknown commands",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cmd_warncmd },
};

static hashtab_t  *cmd_alias_hash;
static hashtab_t  *cmd_hash;
static hashtab_t  *cmd_provider_hash;

VISIBLE cbuf_args_t *cmd_args;
static cbuf_t *cmd_cbuf;
VISIBLE cmd_source_t cmd_source;


/* Command parsing functions */

static cmd_function_t *cmd_functions;	// possible commands to execute

VISIBLE int
Cmd_Argc (void)
{
	return cmd_args->argc;
}

VISIBLE const char *
Cmd_Argv (int arg)
{
	if (arg >= cmd_args->argc)
		return "";
	return cmd_args->argv[arg]->str;
}

VISIBLE const char *
Cmd_Args (int start)
{
	if (start >= cmd_args->argc)
		return "";
	return cmd_args->args[start];
}

VISIBLE int
Cmd_Command (cbuf_args_t *args)
{
	cmd_function_t *cmd;

	cmd_args = args;
	//cmd_source = src;

	if (!args->argc)
		return 0;							// no tokens

	// check functions
	cmd = (cmd_function_t *) Hash_Find (cmd_hash, args->argv[0]->str);
	if (cmd) {
		if (cmd->function) {
			cmd->function ();
		} else if (cmd->datafunc) {
			cmd->datafunc (cmd->data);
		}
		return 0;
	}
	// check cvars
	if (Cvar_Command ())
		return 0;
	if (cbuf_active->unknown_command && cbuf_active->unknown_command ())
		return 0;
	if (cbuf_active->strict)
		return -1;
	else if (cmd_warncmd || developer & SYS_dev)
		Sys_Printf ("Unknown command \"%s\"\n", Cmd_Argv (0));
	return 0;
}

static int
add_command (const char *cmd_name, xcommand_t func, xdatacmd_t datafunc,
			 void *data, const char *description)
{
	cmd_function_t *cmd;
	cmd_function_t **c;

	// fail if the command already exists
	cmd = (cmd_function_t *) Hash_Find (cmd_hash, cmd_name);
	if (cmd) {
		Sys_MaskPrintf (SYS_dev, "Cmd_AddCommand: %s already defined\n",
						cmd_name);
		return 0;
	}

	cmd = calloc (1, sizeof (cmd_function_t));
	SYS_CHECKMEM (cmd);
	cmd->name = cmd_name;
	cmd->function = func;
	cmd->datafunc = datafunc;
	cmd->data = data;
	cmd->description = description;
	Hash_Add (cmd_hash, cmd);
	for (c = &cmd_functions; *c; c = &(*c)->next)
		if (strcmp ((*c)->name, cmd->name) >= 0)
			break;
	cmd->next = *c;
	*c = cmd;
	return 1;
}

/* Registers a command and handler function */
VISIBLE int
Cmd_AddCommand (const char *cmd_name, xcommand_t function,
				const char *description)
{
	return add_command (cmd_name, function, 0, 0, description);
}

/* Registers a command and handler function with data */
VISIBLE int
Cmd_AddDataCommand (const char *cmd_name, xdatacmd_t function,
					void *data, const char *description)
{
	return add_command (cmd_name, 0, function, data, description);
}

/* Unregisters a command */
VISIBLE int
Cmd_RemoveCommand (const char *name)
{
	cmd_function_t *cmd;
	cmd_function_t **c;

	cmd = (cmd_function_t *) Hash_Del (cmd_hash, name);
	if (!cmd)
		return 0;
	for (c = &cmd_functions; *c; c = &(*c)->next)
		if (*c == cmd)
			break;
	*c = cmd->next;
	free (cmd);
	return 1;
}

/* Checks for the existance of a command */
VISIBLE qboolean
Cmd_Exists (const char *cmd_name)
{
	cmd_function_t *cmd;

	cmd = (cmd_function_t *) Hash_Find (cmd_hash, cmd_name);
	if (cmd) {
		return true;
	}

	return false;
}


/* Command completion functions */

VISIBLE const char *
Cmd_CompleteCommand (const char *partial)
{
	cmd_function_t *cmd;
	int         len;

	len = strlen (partial);

	if (!len)
		return NULL;

	// check for exact match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strcmp (partial, cmd->name))
			return cmd->name;

	// check for partial match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncmp (partial, cmd->name, len))
			return cmd->name;

	return NULL;
}

/*
	Cmd_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
*/
VISIBLE int
Cmd_CompleteCountPossible (const char *partial)
{
	cmd_function_t *cmd;
	int         len;
	int         h;

	h = 0;
	len = strlen (partial);

	if (!len)
		return 0;

	// Loop through the command list and count all partial matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncmp (partial, cmd->name, len))
			h++;

	return h;
}

/*
	Cmd_CompleteBuildList

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
*/
VISIBLE const char **
Cmd_CompleteBuildList (const char *partial)
{
	cmd_function_t *cmd;
	int         len = 0;
	int         bpos = 0;
	int         sizeofbuf;
	const char **buf;

	sizeofbuf = (Cmd_CompleteCountPossible (partial) + 1) * sizeof (char *);
	len = strlen (partial);
	buf = malloc (sizeofbuf + sizeof (char *));

	SYS_CHECKMEM (buf);
	// Loop through the alias list and print all matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncmp (partial, cmd->name, len))
			buf[bpos++] = cmd->name;

	buf[bpos] = NULL;
	return buf;
}

/* Hash table functions for aliases and commands */
static void
cmd_free (void *_c, void *unused)
{
	cmd_function_t *cmd = _c;
	free (cmd);
}

static void
cmd_alias_free (void *_a, void *unused)
{
	cmdalias_t *a = (cmdalias_t *) _a;

	free ((char *) a->name);
	free ((char *) a->value);
	free (a);
}

static const char *
cmd_alias_get_key (const void *_a, void *unused)
{
	cmdalias_t *a = (cmdalias_t *) _a;

	return a->name;
}

static const char *
cmd_get_key (const void *c, void *unused)
{
	cmd_function_t *cmd = (cmd_function_t *) c;

	return cmd->name;
}

static void
cmd_provider_free (void *_a, void *unused)
{
	cmd_provider_t *p = (cmd_provider_t *) _a;

	free ((char *) p->name);
	free (p);
}

static const char *
cmd_provider_get_key (const void *_a, void *unused)
{
	cmd_provider_t *p = (cmd_provider_t *) _a;

	return p->name;
}

static void
Cmd_Runalias_f (void)
{
	cmdalias_t *a;

	a = (cmdalias_t *) Hash_Find (cmd_alias_hash, Cmd_Argv (0));

	if (a) {
		Cbuf_InsertText (cbuf_active, a->value);
		return;
	} else {
		Sys_Printf
			("BUG: No alias found for registered command.  Please report this to the QuakeForge development team.");
	}
}

static void
Cmd_Alias_f (void)
{
	cmdalias_t *alias;
	dstring_t  *cmd;
	int         i, c;
	const char *s;

	if (Cmd_Argc () == 1) {
		Sys_Printf ("Current alias commands:\n");
		for (alias = cmd_alias; alias; alias = alias->next)
			Sys_Printf ("alias %s \"%s\"\n", alias->name, alias->value);
		return;
	}

	s = Cmd_Argv (1);
	// if the alias already exists, reuse it
	alias = (cmdalias_t *) Hash_Find (cmd_alias_hash, s);
	if (Cmd_Argc () == 2) {
		if (alias)
			Sys_Printf ("alias %s \"%s\"\n", alias->name, alias->value);
		return;
	}
	if (alias)
		free ((char *) alias->value);
	else if (!Cmd_Exists (s)) {
		cmdalias_t **a;

		alias = calloc (1, sizeof (cmdalias_t));
		SYS_CHECKMEM (alias);
		alias->name = strdup (s);
		Hash_Add (cmd_alias_hash, alias);
		for (a = &cmd_alias; *a; a = &(*a)->next)
			if (strcmp ((*a)->name, alias->name) >= 0)
				break;
		alias->next = *a;
		*a = alias;
		Cmd_AddCommand (alias->name, Cmd_Runalias_f, "Alias command.");
	} else {
		Sys_Printf ("alias: a command with the name \"%s\" already exists.\n", s);
		return;
	}
	// copy the rest of the command line
	cmd = dstring_newstr ();
	c = Cmd_Argc ();
	for (i = 2; i < c; i++) {
		dstring_appendstr (cmd, Cmd_Argv (i));
		if (i != c - 1)
			dstring_appendstr (cmd, " ");
	}

	alias->value = dstring_freeze (cmd);
}

static void
Cmd_UnAlias_f (void)
{
	cmdalias_t *alias;
	const char *s;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("unalias <alias>: erase an existing alias\n");
		return;
	}

	s = Cmd_Argv (1);
	alias = Hash_Del (cmd_alias_hash, s);

	if (alias) {
		cmdalias_t **a;

		Cmd_RemoveCommand (alias->name);

		for (a = &cmd_alias; *a != alias; a = &(*a)->next)
			;
		*a = alias->next;

		free ((char *) alias->name);
		free ((char *) alias->value);
		free (alias);
	} else {
		Sys_Printf ("Unknown alias \"%s\"\n", s);
	}
}

static void
Cmd_CmdList_f (void)
{
	cmd_function_t *cmd;
	int         i;
	int         show_description = 0;

	if (Cmd_Argc () > 1)
		show_description = 1;
	for (cmd = cmd_functions, i = 0; cmd; cmd = cmd->next, i++) {
		if (show_description) {
			Sys_Printf ("%-20s :\n%s\n", cmd->name, cmd->description);
		} else {
			Sys_Printf ("%s\n", cmd->name);
		}
	}

	Sys_Printf ("------------\n%d commands\n", i);
}

static void
Cmd_Help_f (void)
{
	const char *name;
	cvar_t     *var;
	cmd_function_t *cmd;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("usage: help <cvar/command>\n");
		return;
	}

	name = Cmd_Argv (1);

	for (cmd = cmd_functions; cmd && strcmp (name, cmd->name);
		 cmd = cmd->next);
	if (cmd) {
		Sys_Printf ("%s\n", cmd->description);
		return;
	}

	var = Cvar_FindVar (name);
	if (!var)
		var = Cvar_FindAlias (name);
	if (var) {
		Sys_Printf ("%s\n", var->description);
		return;
	}

	Sys_Printf ("variable/command not found\n");
}

static void
Cmd_Exec_f (void)
{
	char       *f;
	size_t      mark;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	mark = Hunk_LowMark (0);
	f = (char *) QFS_LoadHunkFile (QFS_FOpenFile (Cmd_Argv (1)));
	if (!f) {
		Sys_Printf ("couldn't exec %s\n", Cmd_Argv (1));
		return;
	}
	if (!Cvar_Command () && (cmd_warncmd || (developer & SYS_dev)))
		Sys_Printf ("execing %s\n", Cmd_Argv (1));
	Cbuf_InsertText (cbuf_active, f);
	Hunk_FreeToLowMark (0, mark);
}

/*
	Cmd_Echo_f

	Just prints the rest of the line to the console
*/
static void
Cmd_Echo_f (void)
{
	if (Cmd_Argc () == 2)
		Sys_Printf ("%s\n", Cmd_Argv (1));
	else
		Sys_Printf ("%s\n", Cmd_Args (1));
}

/* Pauses execution of the current stack until
next call of Cmd_Execute (usually next frame) */
static void
Cmd_Wait_f (void)
{
	cbuf_active->state = CBUF_STATE_WAIT;
}

/* Pauses execution for a specified number
of seconds */
static void
Cmd_Sleep_f (void)
{
	double waittime;
	cbuf_t *p;
	cbuf_active->state = CBUF_STATE_WAIT;
	waittime = atof (Cmd_Argv (1));
	for (p = cbuf_active; p->up; p = p->up); // Get to top of stack
	p->resumetime = Sys_DoubleTime() + waittime;
}

VISIBLE void
Cmd_StuffCmds (cbuf_t *cbuf)
{
	int         i, j;
	dstring_t  *build;

	if (!*com_cmdline)
		return;

	build = dstring_newstr ();

	// pull out the commands
	for (i = 0; com_cmdline[i]; i++) {
		if (com_cmdline[i] == '+') {
			i++;

			for (j = i;
				 (com_cmdline[j]
				  && !((j == 0 || isspace((byte) com_cmdline[j - 1]))
					   && ((com_cmdline[j] == '+')
						    || (com_cmdline[j] == '-'))));
				 j++)
				;

			dstring_appendsubstr (build, com_cmdline + i, j - i);
			dstring_appendstr (build, "\n");
			i = j - 1;
		}
	}

	if (build->str[0])
		Cbuf_InsertText (cbuf, build->str);

	dstring_delete (build);
}

static void
Cmd_StuffCmds_f (void)
{
	Cmd_StuffCmds (cbuf_active);
}

static void
cmd_shutdown (void *data)
{
	Cbuf_Delete (cmd_cbuf);
	Hash_DelTable (cmd_hash);
	Hash_DelTable (cmd_alias_hash);
	Hash_DelTable (cmd_provider_hash);
}

VISIBLE void
Cmd_Init_Hash (void)
{
	Sys_RegisterShutdown (cmd_shutdown, 0);
	cmd_hash = Hash_NewTable (1021, cmd_get_key, cmd_free, 0, 0);
	cmd_alias_hash = Hash_NewTable (1021, cmd_alias_get_key,
									cmd_alias_free, 0, 0);
	cmd_provider_hash = Hash_NewTable(1021, cmd_provider_get_key,
									  cmd_provider_free, 0, 0);
}

VISIBLE void
Cmd_Init (void)
{
	Cmd_AddCommand ("stuffcmds", Cmd_StuffCmds_f, "Execute the commands given "
					"at startup again");
	Cmd_AddCommand ("alias", Cmd_Alias_f, "Used to create a reference to a "
					"command or list of commands.\n"
					"When used without parameters, displays all current "
					"aliases.\n"
					"Note: Enclose multiple commands within quotes and "
					"seperate each command with a semi-colon.");
	Cmd_AddCommand ("unalias", Cmd_UnAlias_f, "Remove the selected alias");
	Cmd_AddCommand ("cmdlist", Cmd_CmdList_f, "List all commands");
	Cmd_AddCommand ("help", Cmd_Help_f, "Display help for a command or "
					"variable");
	Cmd_AddCommand ("exec", Cmd_Exec_f, "Execute a script file");
	Cmd_AddCommand ("echo", Cmd_Echo_f, "Print text to console");
	Cmd_AddCommand ("wait", Cmd_Wait_f, "Wait a game tic");
	Cmd_AddCommand ("sleep", Cmd_Sleep_f, "Wait for a certain number of seconds.");
	Cvar_Register (&cmd_warncmd_cvar, 0, 0);
	cmd_cbuf = Cbuf_New (&id_interp);

	Cmd_AddProvider("id", &id_interp);
}

VISIBLE int
Cmd_ExecuteString (const char *text, cmd_source_t src)
{
	cbuf_t     *old = cbuf_active;
	cbuf_active = cmd_cbuf;

	cmd_source = src;
	COM_TokenizeString (text, cmd_cbuf->args);
	Cmd_Command (cmd_cbuf->args);

	cbuf_active = old;
	return 0;
}

VISIBLE int
Cmd_Exec_File (cbuf_t *cbuf, const char *path, int qfs)
{
	char       *f;
	int         len;
	QFile      *file;

	if (!path || !*path)
		return 0;
	if (qfs) {
		file = QFS_FOpenFile (path);
	} else {
		char *newpath = Sys_ExpandSquiggle (path);
		file = Qopen (newpath, "r");
		free (newpath);
	}
	if (file) {
		len = Qfilesize (file);
		f = (char *) malloc (len + 1);
		if (f) {
			f[len] = 0;
			Qread (file, f, len);
			Cbuf_InsertText (cbuf, f);
			free (f);
		}
		Qclose (file);
		return 1;
	}
	return 0;
}

VISIBLE void
Cmd_AddProvider(const char *name, cbuf_interpreter_t *interp)
{
	cmd_provider_t *p = (cmd_provider_t *) Hash_Find (cmd_provider_hash, name);

	if (!p)
	{
		cmd_provider_t *p = malloc(sizeof(cmd_provider_t));
		p->name = strdup(name);
		p->interp = interp;
		Hash_Add(cmd_provider_hash, p);
	}
}

VISIBLE cbuf_interpreter_t *
Cmd_GetProvider(const char *name)
{
	cmd_provider_t *p = (cmd_provider_t *) Hash_Find (cmd_provider_hash, name);

	if (!p)
		return NULL;
	else
		return p->interp;
}
