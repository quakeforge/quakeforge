/*
	commdef.h

	Definitions common to client and server.

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000  Marcus Sundberg <mackan@stacken.kth.se>

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

#ifndef _COMMDEF_H
#define _COMMDEF_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/gcc_attr.h"
#include "QF/qtypes.h"

/* The host system specifies the base of the directory tree, the
   command line parms passed to the program, and the amount of memory
   available for the program to use.
*/

typedef struct
{
	int		argc;
	char	**argv;
	void	*membase;
	int		memsize;
} quakeparms_t;

/* Host */
extern	quakeparms_t host_parms;

extern	struct cvar_s	*sys_nostdout;
extern	struct cvar_s	*developer;

extern	qboolean	host_initialized;	/* True if into command execution. */
//extern	double		host_frametime;
extern	double		realtime;			/* Not bounded in any way, changed at
										   start of every frame, never reset */

#endif // _COMMDEF_H
