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

VISIBLE float
R_IQMGetLerpedFrames (double in_time, animation_t *animation,
					  qf_model_t *model)
{
	qfZoneScoped (true);
	uint32_t    frame = animation->frame;

	if (!model->anim.numdesc) {
		return R_EntityBlend (in_time, animation, 0, 1.0 / 25.0);
	}
	if (frame >= model->anim.numdesc) {
		Sys_MaskPrintf (SYS_dev, "R_IQMGetLerpedFrames: no such frame %d\n",
						frame);
		frame = 0;
	}
	auto keyframedescs = (keyframedesc_t *) ((byte *) model
											 + model->anim.descriptors);
	auto keyframes = (keyframe_t *) ((byte *) model + model->anim.keyframes);

	auto anim = &keyframedescs[frame];
	uint32_t last_frame = anim->firstframe + anim->numframes - 1;
	float fullinterval = keyframes[last_frame].endtime;
	float framerate = anim->numframes / fullinterval;
	double time = in_time + animation->syncbase;
	time -= floor (time / fullinterval) * fullinterval;
	frame = (int) (time * framerate) + anim->firstframe;
	frame = keyframes[anim->firstframe + frame].data;
	return R_EntityBlend (in_time, animation, frame,
						  fullinterval / anim->numframes);
}


// will use the space for the motors to hold the base joints for animation
static_assert (sizeof (qfm_motor_t) == sizeof (qfm_joint_t));

static void
apply_channels (qfm_joint_t *joints, qf_model_t *model,
				uint32_t frame1, uint32_t frame2, float blend)
{
	auto base = (byte *) joints;
	auto channel = (qfm_channel_t *) ((byte*) model + model->channels.offset);
	auto data1 = (uint16_t *) ((byte *) model + frame1);
	auto data2 = (uint16_t *) ((byte *) model + frame2);
	for (uint32_t i = 0; i < model->channels.count; i++) {
		auto val = (float *) (base + channel[i].data);
		float offset1 = channel[i].scale * data1[i];
		float offset2 = channel[i].scale * data2[i];
		*val = channel[i].base + Blend (offset1, offset2, blend);
	}
}

VISIBLE qfm_motor_t *
R_IQMBlendPoseFrames (qf_model_t *model, int frame1, int frame2,
					  float blend, int extra)
{
	qfZoneScoped (true);

	auto joints = (qfm_joint_t *) ((byte *) model + model->joints.offset);
	auto pose = (qfm_joint_t *) ((byte *) model + model->pose.offset);
	auto base_motors = (qfm_motor_t *) ((byte *) model + model->base.offset);
	auto inv_motors = (qfm_motor_t *) ((byte *) model + model->inverse.offset);

	if (!frame1) {
		frame1 = frame2;
	}

	size_t size = sizeof (qfm_motor_t[model->joints.count]) + extra;
	qfm_motor_t *motors = Hunk_TempAlloc (0, size);
	if (model->channels.count && model->joints.count == model->pose.count) {
		memcpy (motors, pose, sizeof (qfm_motor_t[model->pose.count]));
		pose = (qfm_joint_t *) motors;
		apply_channels (pose, model, frame1, frame2, blend);
	} else {
		pose = joints;
	}

	// use joints for the count in case there's no (valid) pose data, in which
	// case pose == joints
	for (uint32_t i = 0; i < model->joints.count; i++) {
		// grab parent first because pose might be aliased with motors
		int parent = pose[i].parent;
		motors[i] = qfm_make_motor (pose[i]);
		// Pc * Bc^-1
		motors[i] = qfm_motor_mul (motors[i], inv_motors[i]);
		if (parent >= 0) {
			// Bp * Pc * Bc^-1
			motors[i] = qfm_motor_mul (base_motors[parent], motors[i]);
			// Pp * Bp^-1 * Bp * Pc * Bc^-1
			motors[i] = qfm_motor_mul (motors[parent], motors[i]);
		}
	}
	return motors;
}

mat4f_t *
R_IQMBlendFrames (qf_model_t *model, int frame1, int frame2, float blend,
				  size_t extra)
{
	qfZoneScoped (true);

	extra += 2 * model->joints.count * sizeof (mat4f_t);
	auto pose = R_IQMBlendPoseFrames (model, frame1, frame2, blend, extra);
	auto frame = (mat4f_t *) &pose[model->joints.count];

	for (uint32_t i = 0; i < model->joints.count; i++) {
		qfm_motor_to_mat (frame[i * 2], pose[i]);
	}
	return frame;
}

static inline void
cofactor_matrix (mat4f_t cf, const mat4f_t m)
{
	cf[0] = crossf (m[1], m[2]);
	cf[1] = crossf (m[2], m[0]);
	cf[2] = crossf (m[0], m[1]);
	vec4i_t pos = dotf (m[0], cf[0]) >= 0;
	vec4i_t neg = ~pos;
	cf[3] = (vec4f_t)((pos & (vec4i_t)((vec4f_t) { 1, 1, 1, 1}))
					+ (neg & (vec4i_t)((vec4f_t) {-1,-1,-1,-1})));
}

VISIBLE mat4f_t *
R_IQMBlendPalette (qf_model_t *model, int frame1, int frame2, float blend,
				   size_t extra, qfm_blend_t *blend_palette,
				   uint32_t palette_size)
{
	extra += 2 * (palette_size - model->joints.count) * sizeof (mat4f_t);
	auto frame = R_IQMBlendFrames (model, frame1, frame2, blend, extra);
	for (uint32_t i = 0; i < model->joints.count; i++) {
		auto out = &frame[i * 2];
		cofactor_matrix (out[1], out[0]);
	}
	for (int i = 0; i < 1; i++) {
		auto out = &frame[(model->joints.count + i) * 2];
		mat4fidentity (out[0]);
		mat4fidentity (out[1]);
		out[1][3] = (vec4f_t) { 1, 1, 1, 1 };
	}
	for (uint32_t i = model->joints.count + 1; i < palette_size; i++) {
		auto blend = &blend_palette[i];
		auto in = &frame[blend->indices[0] * 2];
		auto out = &frame[i * 2];

		float w = blend->weights[0] / 255.0;
		QuatScale (in[0], w, out[0]);
		for (int j = 1; j < 4 && blend->weights[j]; j++) {
			in = &frame[blend->indices[j] * 2];
			w = blend->weights[j] / 255.0;
			QuatMultAdd (out[0], w, in[0], out[0]);
		}
		cofactor_matrix (out[1], out[0]);
	}
	return frame;
}
