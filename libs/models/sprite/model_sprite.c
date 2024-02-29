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
	frame->up = dframe->origin[1];
	frame->down = dframe->origin[1] - dframe->height;
	frame->left = dframe->origin[0];
	frame->right = dframe->width + dframe->origin[0];
}

static void *
skip_frame (dspriteframe_t *frame)
{
	auto pixels = (byte *) (frame + 1);
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
	auto interval = (dspriteinterval_t *) (group + 1);
	for (int i = 0; i < group->numframes; i++) {
		interval->interval = LittleFloat (interval->interval);
		if (interval->interval <= 0) {
			interval->interval = 0.1;
		}
		if (i) {
			// prefix sum
			interval->interval += interval[i - 1].interval;
		}
		interval++;
	}
	auto frame = (dspriteframe_t *) interval;
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
	auto type = (dspriteframetype_t *) (sprite + 1);
	for (int i = 0; i < sprite->numframes; i++) {
		type->type = LittleLong (type->type);
		if (type->type == SPR_SINGLE) {
			auto frame = (dspriteframe_t *) (type + 1);
			type = swap_frame (frame);
			numframes += 1;
		} else {
			auto group = (dspritegroup_t *) (type + 1);
			type = swap_group (group);
			numframes += group->numframes;
		}
	}
	return numframes;
}

static void
find_frames (mod_sprite_ctx_t *sprite_ctx, dsprite_t *dsprite)
{
	auto sprite = sprite_ctx->sprite;
	auto desc = (framedesc_t *) ((byte *) sprite + sprite->skin.descriptors);
	auto frame = (frame_t *) ((byte *) sprite + sprite->skin.frames);

	int frame_index = 0;
	auto type = (dspriteframetype_t *) (dsprite + 1);
	for (int i = 0; i < dsprite->numframes; i++) {
		if (type->type == SPR_SINGLE) {
			desc[i] = (framedesc_t) {
				.firstframe = frame_index,
				.numframes = 1,
			};
			auto dframe = (dspriteframe_t *) (type + 1);

			sprite_ctx->dframes[frame_index] = dframe;
			sprite_ctx->frames[frame_index] = &frame[frame_index];
			frame[frame_index] = (frame_t) { };
			frame_index += 1;
			type = skip_frame (dframe);
		} else {
			auto group = (dspritegroup_t *) (type + 1);
			desc[i] = (framedesc_t) {
				.firstframe = frame_index,
				.numframes = group->numframes,
			};
			auto intervals = (dspriteinterval_t *) &group[1];
			void *data = &intervals[group->numframes];
			for (int j = 0; j < group->numframes; j++) {
				sprite_ctx->dframes[frame_index] = data;
				sprite_ctx->frames[frame_index] = &frame[frame_index];
				frame[frame_index++] = (frame_t) {
					.endtime = intervals[j].interval,
				};
				data = skip_frame (data);
			}
		}
	}
}

void
Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	auto dsprite = (dsprite_t *) buffer;
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

	size_t      size = sizeof (msprite_t)
						+ sizeof (framedesc_t[dsprite->numframes])
						+ sizeof (frame_t[numframes]);
	sprite = Hunk_AllocName (0, size, mod->name);
	auto descriptors = (framedesc_t *) &sprite[1];
	auto frames = (frame_t *) &descriptors[dsprite->numframes];
	*sprite = (msprite_t) {
		.type = dsprite->type,
		.beamlength = dsprite->beamlength,
		.skin = {
			.numdesc = dsprite->numframes,
			.descriptors = (byte *) descriptors - (byte *) sprite,
			.frames = (byte *) frames - (byte *) sprite,
		},
	};

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
		.dframes = alloca (numframes * sizeof (dspriteframe_t *)),
		.frames = alloca (numframes * sizeof (mspriteframe_t **)),
	};
	find_frames (&sprite_ctx, dsprite);
	m_funcs->Mod_SpriteLoadFrames (&sprite_ctx);
}
