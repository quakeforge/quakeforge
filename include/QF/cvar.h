/*
	cvar.h

	Configuration variable definitions and prototypes

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

#ifndef __cvar_h
#define __cvar_h

/** \defgroup cvar Configuration variables
	\ingroup utils
*/
//@{

#include "QF/qtypes.h"
#include "QF/quakeio.h"

typedef struct cvar_s {
	const char *name;
	const char *string;
	const char *default_string;
	int	        flags;
	void      (*callback)(struct cvar_s *var);
	const char *description;	// for "help" command
	float       value;
	int         int_val;
	vec3_t      vec;
	struct cvar_s *next;
} cvar_t;

typedef struct cvar_alias_s {
	char       *name;
	cvar_t     *cvar;
	struct cvar_alias_s	*next;
} cvar_alias_t;

#define CVAR_NONE			0
#define	CVAR_ARCHIVE		1		// set to cause it to be saved to vars.rc
									// used for system variables, not for player
									// specific configurations
#define	CVAR_USERINFO		2		// sent to server on connect or change
#define	CVAR_SERVERINFO		4		// sent in response to front end requests
#define	CVAR_NOTIFY			32		// Will notify players when changed.
#define	CVAR_ROM			64		// display only, cannot be set by user at all
#define	CVAR_USER_CREATED	128		// created by a set command
#define CVAR_LATCH			2048	// will change only when C code next does
									// a Cvar_Get(), so it can't be changed

// Zoid| A good CVAR_ROM example is userpath.  The code should read "cvar_t
// *fs_userpath = CvarGet("fs_userpath", ".", CVAR_ROM);  The user can
// override that with +set fs_userpath <blah> since the command line +set gets
// created _before_ the C code for fs_basepath setup is called.  The code goes
// "look, the user made fs_basepath already", uses the users value, but sets
// CVAR_ROM as per the call.


// Returns the Cvar if found, creates it with value if not.  Description and
// flags are always updated.
cvar_t	*Cvar_Get (const char *name, const char *value, int cvarflags,
				   void (*callback)(cvar_t*), const char *description);

cvar_t	*Cvar_FindAlias (const char *alias_name);

cvar_t	*Cvar_MakeAlias (const char *name, cvar_t *cvar);

// equivelants to "<name> <variable>" typed at the console
void 	Cvar_Set (cvar_t *var, const char *value);
void	Cvar_SetValue (cvar_t *var, float value);

// sets a CVAR_ROM variable from within the engine
void	Cvar_SetROM (cvar_t *var, const char *value);

// allows you to change a Cvar's flags without a full Cvar_Get
void	Cvar_SetFlags (cvar_t *var, int cvarflags);

// reset a Cvar to its default setting
void	Cvar_Reset (cvar_t *var);

// returns 0 if not defined or non numeric
float	Cvar_VariableValue (const char *var_name);

// returns an empty string if not defined
const char	*Cvar_VariableString (const char *var_name);

// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits
const char 	*Cvar_CompleteVariable (const char *partial);

// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)
qboolean Cvar_Command (void);

// Writes lines containing "set variable value" for all variables
// with the archive flag set to true.
void 	Cvar_WriteVariables (QFile *f);

// Added by EvilTypeGuy - functions for tab completion system
// Thanks to Fett erich@heintz.com
// Thanks to taniwha
int		Cvar_CompleteCountPossible (const char *partial);
const char	**Cvar_CompleteBuildList (const char *partial);

// Returns a pointer to the Cvar, NULL if not found
cvar_t *Cvar_FindVar (const char *var_name);

void Cvar_Init_Hash (void);
void Cvar_Init (void);

void Cvar_Shutdown (void);

extern cvar_t	*cvar_vars;

//@}

#endif // __cvar_h
