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
static const char rcsid[] = 
	"$Id$";

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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/sizebuf.h"
#include "QF/sys.h"
#include "QF/vfs.h"
#include "QF/zone.h"

#include "compat.h"

typedef struct cmdalias_s {
	struct cmdalias_s *next;
	const char       *name;
	const char       *value;
} cmdalias_t;

cmdalias_t *cmd_alias;
cmd_source_t cmd_source;
qboolean    cmd_wait;

cvar_t     *cmd_warncmd;
cvar_t     *cmd_highchars;

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
Cbuf_AddText (const char *text)
{
	int         l;

	l = strlen (text);

	if (cmd_text.cursize + l < cmd_text.maxsize) {
		SZ_Write (&cmd_text, text, l + 1);
		return;
	}
	Sys_Printf ("Cbuf_AddText: overflow\n");
}

/*
	Cbuf_InsertText

	Adds command text immediately after the current command
	Adds a \n to the text
	TODO: Can we just read the buffer in the reverse order?
*/
void
Cbuf_InsertText (const char *text)
{
	int         textlen;

	textlen = strlen (text);
	if (cmd_text.cursize + 1 + textlen >= cmd_text.maxsize) {
		Sys_Printf ("Cbuf_InsertText: overflow\n");
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
	// down this is necessary because commands (exec, alias) can insert
	// data at the beginning of the text buffer

	if (i == cmd_text.cursize)
		cmd_text.cursize = 0;
	else {
		i++;
		cmd_text.cursize -= i;
		memcpy (text, text + i, cmd_text.cursize);
	}
}

void
Cbuf_Execute (void)
{
	char        line[1024];

	while (cmd_text.cursize) {
		extract_line (line);
		// execute the command line
		// Sys_DPrintf("+%s\n",line),
		Cmd_ExecuteString (line, src_command);

		if (cmd_wait) {					// skip out while text still remains
										// in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	}
}

void
Cbuf_Execute_Sets (void)
{
	char        line[1024];

	while (cmd_text.cursize) {
		extract_line (line);
		// execute the command line
		if (strnequal (line, "set", 3) && isspace ((int) line[3])) {
			// Sys_DPrintf ("+%s\n",line);
			Cmd_ExecuteString (line, src_command);
		} else if (strnequal (line, "setrom", 6) && isspace ((int) line[6])) {
			// Sys_DPrintf ("+%s\n",line);
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
	if (!build)
		Sys_Error ("Cmd_StuffCmds_f: Memory Allocation Failure\n");
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

	// Sys_Printf("[\n%s]\n",build);

	if (build[0])
		Cbuf_InsertText (build);

	free (build);
}

void
Cmd_Exec_File (const char *path)
{
	char       *f;
	int         len;
	VFile      *file;

	if (!path || !*path)
		return;
	if ((file = Qopen (path, "r")) != NULL) {
		len = COM_filelength (file);
		f = (char *) malloc (len + 1);
		if (f) {
			f[len] = 0;
			Qread (file, f, len);
			Qclose (file);
			Cbuf_InsertText (f);
			free (f);
		}
	}
}

void
Cmd_Exec_f (void)
{
	char       *f;
	int         mark;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("exec <filename> : execute a script file\n");
		return;
	}
	// FIXME: is this safe freeing the hunk here?
	mark = Hunk_LowMark ();
	f = (char *) COM_LoadHunkFile (Cmd_Argv (1));
	if (!f) {
		Sys_Printf ("couldn't exec %s\n", Cmd_Argv (1));
		return;
	}
	if (!Cvar_Command () && (cmd_warncmd->int_val
							 || (developer && developer->int_val)))
		Sys_Printf ("execing %s\n", Cmd_Argv (1));

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
		Sys_Printf ("%s ", Cmd_Argv (i));
	Sys_Printf ("\n");
}

/*
	Cmd_Alias_f

	Creates a new command that executes a command string (possibly ; seperated)
*/
void
Cmd_Alias_f (void)
{
	cmdalias_t *alias;
	char       *cmd;
	int         i, c;
	const char       *s;

	if (Cmd_Argc () == 1) {
		Sys_Printf ("Current alias commands:\n");
		for (alias = cmd_alias; alias; alias = alias->next)
			Sys_Printf ("alias %s \"%s\"\n", alias->name, alias->value);
		return;
	}

	s = Cmd_Argv (1);
	// if the alias already exists, reuse it
	alias = (cmdalias_t*)Hash_Find (cmd_alias_hash, s);
	if (alias) {
		free ((char*)alias->value);
	} else {
		cmdalias_t **a;

		alias = calloc (1, sizeof (cmdalias_t));
		alias->name = strdup (s);
		Hash_Add (cmd_alias_hash, alias);
		for (a = &cmd_alias; *a; a = &(*a)->next)
			if (strcmp ((*a)->name, alias->name) >=0)
				break;
		alias->next = *a;
		*a = alias;
	}

	// copy the rest of the command line
	cmd = malloc (strlen (Cmd_Args (1)) + 2);// can never be longer
	if (!cmd)
		Sys_Error ("Cmd_Alias_f: Memory Allocation Failure\n");
	cmd[0] = 0;							// start out with a null string
	c = Cmd_Argc ();
	for (i = 2; i < c; i++) {
		strcat (cmd, Cmd_Argv (i));
		if (i != c - 1)
			strcat (cmd, " ");
	}

	alias->value = cmd;
}

void
Cmd_UnAlias_f (void)
{
	cmdalias_t *alias;
	const char       *s;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("unalias <alias>: erase an existing alias\n");
		return;
	}

	s = Cmd_Argv (1);
	alias = Hash_Del (cmd_alias_hash, s);

	if (alias) {
		cmdalias_t **a;

		for (a = &cmd_alias; *a != alias; a = &(*a)->next)
			;
		*a = alias->next;

		free ((char*)alias->name);
		free ((char*)alias->value);
		free (alias);
	} else {
		Sys_Printf ("Unknown alias \"%s\"\n", s);
	}
}


/*
					COMMAND EXECUTION
*/

typedef struct cmd_function_s {
	struct cmd_function_s *next;
	const char       *name;
	xcommand_t  function;
	const char       *description;
} cmd_function_t;


#define	MAX_ARGS		80

static int  cmd_argc;
static char *cmd_argv[MAX_ARGS];
static const char *cmd_null_string = "";
static const char *cmd_args[MAX_ARGS];

static cmd_function_t *cmd_functions;	// possible commands to execute

int
Cmd_Argc (void)
{
	return cmd_argc;
}

const char       *
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
const char       *
Cmd_Args (int start)
{
	if (start >= cmd_argc || !cmd_args[start])
		return "";
	return cmd_args[start];
}

/*
	Cmd_TokenizeString

	Parses the given string into command line tokens.
*/
void
Cmd_TokenizeString (const char *text)
{
	static char *argv_buf;
	static size_t argv_buf_size;
	int         argv_idx;
	size_t         len = strlen (text) + 1;

	if (len > argv_buf_size) {
		argv_buf_size = (len + 1023) & ~1023;
		argv_buf = realloc (argv_buf, argv_buf_size);
		if (!argv_buf)
			Sys_Error ("Cmd_TokenizeString: could not allocate %d bytes",
					   argv_buf_size);
	}

	argv_idx = 0;

	cmd_argc = 0;
	memset (cmd_args, 0, sizeof (cmd_args));

	while (1) {
		// skip whitespace up to a \n
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

		if (cmd_argc < MAX_ARGS)
		    cmd_args[cmd_argc] = text;

		text = COM_Parse (text);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS) {
			cmd_argv[cmd_argc] = argv_buf + argv_idx;
			strcpy (cmd_argv[cmd_argc], com_token);
			argv_idx += strlen (com_token) + 1;

			cmd_argc++;
		}
	}
}

void
Cmd_AddCommand (const char *cmd_name, xcommand_t function, const char *description)
{
	cmd_function_t *cmd;
	cmd_function_t **c;

	// fail if the command is a variable name
	if (Cvar_FindVar (cmd_name)) {
		Sys_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}
	// fail if the command already exists
	cmd = (cmd_function_t*)Hash_Find (cmd_hash, cmd_name);
	if (cmd) {
		Sys_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
		return;
	}

	cmd = malloc (sizeof (cmd_function_t));
	if (!cmd)
		Sys_Error ("Cmd_AddCommand: Memory_Allocation_Failure\n");
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

qboolean
Cmd_Exists (const char *cmd_name)
{
	cmd_function_t *cmd;

	cmd = (cmd_function_t*)Hash_Find (cmd_hash, cmd_name);
	if (cmd) {
		return true;
	}

	return false;
}

const char       *
Cmd_CompleteCommand (const char *partial)
{
	cmd_function_t	*cmd;
	int				len;
	cmdalias_t		*a;

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
	Cmd_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
*/
int
Cmd_CompleteCountPossible (const char *partial)
{
	cmd_function_t	*cmd;
	int				len;
	int				h;
	
	h = 0;
	len = strlen(partial);
	
	if (!len)
		return 0;
	
	// Loop through the command list and count all partial matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp(partial, cmd->name, len))
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
const char	**
Cmd_CompleteBuildList (const char *partial)
{
	cmd_function_t	*cmd;
	int				len = 0;
	int				bpos = 0;
	int				sizeofbuf = (Cmd_CompleteCountPossible (partial) + 1) * sizeof (char *);
	const char			**buf;

	len = strlen(partial);
	buf = malloc(sizeofbuf + sizeof (char *));
	if (!buf)
		Sys_Error ("Cmd_CompleteBuildList: Memory Allocation Failure\n");
	// Loop through the alias list and print all matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp(partial, cmd->name, len))
			buf[bpos++] = cmd->name;

	buf[bpos] = NULL;
	return buf;
}

/*
	Cmd_CompleteAlias

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
*/
const char *
Cmd_CompleteAlias (const char * partial)
{
	cmdalias_t	*alias;
	int			len;

	len = strlen(partial);

	if (!len)
		return NULL;

	// Check functions
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			return alias->name;

	return NULL;
}

/*
	Cmd_CompleteAliasCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
*/
int
Cmd_CompleteAliasCountPossible (const char *partial)
{
	cmdalias_t	*alias;
	int			len;
	int			h;

	h = 0;

	len = strlen(partial);

	if (!len)
		return 0;

	// Loop through the command list and count all partial matches
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			h++;

	return h;
}

/*
	Cmd_CompleteAliasBuildList

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
*/
const char	**
Cmd_CompleteAliasBuildList (const char *partial)
{
	cmdalias_t	*alias;
	int			len = 0;
	int			bpos = 0;
	int			sizeofbuf = (Cmd_CompleteAliasCountPossible (partial) + 1) *
							 sizeof (char *);
	const char		**buf;

	len = strlen(partial);
	buf = malloc(sizeofbuf + sizeof (char *));
	if (!buf)
		Sys_Error ("Cmd_CompleteAliasBuildList: Memory Allocation Failure\n");
	// Loop through the alias list and print all matches
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			buf[bpos++] = alias->name;

	buf[bpos] = NULL;
	return buf;
}

/*
	Cmd_ExpandVariables

	Expand $fov-like expressions
	FIXME: better handling of buffer overflows?
*/
// dest must point to a 1024-byte buffer
void
Cmd_ExpandVariables (const char *data, char *dest)
{
	unsigned int c;
	char        buf[1024];
	int         i, len;
	cvar_t      *bestvar;
	int         quotes = 0;

	len = 0;

	// parse a regular word
	while ((c = *data) != 0) {
		if (c == '"')
			quotes++;
		if (c == '$' && *(data+1) == '{' && !(quotes & 1)) {
			data+=2;
			// Copy the text between the braces to a temp buffer
			i = 0;
			buf[0] = 0;
			bestvar = NULL;
			while ((c = *data) != 0 && c != '}') {
				data++;
				buf[i++] = c;
				buf[i] = 0;
				if (i >= sizeof (buf) - 1)
					break;
			}
			data++;
			bestvar = Cvar_FindVar(buf);

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
				dest[len++] = '{';
				if (len + strlen (buf) >= 1024)
					break;
				strcpy (&dest[len], buf);
				len += strlen (buf);
				dest[len++] = '}';
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
Cmd_ExecuteString (const char *text, cmd_source_t src)
{
	cmd_function_t *cmd;
	cmdalias_t *a;
	char        buf[1024];

	cmd_source = src;

	
#if 0
	Cmd_TokenizeString (text);
#else
	if (cmd_highchars->value) {
		char buf2[1024];

		strncpy(buf, text, sizeof(buf) - 1);
		buf[sizeof(buf)] = 0;
		Cmd_ParseSpecial (buf);
		Cmd_ExpandVariables (buf, buf2);
		Cmd_TokenizeString (buf2);
	}
	else {
		Cmd_ExpandVariables (text, buf);
		Cmd_TokenizeString (buf);
	}
#endif

	// execute the command line
	if (!Cmd_Argc ())
		return;							// no tokens

	// check functions
	cmd = (cmd_function_t*)Hash_Find (cmd_hash, cmd_argv[0]);
	if (cmd) {
		if (cmd->function)
			cmd->function ();
		return;
	}

	// Tonik: check cvars
	if (Cvar_Command ())
		return;

	// check alias
	a = (cmdalias_t*)Hash_Find (cmd_alias_hash, cmd_argv[0]);
	if (a) {
		Cbuf_InsertText ("\n");
		Cbuf_InsertText (a->value);
		return;
	}

	if (cmd_warncmd->int_val || developer->int_val)
		Sys_Printf ("Unknown command \"%s\"\n", Cmd_Argv (0));
}

/*
	Cmd_CheckParm

	Returns the position (1 to argc-1) in the command's argument list
	where the given parameter apears, or 0 if not present
*/
int
Cmd_CheckParm (const char *parm)
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
			Sys_Printf ("%-20s :\n%s\n", cmd->name, cmd->description);
		} else {
			Sys_Printf ("%s\n", cmd->name);
		}
	}

	Sys_Printf ("------------\n%d commands\n", i);
}

void
Cmd_Help_f (void)
{
	const char     *name;
	cvar_t         *var;
	cmd_function_t *cmd;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("usage: help <cvar/command>\n");
		return;
	}

	name = Cmd_Argv (1);

	for (cmd = cmd_functions; cmd && strcasecmp (name, cmd->name);
		 cmd = cmd->next)
		;
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

void
Cmd_Hash_Stats_f (void)
{
	Sys_Printf ("alias hash table:\n");
	Hash_Stats (cmd_alias_hash);
	Sys_Printf ("command hash table:\n");
	Hash_Stats (cmd_hash);
}

static void
cmd_alias_free (void *_a, void *unused)
{
	cmdalias_t *a = (cmdalias_t*)_a;
	free ((char*)a->name);
	free ((char*)a->value);
	free (a);
}

static const char *
cmd_alias_get_key (void *_a, void *unused)
{
	cmdalias_t *a = (cmdalias_t*)_a;
	return a->name;
}

static const char *
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
	cmd_alias_hash = Hash_NewTable (1021, cmd_alias_get_key, cmd_alias_free,
									0);
}

void
Cmd_Init (void)
{
	// register our commands
	Cmd_AddCommand ("stuffcmds", Cmd_StuffCmds_f, "Execute the commands given "
					"at startup again");
	Cmd_AddCommand ("exec", Cmd_Exec_f, "Execute a script file");
	Cmd_AddCommand ("echo", Cmd_Echo_f, "Print text to console");
	Cmd_AddCommand ("alias", Cmd_Alias_f, "Used to create a reference to a "
					"command or list of commands.\n"
					"When used without parameters, displays all current "
					"aliases.\n"
					"Note: Enclose multiple commands within quotes and "
					"seperate each command with a semi-colon.");
	Cmd_AddCommand ("unalias", Cmd_UnAlias_f, "Remove the selected alias");
	Cmd_AddCommand ("wait", Cmd_Wait_f, "Wait a game tic");
	Cmd_AddCommand ("cmdlist", Cmd_CmdList_f, "List all commands");
	Cmd_AddCommand ("help", Cmd_Help_f, "Display help for a command or "
					"variable");
	//Cmd_AddCommand ("cmd_hash_stats", Cmd_Hash_Stats_f, "Display statistics "
	//				"alias and command hash tables");
	cmd_warncmd = Cvar_Get ("cmd_warncmd", "0", CVAR_NONE, NULL, "Toggles the "
							"display of error messages for unknown commands");
	cmd_highchars = Cvar_Get ("cmd_highchars", "0", CVAR_NONE, NULL, "Toggles availability of special "
							"characters by proceeding letters by $ or #.  See "
							"the documentation for details.");
}

char        *com_token;
static size_t com_token_size;

static inline void
write_com_token (size_t pos, char c)
{
	if (pos + 1 <= com_token_size) {
write:
		com_token[pos] = c;
		return;
	}
	com_token_size = (pos + 1024) & ~1023;
	com_token = realloc (com_token, com_token_size);
	if (!com_token)
		Sys_Error ("COM_Parse: could not allocate %d bytes",
				   com_token_size);
	goto write;
}

/*
	COM_Parse

	Parse a token out of a string
*/
const char       *
COM_Parse (const char *_data)
{
	const byte *data = (const byte *)_data;
	unsigned int c;
	size_t      len = 0;

	write_com_token (len, 0);

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
				write_com_token (len, 0);
				return c ? data : data - 1;
			}
			write_com_token (len, c);
			len++;
		}
	}
	// parse a regular word
	do {
		write_com_token (len, c);
		data++;
		len++;

		c = *data;
	} while (c > 32);

	write_com_token (len, 0);
	return data;
}

struct stable_s {char a, b;} stable1[] =
{
	{'\\',0x0D}, // Fake message
	{'[', 0x90}, // Gold braces
	{']', 0x91},
	{'(', 0x80}, // Scroll bar characters
	{'=', 0x81},
	{')', 0x82},
	{'|', 0x83},
	{'<', 0x9D}, // Vertical line characters
	{'-', 0X9E},
	{'>', 0x9F},
	{'.', 0x8E}, // Gold dot
	{',', 0x0E}, // White dot
	{'G', 0x86}, // Ocrana leds from ocrana.wad
	{'R', 0x87},
	{'Y', 0x88},
	{'B', 0x89},
	{'a', 0x7F}, // White arrow
	{'A', 0x8D}, // Brown arrow
	{'0', 0x92}, // Gold numbers
	{'1', 0x93},
	{'2', 0x94},
	{'3', 0x95},
	{'4', 0x96},
	{'5', 0x97},
	{'6', 0x98},
	{'7', 0x99},
	{'8', 0x9A},
	{'9', 0x9B},
	{'n', '\n'}, // Newline!
	{0, 0}
};
	

void Cmd_ParseSpecial (char *s)
{
	char        *d;
	int         i, i2;
	char        c = 0;

	i = 0;
	d = s;

	while (*s) {
		if ((*s == '\\') && ((s[1] == '#') || (s[1] == '$') || (s[1] == '\\'))) {
			d[i++] = s[1];
			s+=2;
			continue;
		}
		if ((*s == '$') && (s[1] != '\0')) {
			for (i2 = 0; stable1[i2].a; i2++) {
				if (s[1] == stable1[i2].a) {
					c = stable1[i2].b;
					break;
				}
			}
			if (c) {
				d[i++] = c;
				s += 2;
				continue;
			}
		}
		if ((*s == '#') && (s[1] != '\0')) {
			d[i++] = s[1] ^ 128;
			s += 2;
			continue;
		}
		d[i++] = *s++;
	}
	d[i] = 0;
	return;
}

