/*
	console.h

	Console definitions and prototypes

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

#ifndef __console_h
#define __console_h
//
// console
//

#include "QF/qtypes.h"
#include "QF/gcc_attr.h"

#define		CON_TEXTSIZE	32764
typedef struct
{
	char	text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line
	int		numlines;		// number of non-blank text lines, used for backscroling
} console_t;

extern	console_t	con_main;
extern	console_t	con_chat;
extern	console_t	*con;			// point to either con_main or con_chat

extern	int			con_ormask;

extern int con_totallines;
extern qboolean con_initialized;
extern byte *con_chars;
extern	int	con_notifylines;		// scan lines to clear for notify lines

void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_Init (void);
void Con_Shutdown (void);
void Con_Init_Cvars (void);
void Con_ProcessInput (void);
void Con_DrawConsole (int lines);
void Con_DrawDownload (int lines);
void Con_Print (const char *txt);
void Con_Printf (const char *fmt, ...) __attribute__((format(printf,1,2)));
void Con_DPrintf (const char *fmt, ...) __attribute__((format(printf,1,2)));
void Con_Clear_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);

// wrapper function to attempt to either complete the command line
// or to list possible matches grouped by type
// (i.e. will display possible variables, aliases, commands
// that match what they've typed so far)
void Con_CompleteCommandLine(void);

// Generic libs/util/console.c function to display a list
// formatted in columns on the console
void Con_DisplayList(const char **list, int con_linewidth);

extern struct cvar_s *developer;

#endif // __console_h
