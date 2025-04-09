/*
	sw_skin.c

	SW Skin support

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/23

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

#include "QF/image.h"

#include "mod_internal.h"

static byte *colormaps[256];

const byte *
sw_Skin_Colormap (const colormap_t *colormap)
{
	byte top = colormap->top & 0x0f;
	byte bot = colormap->bottom & 0x0f;
	int  ind = top | (bot << 4);
	if (colormaps[ind]) {
		return colormaps[ind];
	}
	colormaps[ind] = malloc (VID_GRADES * 256);
	Skin_SetColormap (colormaps[ind], top, bot);
	return colormaps[ind];
}

void
sw_Skin_SetupSkin (skin_t *skin)
{
	skin->tex = Skin_DupTex (skin->tex);
}

void
sw_Skin_Destroy (skin_t *skin)
{
	free (skin->tex);
}
