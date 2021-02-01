/*
	sw_model_brush.c

	sw renderer support routines for model loading and caching

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

#include "mod_internal.h"

void
sw_Mod_LoadLighting (model_t *mod, bsp_t *bsp)
{
	mod_lightmap_bytes = 1;
	if (!bsp->lightdatasize) {
		mod->brush.lightdata = NULL;
		return;
	}
	mod->brush.lightdata = Hunk_AllocName (bsp->lightdatasize, mod->name);
	memcpy (mod->brush.lightdata, bsp->lightdata, bsp->lightdatasize);
}
