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

#include <ctype.h>

#include "QF/cvar.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/hash.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sizebuf.h"
#include "QF/sys.h"
#include "QF/zone.h"

void        Cmd_ForwardToServer (void);

#define	MAX_ALIAS_NAME	32

typedef struct cmdalias_s {
	struct cmdalias_s *next;
	char        name[MAX_ALIAS_NAME];
	char       *value;
} cmdalias_t;

cmdalias_t *cmd_alias;
cmd_source_t cmd_source;
qboolean    cmd_wait;

cvar_t     *cl_warncmd;

hashtab_t  *cmd_alias_hash;
hashtab_t  *cmd_hash;

//=============================================================================

/*
	Cmd_Wait_f

	Causes execution of the remainder of the command buffer to be delayed until
	next frame.  This allows commands like:
	bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
*/
void
Cmd_Wait_f (void)
{
	cmd_wait = true;
}

/*
						COMMAND BUFFER
*/

sizebuf_t   cmd_text;
byte        cmd_text_buf[8192];

/*
	Cbuf_Init
*/
void
Cbuf_Init (void)
{
	cmd_text.data = cmd_text_buf;
	cmd_text.maxsize = sizeof (cmd_text_buf);
}

/*
	Cbuf_AddText

	Adds command text at the end of the buffer
*/
void
Cbuf_AddText (char *text)
{
	int         l;

	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize) {
		Con_Printf ("Cbuf_AddText: overflow\n");
		return;
	}
	SZ_Write (&cmd_text, text, strlen (text));
}


/*
	Cbuf_InsertText

	Adds command text immediately after the current command
	Adds a \n to the text
	TODO: Can we just read the buffer in the reverse order?
*/
void
Cbuf_InsertText (char *text)
{
	int         textlen;

	textlen = strlen (text);
	if (cmd_text.cursize + 1 + textlen >= cmd_text.maxsize) {
		Con_Printf ("Cbuf_InsertText: overflow\n");
		return;
	}

	if (!cmd_text.cursize) {
		memcpy (cmd_text.data, text, textlen);
		cmd_text.cursize = textlen;
		return;
	}
	// Move up to make room for inserted text
	memmove (cmd_text.data + textlen + 1, cmd_text.data, cmd_text.cursize);
	cmd_text.cursize += textlen + 1;

	// Insert new text
	memcpy (cmd_text.data, text, textlen);
	cmd_text.data[textlen] = '\n';
}


static void
extract_line (char *line)
{
	int         i;
	char       *text;
	int         quotes;

	// find a \n or ; line break
	text = (char *) cmd_text.data;
	quotes = 0;
	for (i = 0; i < cmd_text.cursize; i++) {
		if (text[i] == '"')
			quotes++;
		if (!(quotes & 1) && text[i] == ';')
			break;						// don't break if inside a quoted
										// string
		if (text[i] == '\n' || text[i] == '\r')
			break;
	}

	memcpy (line, text, i);
	line[i] = '\0';
	// delete the text from the command buffer and move remaining commands
	// down
	// this is necessary because commands (exec, alias) can insert data at
	// the
	// beginning of the text buffer

	if (i == cmd_text.cursize)
		cmd_text.cursize = 0;
	else {
		i++;
		cmd_text.cursize -= i;
		memcpy (text, text + i, cmd_text.cursize);
	}
}

