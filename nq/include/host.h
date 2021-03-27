/*
	host.h

	@description@

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

#ifndef __host_h
#define __host_h

#include "QF/qtypes.h"

extern qboolean noclip_anglehack;

typedef struct
{
	int		argc;
	const char	**argv;
} quakeparms_t;

extern	quakeparms_t host_parms;

extern	struct cvar_s	*sys_ticrate;
extern	struct cvar_s	*sys_nostdout;
extern	struct cvar_s	*developer;

extern	struct cvar_s	*pausable;

extern int viewentity;

extern	qboolean	host_initialized;		// true if into command execution
extern	double		host_frametime;
extern	int			host_framecount;	// incremented every frame, never reset
extern	int			host_in_game;		// input focus goes to the game
extern	double		realtime;			// not bounded in any way, changed at
										// start of every frame, never reset

extern struct cbuf_s *host_cbuf;

void Host_ClearMemory (void);
void Host_ServerFrame (void);
void Host_InitCommands (void);
void Host_Init (void);
void Host_Shutdown(void *data);
void Host_Error (const char *error, ...) __attribute__((format(PRINTF,1,2), noreturn));
void Host_EndGame (const char *message, ...) __attribute__((format(PRINTF,1,2), noreturn));
void Host_Frame (float time);
void Host_Quit_f (void) __attribute__((noreturn));
void Host_ClientCommands (const char *fmt, ...) __attribute__((format(PRINTF,1,2)));
void Host_ShutdownServer (qboolean crash);

#endif // __host_h
