/*
	null_model.c

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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#include "QF/cvar.h"
#include "QF/model.h"
#include "QF/render.h"

int         mod_lightmap_bytes = 1;

void
Mod_LoadLighting (lump_t *l)
{
}

void
Mod_LoadAliasModel (model_t *mod, void *buf, cache_allocator_t allocator)
{
}

void
Mod_LoadSpriteModel (model_t *mod, void *buf)
{
}

void
R_InitSky (struct texture_s *mt)
{
}

void
Mod_ProcessTexture (miptex_t *mx, texture_t *tx)
{
}

void
Mod_LoadExternalSkins (model_t *mod)
{
}

void
Mod_LoadExternalTextures (model_t *mod)
{
}

void
Mod_SubdivideSurface (msurface_t *fa)
{
}