/*

	Cbuf_Execute

*/
void
Cbuf_Execute (void)
{
	char        line[1024] = { 0 };

	while (cmd_text.cursize) {
		extract_line (line);
		// execute the command line
		// Con_DPrintf("+%s\n",line),
		Cmd_ExecuteString (line, src_command);

		if (cmd_wait) {					// skip out while text still remains
										// in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	}
}

/*

	Cbuf_Execute

*/
void
Cbuf_Execute_Sets (void)
{
	char        line[1024] = { 0 };

	while (cmd_text.cursize) {
		extract_line (line);
		// execute the command line
		if (strnequal (line, "set", 3) && isspace ((int) line[3])) {
			// Con_DPrintf ("+%s\n",line);
			Cmd_ExecuteString (line, src_command);
		}
		if (strnequal (line, "setrom", 6) && isspace ((int) line[6])) {
			// Con_DPrintf ("+%s\n",line);
			Cmd_ExecuteString (line, src_command);
		}
	}
}

/*
						SCRIPT COMMANDS
*/

/*
	Cmd_StuffCmds_f

	Adds command line parameters as script statements
	Commands lead with a +, and continue until a - or another +
	quake +prog jctest.qp +cmd amlev1
	quake -nosound +cmd amlev1
*/
void
Cmd_StuffCmds_f (void)
{
	int         i, j;
	int         s;
	char       *build, c;

	s = strlen (com_cmdline);
	if (!s)
		return;

// pull out the commands
	build = malloc (s + 1);
	build[0] = 0;

	for (i = 0; i < s - 1; i++) {
		if (com_cmdline[i] == '+') {
			i++;

			for (j = i; !((com_cmdline[j] == '+')
						  || (com_cmdline[j] == '-'
						      && (j==0 || com_cmdline[j - 1] == ' '))
						  || (com_cmdline[j] == 0)); j++);

			c = com_cmdline[j];
			com_cmdline[j] = 0;

			strncat (build, com_cmdline + i, s - strlen (build));
			strncat (build, "\n", s - strlen (build));
			com_cmdline[j] = c;
			i = j - 1;
		}
	}

	// Con_Printf("[\n%s]\n",build);

	if (build[0])
		Cbuf_InsertText (build);

	free (build);
}

/*

  Cmd_Exec_File

*/
void
Cmd_Exec_File (char *path)
{
	char       *f;
	int         len;
	QFile      *file;

	if (!path || !*path)
		return;
	if ((file = Qopen (path, "r")) != NULL) {
		len = COM_filelength (file);
		f = (char *) Hunk_TempAlloc (len + 1);
		if (f) {
			f[len] = 0;
			Qread (file, f, len);
			Qclose (file);
			Cbuf_InsertText (f);
		}
	}
}

/*
	Cmd_Exec_f
*/
void
Cmd_Exec_f (void)
{
	char       *f;
	int         mark;

	if (Cmd_Argc () != 2) {
		Con_Printf ("exec <filename> : execute a script file\n");
		return;
	}
	// FIXME: is this safe freeing the hunk here?
	mark = Hunk_LowMark ();
	f = (char *) COM_LoadHunkFile (Cmd_Argv (1));
	if (!f) {
		Con_Printf ("couldn't exec %s\n", Cmd_Argv (1));
		return;
	}
	if (!Cvar_Command () && ((cl_warncmd && cl_warncmd->int_val)
							 || (developer && developer->int_val)))
		Con_Printf ("execing %s\n", Cmd_Argv (1));

	Cbuf_InsertText (f);
	Hunk_FreeToLowMark (mark);
}


/*
	Cmd_Echo_f

	Just prints the rest of the line to the console
*/
void
Cmd_Echo_f (void)
{
	int         i;

	for (i = 1; i < Cmd_Argc (); i++)
		Con_Printf ("%s ", Cmd_Argv (i));
	Con_Printf ("\n");
}

/*
	Cmd_Alias_f

	Creates a new command that executes a command string (possibly ; seperated)
*/

char       *
CopyString (char *in)
{
	char       *out;

	out = malloc (strlen (in) + 1);
	strcpy (out, in);
	return out;
}

void
Cmd_Alias_f (void)
{
	cmdalias_t *a;
	char        cmd[1024];
	int         i, c;
	char       *s;

	if (Cmd_Argc () == 1) {
		Con_Printf ("Current alias commands:\n");
		for (a = cmd_alias; a; a = a->next)
			Con_Printf ("%s : %s\n", a->name, a->value);
		return;
	}

	s = Cmd_Argv (1);
	if (strlen (s) >= MAX_ALIAS_NAME) {
		Con_Printf ("Alias name is too long\n");
		return;
	}
	// if the alias already exists, reuse it
	a = (cmdalias_t*)Hash_Find (cmd_alias_hash, s);
	if (a) {
		free (a->value);
	}

	if (!a) {
		a = calloc (1, sizeof (cmdalias_t));
		a->next = cmd_alias;
		cmd_alias = a;
		strcpy (a->name, s);
		Hash_Add (cmd_alias_hash, a);
	}

// copy the rest of the command line
	cmd[0] = 0;							// start out with a null string
	c = Cmd_Argc ();
	for (i = 2; i < c; i++) {
		strncat (cmd, Cmd_Argv (i), sizeof (cmd) - strlen (cmd));
		if (i != c)
			strncat (cmd, " ", sizeof (cmd) - strlen (cmd));
	}
	strncat (cmd, "\n", sizeof (cmd) - strlen (cmd));

	a->value = CopyString (cmd);
}

void
Cmd_UnAlias_f (void)
{
	cmdalias_t *a, *prev;
	char       *s;

	if (Cmd_Argc () != 2) {
		Con_Printf ("unalias <alias>: erase an existing alias\n");
		return;
	}

	s = Cmd_Argv (1);
	if (strlen (s) >= MAX_ALIAS_NAME) {
		Con_Printf ("Alias name is too long\n");
		return;
	}

	prev = cmd_alias;
	for (a = cmd_alias; a; a = a->next) {
		if (!strcmp (s, a->name)) {
			Hash_Del (cmd_alias_hash, s);
			free (a->value);
			prev->next = a->next;
			if (a == cmd_alias)
				cmd_alias = a->next;
			free (a);
			return;
		}
		prev = a;
	}
	Con_Printf ("Unknown alias \"%s\"\n", s);
}

/*
					COMMAND EXECUTION
*/

typedef struct cmd_function_s {
	struct cmd_function_s *next;
	char       *name;
	xcommand_t  function;
	char       *description;
} cmd_function_t;


#define	MAX_ARGS		80

static int  cmd_argc;
static char *cmd_argv[MAX_ARGS];
static char *cmd_null_string = "";
static char *cmd_args = NULL;



static cmd_function_t *cmd_functions;	// possible commands to execute

/*
	Cmd_Argc
*/
int
Cmd_Argc (void)
{
	return cmd_argc;
}

/*
	Cmd_Argv
*/
char       *
Cmd_Argv (int arg)
{
	if (arg >= cmd_argc)
		return cmd_null_string;
	return cmd_argv[arg];
}

/*
	Cmd_Args

	Returns a single string containing argv(1) to argv(argc()-1)
*/
char       *
Cmd_Args (void)
{
	if (!cmd_args)
		return "";
	return cmd_args;
}


/*
	Cmd_TokenizeString

	Parses the given string into command line tokens.
*/
void
Cmd_TokenizeString (char *text)
{
	static char argv_buf[1024];
	int         argv_idx;

	argv_idx = 0;

	cmd_argc = 0;
	cmd_args = NULL;

	while (1) {
// skip whitespace up to a /n
		while (*text && *(unsigned char *) text <= ' ' && *text != '\n') {
			text++;
		}

		if (*text == '\n') {			// a newline seperates commands in
										// the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		if (cmd_argc == 1)
			cmd_args = text;

		text = COM_Parse (text);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS) {
			if (argv_idx + strlen (com_token) + 1 > 1024) {
				Con_Printf ("Cmd_TokenizeString: overflow\n");
				return;
			}
			cmd_argv[cmd_argc] = argv_buf + argv_idx;
			strcpy (cmd_argv[cmd_argc], com_token);
			argv_idx += strlen (com_token) + 1;

			cmd_argc++;
		}
	}

}


/*
	Cmd_AddCommand
*/
void
Cmd_AddCommand (char *cmd_name, xcommand_t function, char *description)
{
	cmd_function_t *cmd;
	cmd_function_t **c;

	// fail if the command is a variable name
	if (Cvar_FindVar (cmd_name)) {
		Con_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}
	// fail if the command already exists
	cmd = (cmd_function_t*)Hash_Find (cmd_hash, cmd_name);
	if (cmd) {
		Con_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
		return;
	}

	cmd = malloc (sizeof (cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->description = description;
	Hash_Add (cmd_hash, cmd);
	for (c = &cmd_functions; *c; c = &(*c)->next)
		if (strcmp ((*c)->name, cmd->name) >=0)
			break;
	cmd->next = *c;
	*c = cmd;
}

/*
	Cmd_Exists
*/
qboolean
Cmd_Exists (char *cmd_name)
{
	cmd_function_t *cmd;

	cmd = (cmd_function_t*)Hash_Find (cmd_hash, cmd_name);
	if (cmd) {
		return true;
	}

	return false;
}



/*
	Cmd_CompleteCommand
*/
char       *
Cmd_CompleteCommand (char *partial)
{
	cmd_function_t *cmd;
	int         len;
	cmdalias_t *a;

	len = strlen (partial);

	if (!len)
		return NULL;

// check for exact match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strcasecmp (partial, cmd->name))
			return cmd->name;
	for (a = cmd_alias; a; a = a->next)
		if (!strcasecmp (partial, a->name))
			return a->name;

// check for partial match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp (partial, cmd->name, len))
			return cmd->name;
	for (a = cmd_alias; a; a = a->next)
		if (!strncasecmp (partial, a->name, len))
			return a->name;

	return NULL;
}

/*
	Cmd_ExpandVariables

	Expand $fov-like expressions
	FIXME: better handling of buffer overflows?
*/
// dest must point to a 1024-byte buffer
void
Cmd_ExpandVariables (char *data, char *dest)
{
	unsigned int c;
	char        buf[1024];
	int         i, len;
	cvar_t     *var, *bestvar;
	int         quotes = 0;

	len = 0;

// parse a regular word
	while ((c = *data) != 0) {
		if (c == '"')
			quotes++;
		if (c == '$' && !(quotes & 1)) {
			data++;

			// Copy the text after '$' to a temp buffer
			i = 0;
			buf[0] = 0;
			bestvar = NULL;
			while ((c = *data) > 32) {
				if (c == '$')
					break;
				data++;
				buf[i++] = c;
				buf[i] = 0;
				if ((var = Cvar_FindVar (buf)) != 0)
					bestvar = var;
				if (i >= sizeof (buf) - 1)
					break;
			}

			if (bestvar) {
				// check buffer size
				if (len + strlen (bestvar->string) >= 1024 - 1)
					break;

				strcpy (&dest[len], bestvar->string);
				len += strlen (bestvar->string);
				i = strlen (bestvar->name);
				while (buf[i])
					dest[len++] = buf[i++];
			} else {
				// no matching cvar name was found
				dest[len++] = '$';
				if (len + strlen (buf) >= 1024 - 1)
					break;
				strcpy (&dest[len], buf);
				len += strlen (buf);
			}
		} else {
			dest[len] = c;
			data++;
			len++;
			if (len >= 1024 - 1)
				break;
		}
	};

	dest[len] = 0;
}

/*
	Cmd_ExecuteString

	A complete command line has been parsed, so try to execute it
	FIXME: lookupnoadd the token to speed search?
*/
void
Cmd_ExecuteString (char *text, cmd_source_t src)
{
	cmd_function_t *cmd;
	cmdalias_t *a;
	char        buf[1024];

	cmd_source = src;

#if 0
	Cmd_TokenizeString (text);
#else
	Cmd_ExpandVariables (text, buf);
	Cmd_TokenizeString (buf);
#endif

// execute the command line
	if (!Cmd_Argc ())
		return;							// no tokens

// check functions
	cmd = (cmd_function_t*)Hash_Find (cmd_hash, cmd_argv[0]);
	if (cmd) {
		if (!cmd->function)
			Cmd_ForwardToServer ();
		else
			cmd->function ();
		return;
	}

// Tonik: check cvars
	if (Cvar_Command ())
		return;

// check alias
	a = (cmdalias_t*)Hash_Find (cmd_alias_hash, cmd_argv[0]);
	if (a) {
		Cbuf_InsertText (a->value);
		return;
	}

	if (cl_warncmd->int_val || developer->int_val)
		Con_Printf ("Unknown command \"%s\"\n", Cmd_Argv (0));
}



/*
	Cmd_CheckParm

	Returns the position (1 to argc-1) in the command's argument list
	where the given parameter apears, or 0 if not present
*/
int
Cmd_CheckParm (char *parm)
{
	int         i;

	if (!parm)
		Sys_Error ("Cmd_CheckParm: NULL");

	for (i = 1; i < Cmd_Argc (); i++)
		if (!strcasecmp (parm, Cmd_Argv (i)))
			return i;

	return 0;
}

void
Cmd_CmdList_f (void)
{
	cmd_function_t *cmd;
	int         i;
	int         show_description = 0;

	if (Cmd_Argc() > 1)
		show_description = 1;
	for (cmd = cmd_functions, i = 0; cmd; cmd = cmd->next, i++) {
		if (show_description) {
			Con_Printf ("%-20s :\n%s\n", cmd->name, cmd->description);
		} else {
			Con_Printf ("%s\n", cmd->name);
		}
	}

	Con_Printf ("------------\n%d commands\n", i);
}

static void
cmd_alias_free (void *_a, void *unused)
{
	cmdalias_t *a = (cmdalias_t*)_a;
	free (a->value);
	free (a);
}

static char *
cmd_alias_get_key (void *_a, void *unused)
{
	cmdalias_t *a = (cmdalias_t*)_a;
	return a->name;
}

static char *
cmd_get_key (void *c, void *unused)
{
	cmd_function_t *cmd = (cmd_function_t*)c;
	return cmd->name;
}

/*
	Cmd_Init_Hash

	initialise the command and alias hash tables
*/

void
Cmd_Init_Hash (void)
{
	cmd_hash = Hash_NewTable (1021, cmd_get_key, 0, 0);
	cmd_alias_hash = Hash_NewTable (1021, cmd_alias_get_key, cmd_alias_free, 0);
}

/*
	Cmd_Init
*/
void
Cmd_Init (void)
{
//
// register our commands
//
	Cmd_AddCommand ("stuffcmds", Cmd_StuffCmds_f, "Execute the commands given at startup again");
	Cmd_AddCommand ("exec", Cmd_Exec_f, "Execute a script file");
	Cmd_AddCommand ("echo", Cmd_Echo_f, "Print text to console");
	Cmd_AddCommand ("alias", Cmd_Alias_f, "Used to create a reference to a command or list of commands.\n"
		"When used without parameters, displays all current aliases.\n"
		"Note: Enclose multiple commands within quotes and seperate each command with a semi-colon.");
	Cmd_AddCommand ("unalias", Cmd_UnAlias_f, "Remove the selected alias");
	Cmd_AddCommand ("wait", Cmd_Wait_f, "Wait a game tic");
	Cmd_AddCommand ("cmdlist", Cmd_CmdList_f, "List all commands");
}


char        com_token[MAX_COM_TOKEN];

/*
	COM_Parse

	Parse a token out of a string
*/
char       *
COM_Parse (char *data)
{
	unsigned int c;
	int         len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
  skipwhite:
	while ((c = *data) <= ' ') {
		if (c == 0)
			return NULL;				// end of file;
		data++;
	}

// skip // comments
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"') {
		data++;
		while (1) {
			c = *data++;
			if (c == '\"' || !c) {
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}
// parse a regular word
	do {
		com_token[len] = c;
		data++;
		len++;
		if (len >= MAX_COM_TOKEN - 1)
			break;

		c = *data;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}
