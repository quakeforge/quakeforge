/*
	sw_model_sprite.c

	model loading and caching

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

// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "d_iface.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "server.h"

extern model_t *loadmodel;
extern char loadname[];

/*
	Mod_LoadSpriteFrame
*/
void       *
Mod_LoadSpriteFrame (void *pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t *pinframe;
	mspriteframe_t *pspriteframe;
	int         i, width, height, size, origin[2];
	unsigned short *ppixout;
	byte       *ppixin;

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t) + size * r_pixbytes,

								   loadname);

	memset (pspriteframe, 0, sizeof (mspriteframe_t) + size);

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	if (r_pixbytes == 1) {
		memcpy (&pspriteframe->pixels[0], (byte *) (pinframe + 1), size);
	} else if (r_pixbytes == 2) {
		ppixin = (byte *) (pinframe + 1);
		ppixout = (unsigned short *) &pspriteframe->pixels[0];

		for (i = 0; i < size; i++)
			ppixout[i] = d_8to16table[ppixin[i]];
	} else {
		SV_Error ("Mod_LoadSpriteFrame: driver set invalid r_pixbytes: %d\n",
				  r_pixbytes);
	}

	return (void *) ((byte *) pinframe + sizeof (dspriteframe_t) + size);
}
