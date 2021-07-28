/*
	model_sprite.c

	sprite model loading and caching

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/qendian.h"
#include "QF/sys.h"

#include "compat.h"
#include "mod_internal.h"

static void *
Mod_LoadSpriteFrame (model_t *mod, void *pin, mspriteframe_t **ppframe,
					 int framenum)
{
	dspriteframe_t *pinframe;
	int				width, height, size, origin[2];
	mspriteframe_t *pspriteframe;

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = Hunk_AllocName (0, sizeof (mspriteframe_t) + size,
								   mod->name);

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

	memcpy (pspriteframe->pixels, (byte *) (pinframe + 1), size);

	m_funcs->Mod_SpriteLoadTexture (mod, pspriteframe, framenum);

	return (void *) ((byte *) pinframe + sizeof (dspriteframe_t) + size);
}

static void *
Mod_LoadSpriteGroup (model_t *mod, void *pin, mspriteframe_t **ppframe,
					 int framenum)
{
	dspritegroup_t		*pingroup;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	int					 numframes, i;
	mspritegroup_t		*pspritegroup;
	void				*ptemp;

	pingroup = (dspritegroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Hunk_AllocName (0, field_offset (mspritegroup_t,
													frames[numframes]),
								   mod->name);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *) pspritegroup;

	pin_intervals = (dspriteinterval_t *) (pingroup + 1);

	poutintervals = Hunk_AllocName (0, numframes * sizeof (float), mod->name);

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++) {
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error ("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++) {
		ptemp =
			Mod_LoadSpriteFrame (mod, ptemp, &pspritegroup->frames[i],
								 framenum * 100 + i);
	}

	return ptemp;
}

void
Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	dsprite_t			*pin;
	dspriteframetype_t	*pframetype;
	int					 numframes, size, version, i;
	msprite_t			*psprite;

	pin = (dsprite_t *) buffer;

	version = LittleLong (pin->version);
	if (version != SPR_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				   mod->path, version, SPR_VERSION);

	numframes = LittleLong (pin->numframes);

	size = field_offset (msprite_t, frames[numframes]);

	psprite = Hunk_AllocName (0, size, mod->name);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
	mod->mins[2] = -psprite->maxheight / 2;
	mod->maxs[2] = psprite->maxheight / 2;

	// load the frames
	if (numframes < 1)
		Sys_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *) (pin + 1);

	for (i = 0; i < numframes; i++) {
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE) {
			pframetype = (dspriteframetype_t *)
				Mod_LoadSpriteFrame (mod, pframetype + 1,
									 &psprite->frames[i].frameptr, i);
		} else {
			pframetype = (dspriteframetype_t *)
				Mod_LoadSpriteGroup (mod, pframetype + 1,
									 &psprite->frames[i].frameptr, i);
		}
	}

	mod->type = mod_sprite;
}
