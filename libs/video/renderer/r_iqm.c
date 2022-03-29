/*
	r_iqm.c

	Shared IQM rendering support

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/5/11

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
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/scene/entity.h"

#include "r_internal.h"

float
R_IQMGetLerpedFrames (entity_t *ent, iqm_t *iqm)
{
	animation_t *animation = &ent->animation;
	int         frame = ent->animation.frame;
	float       time, fullinterval;
	iqmanim    *anim;

	if (!iqm->num_anims)
		return R_EntityBlend (animation, 0, 1.0 / 25.0);
	if (frame >= iqm->num_anims || frame < 0) {
		Sys_MaskPrintf (SYS_dev, "R_IQMGetLerpedFrames: no such frame %d\n",
						frame);
		frame = 0;
	}
	anim = &iqm->anims[frame];
	fullinterval = anim->num_frames / anim->framerate;
	time = vr_data.realtime + ent->animation.syncbase;
	time -= ((int) (time / fullinterval)) * fullinterval;
	frame = (int) (time * anim->framerate) + anim->first_frame;
	return R_EntityBlend (animation, frame, 1.0 / anim->framerate);
}

iqmframe_t *
R_IQMBlendFrames (const iqm_t *iqm, int frame1, int frame2, float blend,
				  int extra)
{
	iqmframe_t *frame;
	int         i;

	frame = Hunk_TempAlloc (0, iqm->num_joints * sizeof (iqmframe_t) + extra);
	if (iqm->num_frames) {
#if 0
		for (i = 0; i < iqm->num_joints; i++) {
			iqmframe_t *f1 = &iqm->frames[frame1][i];
			iqmframe_t *f2 = &iqm->frames[frame2][i];
			DualQuatBlend (f1->rt, f2->rt, blend, frame[i].rt);
			QuatBlend (f1->shear, f2->shear, blend, frame[i].shear);
			QuatBlend (f1->scale, f2->scale, blend, frame[i].scale);
		}
#else
		for (i = 0; i < iqm->num_joints; i++) {
			iqmframe_t *f1 = &iqm->frames[frame1][i];
			iqmframe_t *f2 = &iqm->frames[frame2][i];
			iqmjoint   *j = &iqm->joints[i];
			Mat4Blend ((float *) f1, (float *) f2, blend, (float*)&frame[i]);
			if (j->parent >= 0)
				Mat4Mult ((float*)&frame[j->parent],
						  (float*)&frame[i], (float*)&frame[i]);
		}
#endif
	} else {
#if 0
		for (i = 0; i < iqm->num_joints; i++) {
			QuatSet (0, 0, 0, 1, frame[i].rt.q0.q);
			QuatSet (0, 0, 0, 0, frame[i].rt.qe.q);
			QuatSet (0, 0, 0, 0, frame[i].shear);
			QuatSet (1, 1, 1, 0, frame[i].scale);
		}
#else
		for (i = 0; i < iqm->num_joints; i++) {
			Mat4Identity ((float*)&frame[i]);
		}
#endif
	}
	return frame;
}

iqmframe_t *
R_IQMBlendPalette (const iqm_t *iqm, int frame1, int frame2, float blend,
				   int extra, iqmblend_t *blend_palette, int palette_size)
{
	iqmframe_t *frame;
	int         i, j;

	extra += (palette_size - iqm->num_joints) * sizeof (iqmframe_t);
	frame = R_IQMBlendFrames (iqm, frame1, frame2, blend, extra);
	for (i = iqm->num_joints; i < palette_size; i++) {
		vec_t      *mat = (vec_t *) &frame[i];
		iqmblend_t *blend = &blend_palette[i];
		vec_t      *f;

		f = (vec_t *) &frame[blend->indices[0]];
		Mat4Scale (f, blend->weights[0] / 255.0, mat);
		for (j = 1; j < 4; j++) {
			if (!blend->weights[j])
				break;
			f = (vec_t *) &frame[blend->indices[j]];
			Mat4MultAdd (mat, blend->weights[j] / 255.0, f, mat);
		}
	}
	return frame;
}
