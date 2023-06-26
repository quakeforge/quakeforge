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

*/

#ifndef __QF_cvar_h
#define __QF_cvar_h

/** \defgroup cvar Configuration variables
	\ingroup utils
*/
///@{

#include "QF/cexpr.h"
#include "QF/listener.h"
#include "QF/qtypes.h"
#include "QF/quakeio.h"

typedef struct cvar_s {
	const char *name;
	const char *description;
	const char *default_value;
	unsigned    flags;
	exprval_t   value;
	int       (*validator) (const struct cvar_s *var);
	struct cvar_listener_set_s *listeners;
} cvar_t;

typedef struct cvar_listener_set_s LISTENER_SET_TYPE (cvar_t)
	cvar_listener_set_t;
typedef void (*cvar_listener_t) (void *data, const cvar_t *cvar);

typedef int (*cvar_select_t) (const cvar_t *cvar, void *data);

typedef struct cvar_alias_s {
	char       *name;			///< The name of the alias.
	cvar_t     *cvar;			///< The cvar to which this alias refers
} cvar_alias_t;

/** \name cvar_flags
	Zoid| A good CVAR_ROM example is userpath.  The code should read "cvar_t
	*fs_userpath = CvarGet("fs_userpath", ".", CVAR_ROM);  The user can
	override that with +set fs_userpath \<blah\> since the command line +set
	gets created _before_ the C code for fs_basepath setup is called.  The
	code goes "look, the user made fs_basepath already", uses the users value,
	but sets CVAR_ROM as per the call.
*/
///@{
#define CVAR_NONE			0		///< normal cvar
#define	CVAR_ARCHIVE		1		///< set to cause it to be saved to
									///< config.cfg
#define	CVAR_USERINFO		2		///< sent to server on connect or change
#define	CVAR_SERVERINFO		4		///< sent in response to front end requests
#define	CVAR_NOTIFY			32		///< Will notify players when changed.
									///< (not implemented)
#define	CVAR_ROM			64		///< display only, cannot be set
#define	CVAR_USER_CREATED	128		///< created by a set command
#define CVAR_REGISTERED     256		///< var has been registered
#define CVAR_LATCH			2048	///< will change only when C code next does
									///< a Cvar_Get(), so it can't be changed
									///< (not implemented)
///@}


void Cvar_Register (cvar_t *var, cvar_listener_t listener, void *data);

cvar_t	*Cvar_FindAlias (const char *alias_name);

cvar_t	*Cvar_MakeAlias (const char *name, cvar_t *cvar);
cvar_t	*Cvar_RemoveAlias (const char *name);
void Cvar_AddListener (cvar_t *cvar, cvar_listener_t listener, void *data);
void Cvar_RemoveListener (cvar_t *cvar, cvar_listener_t listener, void *data);

// equivelants to "<name> <variable>" typed at the console
void Cvar_Set (const char *var, const char *value);
void Cvar_SetVar (cvar_t *var, const char *value);

// allows you to change a Cvar's flags without a full Cvar_Get
void	Cvar_SetFlags (cvar_t *var, int cvarflags);

// returns 0 if not defined or non numeric
float	Cvar_Value (const char *var_name);

// returns an empty string if not defined
const char *Cvar_String (const char *var_name);
const char *Cvar_VarString (const cvar_t *var);

// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)
bool Cvar_Command (void);

// Writes lines containing "set variable value" for all variables
// with the archive flag set to true.
void 	Cvar_WriteVariables (QFile *f);

struct plitem_s;
void Cvar_SaveConfig (struct plitem_s *config);
void Cvar_LoadConfig (struct plitem_s *config);

// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits
const char 	*Cvar_CompleteVariable (const char *partial) __attribute__((pure));

// Added by EvilTypeGuy - functions for tab completion system
// Thanks to Fett erich@heintz.com
// Thanks to taniwha
int		Cvar_CompleteCountPossible (const char *partial) __attribute__((pure));
const char	**Cvar_CompleteBuildList (const char *partial);

// Returns a pointer to the Cvar, NULL if not found
cvar_t *Cvar_FindVar (const char *var_name);

const cvar_t **Cvar_Select (cvar_select_t select, void *data);

void Cvar_Init_Hash (void);
void Cvar_Init (void);

///@}

#endif//__QF_cvar_h
