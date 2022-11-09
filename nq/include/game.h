/*
	game.h

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#ifndef __game_h
#define __game_h

#include "QF/qtypes.h"
#include "QF/qdefs.h"

#define MIN_EDICTS		256			// lowest allowed value for max_edicts
#define MAX_EDICTS		32000		// highest allowed value for max_edicts
#define	MAX_DATAGRAM	32000		// max length of unreliable message
#define	MAX_MSGLEN		32000		// max length of a reliable message
#define DATAGRAM_MTU	1400		// actual limit for unreliable messages
									// to nonlocal clients
#define MAX_MODELS		2048
#define MAX_SOUNDS		2048

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#define	MINIMUM_MEMORY			0x550000
#define	MINIMUM_MEMORY_LEVELPAK	(MINIMUM_MEMORY + 0x100000)

#define MAX_NUM_ARGVS	50


#define	ON_EPSILON		0.1			// point on plane side epsilon

#define	SAVEGAME_COMMENT_LENGTH	39

#include "gamedefs.h"

#define	SOUND_CHANNELS		8


extern int			current_skill;		// skill level for currently loaded level (in case
										//  the user changes the cvar while the level is
										//  running, this reflects the level actually in use)

extern qboolean		isDedicated;
extern qboolean		standard_quake;

struct memhunk_s;
void Game_Init (struct memhunk_s *hunk);

#endif // __game_h
