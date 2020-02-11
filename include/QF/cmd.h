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

*/

#ifndef __QF_cmd_h
#define __QF_cmd_h

/** \defgroup cmd Command management.
	\ingroup utils
*/
///@{

#include "QF/qtypes.h"
#include "QF/cbuf.h"

typedef void (*xcommand_t) (void);

typedef enum {
    src_client,			// came in over a net connection as a clc_stringcmd
						// host_client will be valid during this state.
    src_command,		// from a command buffer
} cmd_source_t;

typedef struct cmd_function_s {
	struct cmd_function_s *next;
	const char *name;
	xcommand_t  function;
	const char *description;
} cmd_function_t;

extern cmd_source_t	cmd_source;

void	Cmd_Init_Hash (void);
void	Cmd_Init (void);

int		Cmd_AddCommand (const char *cmd_name, xcommand_t function, const char *description);
int		Cmd_RemoveCommand (const char *cmd_name);

qboolean Cmd_Exists (const char *cmd_name);
const char 	*Cmd_CompleteCommand (const char *partial) __attribute__((pure));
int		Cmd_CompleteCountPossible (const char *partial) __attribute__((pure));
const char	**Cmd_CompleteBuildList (const char *partial);


int Cmd_Argc (void) __attribute__((pure));
const char *Cmd_Argv (int arg) __attribute__((pure));
const char *Cmd_Args (int start) __attribute__((pure));
struct cbuf_args_s;
int Cmd_Command (struct cbuf_args_s *args);
int Cmd_ExecuteString (const char *text, cmd_source_t src);
struct cbuf_s;
void Cmd_StuffCmds (struct cbuf_s *cbuf);
void Cmd_Exec_File (struct cbuf_s *cbuf, const char *path, int qfs);
void Cmd_AddProvider(const char *name, struct cbuf_interpreter_s *interp);
struct cbuf_interpreter_s *Cmd_GetProvider(const char *name);


extern struct cbuf_args_s *cmd_args;
extern struct cvar_s *cmd_warncmd;

///@}

#endif//__QF_cmd_h
