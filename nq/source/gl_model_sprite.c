/*
	gl_model_sprite.c

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "r_local.h"
#include "QF/sys.h"
#include "QF/compat.h"
#include "QF/console.h"
#include "QF/qendian.h"
#include "QF/checksum.h"
#include "glquake.h"

extern char loadname[];
extern model_t *loadmodel;

/*
=================
Mod_LoadSpriteFrame
=================
*/
void       *
Mod_LoadSpriteFrame (void *pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t *pinframe;
	mspriteframe_t *pspriteframe;
	int         width, height, size, origin[2];
	char        name[64];

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t), loadname);

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	snprintf (name, sizeof (name), "%s_%i", loadmodel->name, framenum);
	pspriteframe->gl_texturenum =
		GL_LoadTexture (name, width, height, (byte *) (pinframe + 1), true,
						true, 1);

	return (void *) ((byte *) pinframe + sizeof (dspriteframe_t) + size);
}
