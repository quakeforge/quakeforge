/*
	r_sprite.c

	Draw Sprite Model

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

#include "QF/sys.h"
#include "QF/scene/entity.h"

#include "r_internal.h"

mspriteframe_t *
R_GetSpriteFrame (const msprite_t *sprite, const animation_t *animation)
{
	mspritegroup_t *group;
	mspriteframe_t *frame;
	int         i, numframes, frame_num;
	float      *intervals, fullinterval, targettime, time;

	frame_num = animation->frame;

	if ((frame_num >= sprite->numframes) || (frame_num < 0)) {
		Sys_Printf ("R_DrawSprite: no such frame %d\n", frame_num);
		frame_num = 0;
	}

	if (sprite->frames[frame_num].type == SPR_SINGLE) {
		frame = sprite->frames[frame_num].frame;
	} else {
		group = sprite->frames[frame_num].group;
		intervals = group->intervals;
		numframes = group->numframes;
		fullinterval = intervals[numframes - 1];

		time = vr_data.realtime + animation->syncbase;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval
		// values are positive, so we don't have to worry about division by 0
		targettime = time - ((int) (time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++) {
			if (intervals[i] > targettime)
				break;
		}

		frame = group->frames[i];
	}

	return frame;
}
