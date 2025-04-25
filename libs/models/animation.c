/*
	animation.c

	Animation system

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

#include "QF/hash.h"
#include "QF/qfmodel.h"

#define IMPLEMENT_ENTITY_Funcs
#include "QF/animation.h"

typedef struct {
	model_t    *mod;
	uint32_t    name;
	uint32_t    offset;
} qfa_item_t;

typedef struct {
	hashtab_t  *clips;
	hashtab_t  *joints;
} anim_registry_t;

anim_registry_t anim_registry;

void
qfa_register (model_t *mod)
{
	if (mod->type != mod_mesh) {
		return;
	}

	bool cached = false;
	auto model = mod->model;
	if (!model) {
		model = Cache_Get (&mod->cache);
		cached = true;
	}



	if (cached) {
		Cache_Release (&mod->cache);
	}
}

animation_t *
qfa_create_animation (uint32_t *clips, uint32_t num_clips, qf_model_t *model)
{
	size_t size = sizeof (animation_t)
				+ sizeof (clipstate_t[num_clips])
				+ sizeof (qfm_joint_t[model->joints.count])
				+ sizeof (qfm_motor_t[model->joints.count])
				+ sizeof (qfm_motor_t[model->joints.count]);
	animation_t *anim = malloc (size);
	auto clip_states = (clipstate_t *) &anim[1];
	auto local_pose = (qfm_joint_t *) &clip_states[num_clips];
	auto global_pose = (qfm_motor_t *) &local_pose[model->joints.count];
	auto matrix_palette = (qfm_motor_t *) &global_pose[model->joints.count];
	*anim = (animation_t) {
		.clip_states = {
			.offset = (byte *) clip_states - (byte *) anim,
			.count = num_clips,
		},
		.local_pose = (byte *) local_pose - (byte *) anim,
		.global_pose = (byte *) global_pose - (byte *) anim,
		.matrix_palette = (byte *) matrix_palette - (byte *) anim,
	};
	for (uint32_t i = 0; i < num_clips; i++) {
		clip_states[i] = (clipstate_t) {
			.type = clips[i] < model->anim.numclips ? qfc_channel : qfc_morph,
			.clip_id = clips[i],
		};
	}

	return anim;
}
