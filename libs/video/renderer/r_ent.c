/*
	r_tent.c

	rendering entities

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

#include <math.h>
#include <stdlib.h>

#include "QF/model.h"
#include "QF/msg.h"
#include "QF/render.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "r_internal.h"

entqueue_t *r_ent_queue;

float
R_EntityBlend (animation_t *animation, int pose, float interval)
{
	float       blend;

	if (animation->nolerp) {
		animation->nolerp = 0;
		animation->pose1 = pose;
		animation->pose2 = pose;
		return 0.0;
	}
	animation->frame_interval = interval;
	if (animation->pose2 != pose) {
		animation->frame_start_time = vr_data.realtime;
		if (animation->pose2 == -1) {
			animation->pose1 = pose;
		} else {
			animation->pose1 = animation->pose2;
		}
		animation->pose2 = pose;
		blend = 0.0;
	} else if (vr_data.paused) {
		blend = 1.0;
	} else {
		blend = (vr_data.realtime - animation->frame_start_time)
				/ animation->frame_interval;
		blend = min (blend, 1.0);
	}
	return blend;
}
