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
#include "mod_internal.h"
#if 0
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
#endif
static void
framemat (mat4f_t mat, const qfm_joint_t *f)
{
	mat4fquat (mat, f->rotate);
	mat[0] *= f->scale[0];
	mat[1] *= f->scale[1];
	mat[2] *= f->scale[2];
	mat[3] = loadvec3f (f->translate) + (vec4f_t) {0, 0, 0, 1};
}
#if 0
VISIBLE iqmframe_t *
R_IQMBlendPoseFrames (const iqm_t *iqm, int frame1, int frame2, float blend,
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
#endif
mat4f_t *
R_IQMBlendFrames (qf_model_t *model, int frame1, int frame2, float blend,
				  int extra)
{
	qfZoneScoped (true);

	size_t size = model->joints.count * sizeof (mat4f_t) + extra;
	mat4f_t *frame = Hunk_TempAlloc (0, size);

	//FIXME separate animation state
	if (model->poses.count) {
		auto base = (byte *) model + model->poses.offset;
		auto channel = (qfm_channel_t*)((byte*) model + model->channels.offset);
		auto data = (uint16_t *) ((byte *) model + frame2);
		for (uint32_t i = 0; i < model->channels.count; i++) {
			auto val = (float *) (base + channel[i].data);
			*val = channel[i].base + channel[i].scale * data[i];
		}
	}

	auto basejoints = (qfm_joint_t *) ((byte *) model + model->joints.offset);
	auto inverse = (mat4f_t *) model + model->inverse.offset;
	for (uint32_t i = 0; i < model->joints.count; i++) {
		qfm_joint_t *joint;
		if (model->poses.count) {
			joint = (qfm_joint_t *) ((byte *) model + model->poses.offset);
		} else {
			joint = (qfm_joint_t *) ((byte *) model + model->joints.offset);
		}
		int parent = joint->parent;
		framemat (frame[i], joint);
		// Pc * Bc^-1
		mmulf (frame[i], frame[i], inverse[i]);
		if (parent >= 0) {
			mat4f_t baseframe;
			framemat (baseframe, &basejoints[parent]);
			// Bp * Pc * Bc^-1
			mmulf (frame[i], baseframe, frame[i]);
			// Pp * Bp^-1 * Bp * Pc * Bc^-1
			mmulf (frame[i], frame[parent], frame[i]);
		}
	}
	return frame;
}

VISIBLE mat4f_t *
R_IQMBlendPalette (qf_model_t *model, int frame1, int frame2, float blend,
				   int extra, qfm_blend_t *blend_palette, uint32_t palette_size)
{
	extra += (palette_size - model->joints.count) * sizeof (qfm_joint_t);
	auto frame = R_IQMBlendFrames (model, frame1, frame2, blend, extra);
	for (uint32_t i = model->joints.count; i < palette_size; i++) {
		auto blend = &blend_palette[i];
		auto in = &frame[blend->indices[0]];
		auto out = &frame[i];

		float w = blend->weights[0] / 255.0;
		QuatScale (*in, w, *out);
		for (int j = 1; j < 4 && blend->weights[j]; j++) {
			in = &frame[blend->indices[j]];
			w = blend->weights[j] / 255.0;
			QuatMultAdd (*out, w, *in, *out);
		}
	}
	return frame;
}
