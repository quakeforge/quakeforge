/*
	sv_model.c

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
static const char rcsid[] = 
	"$Id$";


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "QF/model.h"

const int   mod_lightmap_bytes = 1;
cvar_t     *gl_sky_divide;	// not used but needed for linking

void Mod_LoadBrushModel (model_t *mod, void *buffer);


void
Mod_LoadLighting (lump_t *l)
{
}

void
Mod_LoadAliasModel (model_t *mod, void *buf)
{
	//Mod_LoadBrushModel (mod, buf);
}

void
Mod_LoadSpriteModel (model_t *mod, void *buf)
{
	//Mod_LoadBrushModel (mod, buf);
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
GL_SubdivideSurface (msurface_t *fa)
{
}
