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

#include "r_internal.h"

float
R_IQMGetLerpedFrames (entity_t *ent, iqm_t *iqm)
{
	int         frame = ent->frame;
	float       time, fullinterval;
	iqmanim    *anim;

	if (frame >= iqm->num_anims || frame < 0) {
		Sys_MaskPrintf (SYS_DEV, "R_IQMGetLerpedFrames: no such frame %d\n",
						frame);
		frame = 0;
	}
	anim = &iqm->anims[frame];
	fullinterval = anim->num_frames / anim->framerate;
	time = vr_data.realtime + currententity->syncbase;
	time -= ((int) (time / fullinterval)) * fullinterval;
	frame = (int) (time * anim->framerate) + anim->first_frame;
	return R_EntityBlend (ent, frame, anim->framerate);
}

iqmframe_t *
R_IQMBlendFrames (const iqm_t *iqm, int frame1, int frame2, float blend,
				  int extra)
{
	iqmframe_t *frame;
	int         i;

	frame = Hunk_TempAlloc (iqm->num_joints * sizeof (iqmframe_t) + extra);
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
	return frame;
}
