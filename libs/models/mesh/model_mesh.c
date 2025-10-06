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
	if (loc.offset >= buf_size || loc.offset + obj_size > buf_size) {
		Sys_Error ("%s: bad loc record for %s: %ud %ud*%zd %zd",
				   mod_name, name, loc.offset, loc.count, size, buf_size);
	}
}

void
Mod_LoadMeshModel (model_t *mod, byte *buffer, size_t buf_size)
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
	if (in->channels.count || in->anim.numclips) {
		Sys_Error ("%s: FIXME animations not supported yet", mod->name);
	}

	size_t size = sizeof (qf_model_t)
				+ sizeof (qf_mesh_t[in->meshes.count])
				+ sizeof (qfm_joint_t[in->joints.count])
				+ sizeof (qfm_motor_t[in->inverse.count])
				+ sizeof (qfm_joint_t[in->pose.count])
				+ in->text.count;
	qf_model_t *model = Hunk_AllocName (nullptr, size, mod->name);
	auto meshes    = (qf_mesh_t *)      &model[1];
	auto joints    = (qfm_joint_t *)    &meshes[in->meshes.count];
	auto inverse   = (qfm_motor_t *)    &joints[in->joints.count];
	auto pose      = (qfm_joint_t *)    &inverse[in->inverse.count];
	auto text      = (char *)           &pose[in->pose.count];

	memcpy (meshes, buffer + in->meshes.offset,
			in->meshes.count * sizeof (qf_mesh_t));
	memcpy (joints, buffer + in->joints.offset,
			in->joints.count * sizeof (qfm_joint_t));
	memcpy (inverse, buffer + in->inverse.offset,
			in->inverse.count * sizeof (qfm_motor_t));
	memcpy (pose, buffer + in->pose.offset,
			in->pose.count * sizeof (qfm_joint_t));
	memcpy (text, buffer + in->text.offset, in->pose.count * sizeof (char));

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
		.text = {
			.offset = (byte *) text - (byte *) model,
			.count = in->text.count,
		},
		.crc = in->crc,
	};

	mod->type = mod_mesh;

	mod_mesh_ctx_t mesh_ctx = {
		.qf_model = model,
		.qf_meshes = meshes,
		.mod = mod,
		.in = in,
	};
	m_funcs->Mod_MeshFinish (&mesh_ctx);

	memset (&mod->cache, 0, sizeof (mod->cache));
	mod->model = model;
}
