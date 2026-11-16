/*
	model_mesh.c

	Direct qf_model_t loading

	Copyright (C) 2025 Bill Currie <bill@taniwha.org>

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

#include "mod_internal.h"

static void
check_loc (qfm_loc_t loc, size_t size, size_t buf_size, const char *name,
		   const char *mod_name)
{
	size_t obj_size = loc.count * size;
	if (loc.offset > buf_size || loc.offset + obj_size > buf_size) {
		Sys_Error ("%s: bad loc record for %s: %u %u*%zd %zd",
				   mod_name, name, loc.offset, loc.count, size, buf_size);
	}
}

void
Mod_LoadMeshModel (model_t *mod, byte *buffer, size_t buf_size, memhunk_t *hunk)
{
	if (!m_funcs->Mod_MeshFinish) {
		Sys_Error ("Mod_LoadMeshModel: not supported for this renderer");
	}

	//FIXME should have a file header and byte swapping (separate function,
	//though)
	auto in = (qf_model_t *) buffer;

	check_loc (in->meshes,   sizeof (qf_mesh_t),     buf_size, "meshes",
			   mod->name);
	check_loc (in->joints,   sizeof (qfm_joint_t),   buf_size, "joints",
			   mod->name);
	check_loc (in->inverse,  sizeof (qfm_motor_t),   buf_size, "inverse",
			   mod->name);
	check_loc (in->pose,     sizeof (qfm_joint_t),   buf_size, "pose",
			   mod->name);
	check_loc (in->channels, sizeof (qfm_channel_t), buf_size, "channels",
			   mod->name);
	check_loc (in->text,     sizeof (char),          buf_size, "text",
			   mod->name);
	//FIXME check anim
	if (in->render_data) {
		Sys_Error ("%s: render_data not 0", mod->name);
	}

	if (in->inverse.count && in->inverse.count != in->joints.count) {
		Sys_Error ("%s: inverse not 0 and doesn't match joints", mod->name);
	}
	if (in->pose.count && in->pose.count != in->joints.count) {
		Sys_Error ("%s: pose not 0 and doesn't match joints", mod->name);
	}
	uint16_t *in_framedata = nullptr;
	uint32_t num_keyframes = 0;
	for (uint32_t i = 0; i < in->anim.numclips; i++) {
		auto c = &((clipdesc_t *) ((byte *) in + in->anim.clips))[i];
		num_keyframes += c->numframes;
	}
	if (num_keyframes) {
		auto kf = (keyframe_t *) ((byte *) in + in->anim.keyframes);
		in_framedata = (uint16_t *) ((byte *) in + kf->data);
	}
	uint32_t frame_data_count = num_keyframes * in->channels.count;

	size_t size = sizeof (qf_model_t)
				+ sizeof (qf_mesh_t[in->meshes.count])
				+ sizeof (qfm_joint_t[in->joints.count])
				+ sizeof (qfm_motor_t[in->inverse.count])
				+ sizeof (qfm_joint_t[in->pose.count])
				+ sizeof (clipdesc_t[in->anim.numclips])
				+ sizeof (keyframe_t[num_keyframes])
				+ sizeof (qfm_channel_t[in->channels.count])
				+ sizeof (qfm_frame_t[num_keyframes])
				+ sizeof (uint16_t[frame_data_count])
				+ in->text.count;
	qf_model_t *model = Hunk_AllocName (hunk, size, mod->name);
	auto meshes    = (qf_mesh_t *)      &model[1];
	auto joints    = (qfm_joint_t *)    &meshes[in->meshes.count];
	auto inverse   = (qfm_motor_t *)    &joints[in->joints.count];
	auto pose      = (qfm_joint_t *)    &inverse[in->inverse.count];
	auto clips     = (clipdesc_t *)     &pose[in->pose.count];
	auto keyframes = (keyframe_t *)     &clips[in->anim.numclips];
	auto channels  = (qfm_channel_t *)  &keyframes[num_keyframes];
	auto bounds    = (qfm_frame_t *)    &channels[in->channels.count];
	auto framedata = (uint16_t *)       &bounds[num_keyframes];
	auto text      = (char *)           &framedata[frame_data_count];

	*model = (qf_model_t) {
		.meshes = {
			.offset = (byte *) meshes - (byte *) model,
			.count = in->meshes.count,
		},
		.joints = {
			.offset = (byte *) joints - (byte *) model,
			.count = in->joints.count,
		},
		.inverse = {
			.offset = (byte *) inverse - (byte *) model,
			.count = in->joints.count,
		},
		.pose = {
			.offset = (byte *) pose - (byte *) model,
			.count = in->pose.count,
		},
		.channels = {
			.offset = (byte *) channels - (byte *) model,
			.count = in->channels.count,
		},
		.text = {
			.offset = (byte *) text - (byte *) model,
			.count = in->text.count,
		},
		.anim = {
			.numclips = in->anim.numclips,
			.clips = (byte *) clips - (byte *) model,
			.keyframes = (byte *) keyframes - (byte *) model,
			.data = (byte *) bounds - (byte *) model,
		},
		.bounds_min = { VectorExpand (in->bounds_min) },
		.bounds_max = { VectorExpand (in->bounds_max) },
		.crc = in->crc,
	};

	memcpy (meshes, buffer + in->meshes.offset,
			in->meshes.count * sizeof (qf_mesh_t));
	memcpy (joints, buffer + in->joints.offset,
			in->joints.count * sizeof (qfm_joint_t));
	memcpy (inverse, buffer + in->inverse.offset,
			in->inverse.count * sizeof (qfm_motor_t));
	memcpy (pose, buffer + in->pose.offset,
			in->pose.count * sizeof (qfm_joint_t));
	memcpy (clips, buffer + in->anim.clips,
			in->anim.numclips * sizeof (clipdesc_t));
	int32_t frame_data_delta = 0;
	if (frame_data_count) {
		frame_data_delta = (byte *) framedata - (byte *) model;
		frame_data_delta -= (byte *) in_framedata - (byte *) in;
	}
	for (uint32_t i = 0; i < num_keyframes; i++) {
		auto ik = &((keyframe_t *) ((byte *) in + in->anim.keyframes))[i];
		auto ok = &((keyframe_t *) ((byte *) model + model->anim.keyframes))[i];
		*ok = (keyframe_t) {
			.endtime = ik->endtime,
			.data = ik->data + frame_data_delta,
		};
	}
	memcpy (channels, buffer + in->channels.offset,
			in->channels.count * sizeof (qfm_channel_t));
	memcpy (bounds, buffer + in->anim.data,
			num_keyframes * sizeof (qfm_frame_t));
	memcpy (framedata, in_framedata,
			frame_data_count * sizeof (uint16_t));
	memcpy (text, buffer + in->text.offset, in->text.count * sizeof (char));

	mod->type = mod_mesh;

	mod_mesh_ctx_t mesh_ctx = {
		.qf_model = model,
		.qf_meshes = meshes,
		.mod = mod,
		.hunk = hunk,
		.in = in,
	};
	if (model->meshes.count) {
		m_funcs->Mod_MeshFinish (&mesh_ctx);
	}

	memset (&mod->cache, 0, sizeof (mod->cache));

	vec4f_t model_min = { INFINITY, INFINITY, INFINITY };
	vec4f_t model_max = {-INFINITY,-INFINITY,-INFINITY };
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		model_min = minv4f (model_min, loadvec3f (meshes[i].bounds_min));
		model_max = maxv4f (model_max, loadvec3f (meshes[i].bounds_max));
	}
	mod->model = model;
	storevec3f (mod->mins, model_min);
	storevec3f (mod->maxs, model_max);
}
