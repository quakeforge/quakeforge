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
R_IQMGetLerpedFrames (animation_t *animation, iqm_t *iqm)
{
	qfZoneScoped (true);
	int         frame = animation->frame;
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
	time = vr_data.realtime + animation->syncbase;
	time -= ((int) (time / fullinterval)) * fullinterval;
	frame = (int) (time * anim->framerate) + anim->first_frame;
	return R_EntityBlend (animation, frame, 1.0 / anim->framerate);
}

static vec4f_t
iqm_translate (const iqm_t *iqm, const iqmpose_t *pose, int frame)
{
	auto data = iqm->framedata + frame * iqm->num_framechannels;
	auto offset = &pose->channeloffset[0];
	auto base = &pose->channelbase[0];
	auto scale = &pose->channelscale[0];
	vec4f_t t = { VectorExpand (offset) };
	for (int i = 0; i < 3; i++) {
		if (pose->mask & (1<<i)) {
			t[i] += data[base[i]] * scale[i];
		}
	}
	return t;
}

static vec4f_t
iqm_rotate (const iqm_t *iqm, const iqmpose_t *pose, int frame)
{
	auto data = iqm->framedata + frame * iqm->num_framechannels;
	auto offset = &pose->channeloffset[3];
	auto base = &pose->channelbase[3];
	auto scale = &pose->channelscale[3];
	vec4f_t r = { QuatExpand (offset) };
	for (int i = 0; i < 4; i++) {
		if (pose->mask & (1<<(i + 3))) {
			r[i] += data[base[i]] * scale[i];
		}
	}
	return r;
}

static vec4f_t
iqm_scale (const iqm_t *iqm, const iqmpose_t *pose, int frame)
{
	auto data = iqm->framedata + frame * iqm->num_framechannels;
	auto offset = &pose->channeloffset[7];
	auto base = &pose->channelbase[7];
	auto scale = &pose->channelscale[7];
	vec4f_t s = { VectorExpand (offset) };
	for (int i = 0; i < 3; i++) {
		if (pose->mask & (1<<(i + 7))) {
			s[i] += data[base[i]] * scale[i];
		}
	}
	return s;
}

static void
iqm_framemul (iqmframe_t *c, const iqmframe_t *a, const iqmframe_t *b)
{
	iqmframe_t t = {
		.translate = qvmulf (a->rotate, b->translate) + a->translate,
		.rotate = qmulf (a->rotate, b->rotate),
		.scale = a->scale * b->scale,
	};
	*c = t;
}

iqmframe_t *
R_IQMBlendFrames (const iqm_t *iqm, int frame1, int frame2, float blend,
				  int extra)
{
	qfZoneScoped (true);
	iqmframe_t *frame;

	frame = Hunk_TempAlloc (0, iqm->num_joints * sizeof (iqmframe_t) + extra);
	for (int i = 0; i < iqm->num_joints; i++) {
		auto f = &frame[i];
		int parent = -1;
		if (iqm->num_frames) {
			auto pose = &iqm->poses[i];
			parent = pose->parent;

			vec4f_t t1 = iqm_translate (iqm, pose, frame1);
			vec4f_t r1 = iqm_rotate (iqm, pose, frame1);
			vec4f_t s1 = iqm_scale (iqm, pose, frame1);

			vec4f_t t2 = iqm_translate (iqm, pose, frame2);
			vec4f_t r2 = iqm_rotate (iqm, pose, frame2);
			vec4f_t s2 = iqm_scale (iqm, pose, frame2);

			*f = (iqmframe_t) {
				.translate = t1 * (1 - blend) + t2 * blend,
				.rotate = normalf (r1 * (1 - blend) + r2 * blend),
				.scale = s1 * (1 - blend) + s2 * blend,
			};
		} else {
			iqmjoint_t *j = &iqm->joints[i];
			parent = j->parent;
			*f = (iqmframe_t) {
				.translate = { VectorExpand (j->translate) },
				.rotate = j->rotate,
				.scale = { VectorExpand (j->scale)},
			};
		}
		// Pc * Bc^-1
		iqm_framemul (f, f, &iqm->inverse_basejoints[i]);
		if (parent >= 0) {
			// Bp * Pc * Bc^-1
			iqm_framemul (f, &iqm->basejoints[parent], f);
			// Pp * Bp^-1 * Bp * Pc * Bc^-1
			iqm_framemul (f, &frame[parent], f);
		}
	}
	return frame;
}

iqmframe_t *
R_IQMBlendPalette (const iqm_t *iqm, int frame1, int frame2, float blend,
				   int extra, iqmblend_t *blend_palette, int palette_size)
{
	iqmframe_t *frame;

	extra += (palette_size - iqm->num_joints) * sizeof (iqmframe_t);
	frame = R_IQMBlendFrames (iqm, frame1, frame2, blend, extra);
	for (int i = iqm->num_joints; i < palette_size; i++) {
		iqmblend_t *blend = &blend_palette[i];
		auto in = &frame[blend->indices[0]];
		auto out = &frame[i];

		float w = blend->weights[0] / 255.0;
		*out = (iqmframe_t) {
			.translate = in->translate * w,
			.rotate = in->rotate * w,
			.scale = in->scale * w,
		};
		for (int j = 1; j < 4 && blend->weights[j]; j++) {
			in = &frame[blend->indices[j]];
			w = blend->weights[0] / 255.0;
			out->translate += in->translate * w;
			out->rotate += in->rotate * w;
			out->scale += in->scale * w;
		}
	}
	return frame;
}
