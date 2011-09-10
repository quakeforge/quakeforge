/*
	game.c

	game specific support (notably hipnotic, rogue and abyss)

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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "QF/qargs.h"
#include "QF/quakefs.h"

#include "game.h"

qboolean standard_quake = false;

const char *
Game_Init (void)
{
	int     i;

	// FIXME: make this dependant on QF metadata in the mission packs
	standard_quake = true;

	if ((i = COM_CheckParm ("-hipnotic"))) {
		standard_quake = false;
		return "hipnotic";
	} else if ((i = COM_CheckParm ("-rogue"))) {
		standard_quake = false;
		return "rogue";
	} else if ((i = COM_CheckParm ("-abyss"))) {
		return "abyss";
	}

	return "nq";
}
