/*
	skin.h

	Client skin definitions

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

#ifndef __QF_skin_h
#define __QF_skin_h

#include "QF/qtypes.h"
#include "QF/vid.h"
#include "QF/zone.h"

#define MAX_CACHED_SKINS 128
#define MAX_SKIN_LENGTH	32

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

#define RSSHOT_WIDTH 320
#define RSSHOT_HEIGHT 200

// if more than 32 clients are to be supported, then this will need to be
// updated
#define MAX_TRANSLATIONS 32

#define PLAYER_WIDTH 296
#define PLAYER_HEIGHT 194

typedef struct skin_s {
	const char *name;
	bool		valid;		// the skin was found
	struct tex_s *texels;
	byte       *colormap;
	int         texnum;
	int         auxtex;
} skin_t;

#endif//__QF_skin_h
