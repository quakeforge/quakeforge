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

#include "QF/cvar.h"
#include "QF/model.h"
#include "QF/render.h"
#include "QF/skin.h"

void
Mod_LoadLighting (bsp_t *bsp)
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
Mod_LoadExternalSkins (model_t *mod)
{
}

viddef_t    vid;

VISIBLE void
Skin_InitTranslations (void)
{
}

VISIBLE void
Skin_ProcessTranslation (int cmap, const byte *translation)
{
}

VISIBLE void
Skin_SetupSkin (skin_t *skin, int cmap)
{
}
