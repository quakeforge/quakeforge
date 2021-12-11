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
Mod_LoadSpriteFrame (model_t *mod, void *pin, mspriteframe_t **frame,
					 int framenum)
{
	dspriteframe_t *in_frame;
	int				width, height, size, origin[2];

	in_frame = (dspriteframe_t *) pin;

	width = LittleLong (in_frame->width);
	height = LittleLong (in_frame->height);
	size = width * height;

	*frame = Hunk_AllocName (0, sizeof (mspriteframe_t) + size, mod->name);

	(*frame)->width = width;
	(*frame)->height = height;
	origin[0] = LittleLong (in_frame->origin[0]);
	origin[1] = LittleLong (in_frame->origin[1]);

	(*frame)->up = origin[1];
	(*frame)->down = origin[1] - height;
	(*frame)->left = origin[0];
	(*frame)->right = width + origin[0];

	memcpy ((*frame)->pixels, (byte *) (in_frame + 1), size);

	m_funcs->Mod_SpriteLoadTexture (mod, *frame, framenum);

	return (void *) ((byte *) in_frame + sizeof (dspriteframe_t) + size);
}

static void *
Mod_LoadSpriteGroup (model_t *mod, void *pin, mspritegroup_t **group,
					 int framenum)
{
	dspritegroup_t		*in_group;
	dspriteinterval_t	*in_intervals;
	float				*intervals;
	int					 numframes, i;
	void				*temp;

	in_group = (dspritegroup_t *) pin;

	numframes = LittleLong (in_group->numframes);

	int         size = field_offset (mspritegroup_t, frames[numframes]);
	*group = Hunk_AllocName (0, size, mod->name);

	(*group)->numframes = numframes;

	in_intervals = (dspriteinterval_t *) (in_group + 1);

	intervals = Hunk_AllocName (0, numframes * sizeof (float), mod->name);

	(*group)->intervals = intervals;

	for (i = 0; i < numframes; i++) {
		*intervals = LittleFloat (in_intervals->interval);
		if (*intervals <= 0.0)
			Sys_Error ("Mod_LoadSpriteGroup: interval<=0");

		intervals++;
		in_intervals++;
	}

	temp = (void *) in_intervals;

	for (i = 0; i < numframes; i++) {
		temp = Mod_LoadSpriteFrame (mod, temp, &(*group)->frames[i],
									framenum * 100 + i);
	}

	return temp;
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
									 &psprite->frames[i].frame, i);
		} else {
			pframetype = (dspriteframetype_t *)
				Mod_LoadSpriteGroup (mod, pframetype + 1,
									 &psprite->frames[i].group, i);
		}
	}

	mod->type = mod_sprite;
}
