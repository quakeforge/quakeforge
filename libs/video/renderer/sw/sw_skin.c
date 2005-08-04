/*
	sw_skin.c

	(description)

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/model.h"
#include "QF/render.h"
#include "QF/skin.h"

#include "compat.h"


void
Skin_Set_Translate (int top, int bottom, void *_dest)
{
	int         i, j;
	byte       *source;

	byte *dest = (byte *) _dest;

	top = bound (0, top, 13) * 16;
	bottom = bound (0, bottom, 13) * 16;

	source = vid.colormap8;
	memcpy (dest, vid.colormap8, VID_GRADES*256);

	for (i = 0; i < VID_GRADES; i++, dest += 256, source += 256) {
		if (top < 128)				// the artists made some backwards
									// ranges.  sigh.
			memcpy (dest + TOP_RANGE, source + top, 16);
		else
			for (j = 0; j < 16; j++)
				dest[TOP_RANGE + j] = source[top + 15 - j];

		if (bottom < 128)
			memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
		else
			for (j = 0; j < 16; j++)
				dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
	}
}


void
Skin_Do_Translation (skin_t *player_skin, int slot, skin_t *skin)
{
}


void
Skin_Do_Translation_Model (model_t *model, int skinnum, int slot, skin_t *skin)
{
}


void
Skin_Player_Model (model_t *model)
{
}


void
Skin_Init_Translation (void)
{
}


void
Skin_Process (skin_t *skin, struct tex_s *tex)
{
}
