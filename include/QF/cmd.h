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
#include "QF/dstring.h"

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
	char delim; // Character that delimeted token
} cmd_token_t;

typedef struct cmd_buffer_s {
	// Data
	struct dstring_s *buffer; // Actual text
	unsigned int argc, maxargc; // Number of args, number of args allocated
	struct cmd_token_s **argv; // Tokens
	struct dstring_s *realline; // Actual command being processed
	struct dstring_s *line; // Processed command line
	struct dstring_s *looptext; // If a looping buffer, the text we are looping on
	struct dstring_s *retval; // Value returned to this buffer
	unsigned int *args; // Array of positions of each token in processed line
	unsigned int *argsu; // Array of positions of each token in original line
	struct hashtab_s *locals; // Local variables

	// Flags
	qboolean subroutine; // Temporarily stopped so a subroutine can run
	qboolean again; // The last command needs to be executed again for some reason
	qboolean wait; // Execution paused until next frame
	qboolean legacy; // Backwards compatible with old console buffer
	qboolean ownvars; // Buffer has its own set of local variables (as opposed to sharing with another buffer)
	unsigned int loop; // Buffer loops itself.  If true, value signifies number of loops done so far
	qboolean embedded; // Buffer exists to evaluate embedded command
	qboolean restricted; // Restricted commands should not run in this buffer

	// Sleep data
	double timeleft;
	double lasttime;

	// Execution position
	enum {
		cmd_ready, // Ready to read a command
		cmd_tokenized, // Command successfully tokenized
		cmd_processed // Command successfully processed, it can be run
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

typedef struct cmd_thread_s {
	struct cmd_buffer_s *cbuf; // Actual buffer
	long int id; // Thread id
	struct cmd_thread_s *prev, *next; // Linked list
} cmd_thread_t;

//===========================================================================

void	escape (dstring_t * dstr, const char *clist);

/*

Any number of commands can be added in a frame, from several different sources,
into several different buffers.

Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();

*/


void Cbuf_Init (void);
// allocates all needed command buffers

void Cbuf_AddTextTo (cmd_buffer_t *buffer, const char *text);
// adds text to the end of a specific buffer
void Cbuf_AddText (const char *text);
// adds text to the end of the active buffer.  By default this is the console bufer

void Cbuf_InsertText (const char *text);
// inserts text at the beginning of the active buffer, ahead of other commands
void Cbuf_InsertTextTo (cmd_buffer_t *buffer, const char *text);
// insert text at the beginning of a particular buffer

void Cbuf_Execute_Sets (void);
// executes all set and setrom commands in the console buffer.  Used early in startup.
void Cbuf_Execute (void);
// Executes all threads in reverse order of creation, all commands from key binds,
// all commands from the console buffer, and all commands stuffed by the server
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!


//===========================================================================

typedef void (*xcommand_t) (void);

typedef enum {
    src_client,			// came in over a net connection as a clc_stringcmd
				// host_client will be valid during this state.
    src_command,		// from a command buffer
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
// as a clc_stringcmd instead of being executed locally

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
// Returns the number of arguments passed to the console command
const char	*Cmd_Argv (int arg);
// Returns the arg-th argument passed to the command, 0 being the name of the command
const char	*Cmd_Args (int start);
// Returns the entire command line starting at the start-th token
const char	*Cmd_Argu (int arg);
// Returns the unprocessed version of an argument
const char	*Cmd_Argsu (int arg);
// Like Cmd_Args, but returns the unprocessed version

// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.


int Cmd_CheckParm (const char *parm);
// Returns the position (1 to argc-1) in the command's argument list
// where the given parameter apears, or 0 if not present

int Cmd_Process (void);
// Processes all parsed tokens according to the type of buffer
// and the settings of each token
void Cmd_TokenizeString (const char *text, qboolean legacy);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void   Cmd_ExecuteParsed (cmd_source_t src);
// Executes a previously tokenized command
int    Cmd_ExecuteString (const char *text, cmd_source_t src);
// Executes a single command in the context of a private buffer
// This allows the rest of QF to access console commands without
// affecting the console buffer


void	Cmd_ForwardToServer (void);
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

void Cmd_StuffCmds_f (void);
// stuffs console commands on the command line into the console buffer

void Cmd_Exec_File (const char *path);
// dumps a file into the console buffer.  Does not execute it as a subroutine
// like the exec command!

extern char	*com_token;
const char *COM_Parse (const char *data);
// This is the legacy token parser.  It exists only to satisfy certain portions
// of the code

void	Cmd_Return (const char *value);
// Returns a value to GIB so that it can be picked up for embedded commands
void	Cmd_Error (const char *message);
// Generates a GIB error
qboolean	Cmd_Restricted (void);
// Returns true if current buffer is restricted

extern struct cvar_s *cmd_warncmd;
// Determines if warnings resulting from console commands are printed

extern cmd_buffer_t *cmd_legacybuffer; // Allow access to the legacy buffer as an alternate console buffer
extern cmd_buffer_t *cmd_keybindbuffer; // Allow access to dedicated key binds command buffer

#endif // __cmd_h
