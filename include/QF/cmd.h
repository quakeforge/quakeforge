/*
	cmd.h

	Command buffer and command execution

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

#ifndef __cmd_h
#define __cmd_h


#include "QF/qtypes.h"

typedef struct cmd_localvar_s {
	struct dstring_s *key, *value;
} cmd_localvar_t;

typedef struct cmd_token_s {
	struct dstring_s *original, *processed; // Token before and after processing
	enum {
		cmd_original,
		cmd_process,
		cmd_done
	} state;
	unsigned int pos; // Last position in string (used by Cmd_ProcessEmbedded)
	unsigned int space; // Amount of white space before token
	char delim; // Character that delimeted token
} cmd_token_t;

typedef struct cmd_buffer_s {
	// Data
	struct dstring_s *buffer; // Actual text
	unsigned int argc, maxargc; // Number of args, number of args allocated
	struct cmd_token_s **argv; // Tokens
	struct dstring_s *realline; // Actual command being processed
	struct dstring_s *line; // Reassembled line
	struct dstring_s *looptext; // If a looping buffer, the text we are looping on
	struct dstring_s *retval; // Return value
	unsigned int *args; // Array of positions of each token in composite line
	struct hashtab_s *locals; // Local variables
	
	// Flags
	qboolean subroutine; // Temporarily stopped so a subroutine can run
	qboolean wait; // Execution paused until next frame
	qboolean legacy; // Backwards compatible with old console buffer
	qboolean ownvars; // Buffer has its own private local variables
	qboolean loop; // Buffer loops itself
	
	// Execution position
	enum {
		cmd_ready,
		cmd_tokenized,
		cmd_processed
	} position;
	
	// Return value status
	enum {
		cmd_normal,  // Normal status
		cmd_waiting, // Waiting for a return value
		cmd_returned // Return value available
	} returned;
	
	// Stack
	struct cmd_buffer_s *prev, *next; // Neighboring buffers in stack
} cmd_buffer_t;

//===========================================================================

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();

*/


void Cbuf_Init (void);
// allocates an initial text buffer that will grow as needed

void Cbuf_AddTextTo (cmd_buffer_t *buffer, const char *text);
void Cbuf_AddText (const char *text);
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.

void Cbuf_InsertText (const char *text);
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.

void Cbuf_Execute_Sets (void);
void Cbuf_Execute (void);
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!


//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

Commands can come from three sources, but the handler functions may choose
to dissallow the action or forward it to a remote server if the source is
not apropriate.

*/

typedef void (*xcommand_t) (void);

typedef enum {
    src_client,			// came in over a net connection as a clc_stringcmd
						// host_client will be valid during this state.
    src_command,		// from the command buffer
} cmd_source_t;

extern cmd_source_t	cmd_source;

void	Cmd_Init_Hash (void);
void	Cmd_Init (void);
void	cl_Cmd_Init (void);

int		Cmd_AddCommand (const char *cmd_name, xcommand_t function, const char *description);
int		Cmd_RemoveCommand (const char *cmd_name);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_stringcmd instead of executed locally

qboolean Cmd_Exists (const char *cmd_name);
// used by the cvar code to check for cvar / command name overlap

const char 	*Cmd_CompleteCommand (const char *partial);
// attempts to match a partial command for automatic command line completion
// returns NULL if nothing fits

int		Cmd_CompleteAliasCountPossible (const char *partial);
const char	**Cmd_CompleteAliasBuildList (const char *partial);
int		Cmd_CompleteCountPossible (const char *partial);
const char	**Cmd_CompleteBuildList (const char *partial);
const char	*Cmd_CompleteAlias (const char *partial);
// Enhanced console completion by Fett erich@heintz.com
// Added by EvilTypeGuy eviltypeguy@qeradiant.com


int		Cmd_Argc (void);
const char	*Cmd_Argv (int arg);
const char	*Cmd_Args (int start);
const char	*Cmd_Argu (int arg);
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

int Cmd_CheckParm (const char *parm);
// Returns the position (1 to argc-1) in the command's argument list
// where the given parameter apears, or 0 if not present

int Cmd_Process (void);
void Cmd_TokenizeString (const char *text, qboolean legacy);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void   Cmd_ExecuteParsed (cmd_source_t src);
int    Cmd_ExecuteString (const char *text, cmd_source_t src);
// Parses a single line of text into arguments and tries to execute it.
// The text can come from the command buffer, a remote client, or stdin.


void	Cmd_ForwardToServer (void);
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

void Cmd_StuffCmds_f (void);

void Cmd_Exec_File (const char *path);

extern char	*com_token;
const char *COM_Parse (const char *data);

// Returns a value to GIB so that it can be picked up for embedded commands
void	Cmd_Return (const char *value);

extern struct cvar_s *cmd_warncmd;

extern cmd_buffer_t *cmd_legacybuffer; // Allow access to the legacy buffer as an alternate console buffer
extern cmd_buffer_t *cmd_keybindbuffer; // Allow access to dedicated key binds command buffer

#endif // __cmd_h
