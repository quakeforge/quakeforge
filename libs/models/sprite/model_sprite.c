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

#include "mod_internal.h"
#include "qfalloca.h"

void
Mod_LoadSpriteFrame (mspriteframe_t *frame, const dspriteframe_t *dframe)
{
	frame->width = dframe->width;
	frame->height = dframe->height;

	frame->up = dframe->origin[1];
	frame->down = dframe->origin[1] - dframe->height;
	frame->left = dframe->origin[0];
	frame->right = dframe->width + dframe->origin[0];
}

static void *
skip_frame (dspriteframe_t *frame)
{
	__auto_type pixels = (byte *) (frame + 1);
	return pixels + frame->width * frame->height;
}

static void *
swap_frame (dspriteframe_t *frame)
{
	for (int i = 0; i < 2; i++) {
		frame->origin[i] = LittleLong (frame->origin[i]);
	}
	frame->width = LittleLong (frame->width);
	frame->height = LittleLong (frame->height);
	return skip_frame (frame);
}

static void *
swap_group (dspritegroup_t *group)
{
	group->numframes = LittleLong (group->numframes);
	__auto_type interval = (dspriteinterval_t *) (group + 1);
	for (int i = 0; i < group->numframes; i++) {
		interval->interval = LittleFloat (interval->interval);
		interval++;
	}
	__auto_type frame = (dspriteframe_t *) interval;
	for (int i = 0; i < group->numframes; i++) {
		frame = swap_frame (frame);
	}
	return frame + 1;
}

static int
swap_sprite (dsprite_t *sprite)
{
	sprite->ident = LittleLong (sprite->ident);
	sprite->version = LittleLong (sprite->version);
	sprite->type = LittleLong (sprite->type);
	sprite->boundingradius = LittleFloat (sprite->boundingradius);
	sprite->width = LittleLong (sprite->width);
	sprite->height = LittleLong (sprite->height);
	sprite->numframes = LittleLong (sprite->numframes);
	sprite->beamlength = LittleFloat (sprite->beamlength);
	sprite->synctype = LittleLong (sprite->synctype);

	int         numframes = 0;
	__auto_type type = (dspriteframetype_t *) (sprite + 1);
	for (int i = 0; i < sprite->numframes; i++) {
		type->type = LittleLong (type->type);
		if (type->type == SPR_SINGLE) {
			__auto_type frame = (dspriteframe_t *) (type + 1);
			type = swap_frame (frame);
			numframes += 1;
		} else {
			__auto_type group = (dspritegroup_t *) (type + 1);
			type = swap_group (group);
			numframes += group->numframes;
		}
	}
	return numframes;
}

static void *
find_group_frames (mspritegroup_t **group, dspritegroup_t *dgroup,
				   mspriteframe_t ***frames, dspriteframe_t **dframes,
				   int *frame_numbers, const char *modname)
{
	int         numframes = dgroup->numframes;
	size_t      size = field_offset (mspritegroup_t, frames[numframes]);
	*group = Hunk_AllocName (0, size, modname);
	(*group)->numframes = numframes;
	(*group)->intervals = Hunk_AllocName (0, numframes * sizeof (float),
										  modname);

	__auto_type interval = (dspriteinterval_t *) (dgroup + 1);
	for (int i = 0; i < numframes; i++) {
		(*group)->intervals[i] = interval->interval;
		interval++;
	}
	__auto_type dframe = (dspriteframe_t *) interval;
	for (int i = 0; i < numframes; i++) {
		frames[i] = &(*group)->frames[i];
		dframes[i] = dframe;
		frame_numbers[i] = i;
		dframe = skip_frame (dframe);
	}
	return dframe;
}

static void
find_frames (msprite_t *sprite, dsprite_t *dsprite,
			 mspriteframe_t ***frames, dspriteframe_t **dframes,
			 int *frame_numbers, const char *modname)
{
	int         frame_index = 0;
	__auto_type type = (dspriteframetype_t *) (dsprite + 1);
	for (int i = 0; i < dsprite->numframes; i++) {
		sprite->frames[i].type = type->type;
		if (type->type == SPR_SINGLE) {
			__auto_type frame = (dspriteframe_t *) (type + 1);
			dframes[frame_index] = frame;
			frames[frame_index] = &sprite->frames[i].frame;
			frame_numbers[frame_index] = i;
			frame_index += 1;
			type = skip_frame (frame);
		} else {
			__auto_type group = (dspritegroup_t *) (type + 1);
			type = find_group_frames (&sprite->frames[i].group, group,
									  frames + frame_index,
									  dframes + frame_index,
									  frame_numbers + frame_index,
									  modname);
			for (int j = 0; j < group->numframes; j++) {
				frame_numbers[frame_index + j] += i * 100;
			}
			frame_index += group->numframes;
		}
	}
}

void
Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	__auto_type dsprite = (dsprite_t *) buffer;
	msprite_t  *sprite;

	if (LittleLong (dsprite->version) != SPR_VERSION) {
		Sys_Error ("%s has wrong version number (%i should be %i)",
				   mod->path, LittleLong (dsprite->version), SPR_VERSION);
	}

	// total number of frames (direct and in groups)
	int         numframes = swap_sprite (dsprite);

	if (numframes < 1) {
		Sys_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d", numframes);
	}

	sprite = Hunk_AllocName (0, field_offset (msprite_t,
											  frames[dsprite->numframes]),
							 mod->name);
	sprite->type = dsprite->type;
	sprite->beamlength = dsprite->beamlength;
	sprite->numframes = dsprite->numframes;
	sprite->data = 0;

	mod->cache.data = sprite;
	mod->mins[0] = mod->mins[1] = -dsprite->width / 2;
	mod->maxs[0] = mod->maxs[1] = dsprite->width / 2;
	mod->mins[2] = -dsprite->height / 2;
	mod->maxs[2] = dsprite->height / 2;
	mod->numframes = dsprite->numframes;
	mod->type = mod_sprite;

	mod_sprite_ctx_t sprite_ctx = {
		.mod = mod,
		.dsprite = dsprite,
		.sprite = sprite,
		.numframes = numframes,
		.frame_numbers = alloca (numframes * sizeof (int)),
		.dframes = alloca (numframes * sizeof (dspriteframe_t *)),
		.frames = alloca (numframes * sizeof (mspriteframe_t **)),
	};
	find_frames (sprite, dsprite, sprite_ctx.frames, sprite_ctx.dframes,
				 sprite_ctx.frame_numbers, mod->name);
	m_funcs->Mod_SpriteLoadFrames (&sprite_ctx);
}
