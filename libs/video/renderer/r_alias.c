/*
	r_alias.c

	Draw Alias Model

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

float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

uint32_t
R_GetFrameData (const manim_t *anim, int framenum, const void *base,
				float syncbase)
{
	if ((framenum >= anim->numdesc) || (framenum < 0)) {
		Sys_MaskPrintf (SYS_dev, "R_GetFrame: no such frame # %d\n",
						framenum);
		return 0;
	}

	auto fdesc = (mframedesc_t *) (base + anim->descriptors);
	fdesc += framenum;
	if (fdesc->numframes < 1) {
		return 0;
	}

	auto frame = (mframe_t *) (base + anim->frames);
	frame += fdesc->firstframe;

	int   numframes = fdesc->numframes;
	float fullinterval = frame[numframes - 1].endtime;
	float time = vr_data.realtime + syncbase;
	// when loading in Mod_LoadAliasSkinGroup, we guaranteed all endtime
	// values are positive, so we don't have to worry about division by 0
	float target = time - ((int) (time / fullinterval)) * fullinterval;

	for (int i = 0; i < (numframes - 1); i++, frame++) {
		if (frame->endtime > target) {
			break;
		}
	}

	return frame->data;
}

uint32_t
R_AliasGetSkindesc (animation_t *animation, int skinnum, malias_t *alias)
{
	float syncbase = animation->syncbase;
	return R_GetFrameData (&alias->skin, skinnum, alias, syncbase);
}

maliasframe_t *
R_AliasGetFramedesc (animation_t *animation, malias_t *alias)
{
	int         framenum = animation->frame;
	float       syncbase = animation->syncbase;
	uint32_t data = R_GetFrameData (&alias->morph, framenum, alias, syncbase);
	return (maliasframe_t *) ((byte *) alias + data);
}

float
R_AliasGetLerpedFrames (animation_t *animation, malias_t *alias)
{
	int         framenum = animation->frame;
	float       syncbase = animation->syncbase;
	uint32_t data = R_GetFrameData (&alias->morph, framenum, alias, syncbase);

	/*
		One tenth of a second is good for most Quake animations. If
		the nextthink is longer then the animation is usually meant
		to pause (e.g. check out the shambler magic animation in
		shambler.qc).  If its shorter then things will still be
		smoothed partly, and the jumps will be less noticable
		because of the shorter time.  So, this is probably a good
		assumption.
	*/
	return R_EntityBlend (animation, data, 0.1);
}
