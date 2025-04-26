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
#include <string.h>

#include "QF/fbsearch.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/qfmodel.h"

#define IMPLEMENT_ENTITY_Funcs
#include "QF/animation.h"

typedef struct qfa_item_s {
	struct qfa_item_s *next;
	struct qfa_item_s **prev;
	model_t    *mod;
	const char *mod_name;
	const char *name;
	uint32_t    id;
} qfa_item_t;

typedef struct {
	hashctx_t  *hashctx;
	hashtab_t  *clip_tab;
	PR_RESMAP (qfa_item_t) clip_map;
	qfa_item_t *clips;

	hashtab_t  *armature_tab;
	PR_RESMAP (qfa_item_t) armature_map;
	qfa_item_t *armatures;
} anim_registry_t;

#define RESMAP_OBJ_TYPE qfa_item_t
#define RESMAP_PREFIX qfa_clip
#define RESMAP_MAP_PARAM anim_registry_t *reg
#define RESMAP_MAP reg->clip_map
#define RESMAP_ERROR(msg, ...) Sys_Error(msg __VA_OPT__(,) __VA_ARGS__)
#include "resmap_template.cinc"
#define qfa_clip_get(reg,index) _qfa_clip_get(reg,index,__FUNCTION__)

#define RESMAP_OBJ_TYPE qfa_item_t
#define RESMAP_PREFIX qfa_armature
#define RESMAP_MAP_PARAM anim_registry_t *reg
#define RESMAP_MAP reg->armature_map
#define RESMAP_ERROR(msg, ...) Sys_Error(msg __VA_OPT__(,) __VA_ARGS__)
#include "resmap_template.cinc"
#define qfa_armature_get(reg,index) _qfa_armature_get(reg,index,__FUNCTION__)

static anim_registry_t *anim_registry;

static qfa_item_t *
alloc_clip_item (anim_registry_t *reg)
{
	auto clip = qfa_clip_new (reg);
	if (reg->clips) {
		reg->clips->prev = &clip->next;
	}
	clip->prev = &reg->clips;
	clip->next = reg->clips;
	reg->clips = clip;

	return clip;
}

static void
free_clip_item (anim_registry_t *reg, qfa_item_t *clip)
{
	if (clip->next) {
		clip->next->prev = clip->prev;
	}
	*clip->prev = clip->next;
	free ((char *) clip->name);
	qfa_clip_free (reg, clip);
}

static qfa_item_t *
alloc_armature_item (anim_registry_t *reg)
{
	auto armature = qfa_armature_new (reg);
	if (reg->armatures) {
		reg->armatures->prev = &armature->next;
	}
	armature->prev = &reg->armatures;
	armature->next = reg->armatures;
	reg->armatures = armature;

	return armature;
}

static void
free_armature_item (anim_registry_t *reg, qfa_item_t *armature)
{
	if (armature->next) {
		armature->next->prev = armature->prev;
	}
	*armature->prev = armature->next;
	qfa_armature_free (reg, armature);
}

static uintptr_t
qfa_item_get_hash (const void *_item, void *data)
{
	const qfa_item_t *item = _item;
	uintptr_t hash = Hash_String (item->mod_name);
	if (item->name) {
		hash ^= Hash_String (item->name);
	}
	return hash;
}

static int
qfa_item_cmp (const void *_a, const void *_b, void *data)
{
	const qfa_item_t *a = _a;
	const qfa_item_t *b = _b;
	int cmp = strcmp (a->mod->name, b->mod_name) == 0;
	if (cmp && a->name && b->name) {
		cmp = strcmp (a->name, b->name) == 0;
	}
	return cmp;
}

void
qfa_init (void)
{
	anim_registry = malloc (sizeof (anim_registry_t));
	*anim_registry = (anim_registry_t) {
	};
	anim_registry->clip_tab = Hash_NewTable (1021, nullptr, nullptr,
											 anim_registry,
											 &anim_registry->hashctx),
	anim_registry->armature_tab = Hash_NewTable (1021, nullptr, nullptr,
												 anim_registry,
												 &anim_registry->hashctx),
	Hash_SetHashCompare (anim_registry->clip_tab, qfa_item_get_hash,
						 qfa_item_cmp);
	Hash_SetHashCompare (anim_registry->armature_tab, qfa_item_get_hash,
						 qfa_item_cmp);
}

void
qfa_shutdown (void)
{
	qfa_clip_reset (anim_registry);
	qfa_armature_reset (anim_registry);

	Hash_DelTable (anim_registry->clip_tab);
	Hash_DelTable (anim_registry->armature_tab);

	free (anim_registry);
	anim_registry = nullptr;
}

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

	for (uint32_t i = 0; i < model->anim.numclips; i++) {
		auto clip = &((clipdesc_t *) ((byte *) model + model->anim.clips))[i];
		auto clip_item = alloc_clip_item (anim_registry);
		clip_item->mod = mod;
		clip_item->mod_name = mod->name;
		clip_item->name = strdup ((const char *) model + model->text.offset
								  + clip->name);
		clip_item->id = i;

		Hash_AddElement (anim_registry->clip_tab, clip_item);
	}

	if (model->joints.count) {
		auto armature_item = alloc_armature_item (anim_registry);
		armature_item->mod = mod;
		armature_item->mod_name = mod->name;
		armature_item->name = nullptr;

		Hash_AddElement (anim_registry->armature_tab, armature_item);
	}

	if (cached) {
		Cache_Release (&mod->cache);
	}
}

void
qfa_deregister (model_t *mod)
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

	for (uint32_t i = 0; i < model->anim.numclips; i++) {
		auto clip = &((clipdesc_t *) ((byte *) model + model->anim.clips))[i];
		qfa_item_t search = {
			.mod = mod,
			.mod_name = mod->path,
			.name = (const char *) model + model->text.offset + clip->name,
		};
		qfa_item_t *clip_item = Hash_DelElement (anim_registry->clip_tab,
												 &search);
		if (clip_item) {
			free_clip_item (anim_registry, clip_item);
		}
	}

	if (model->joints.count) {
		qfa_item_t search = {
			.mod = mod,
			.mod_name = mod->path,
		};
		qfa_item_t *armature_item = Hash_DelElement(anim_registry->armature_tab,
												    &search);
		if (armature_item) {
			free_armature_item (anim_registry, armature_item);
		}
	}

	if (cached) {
		Cache_Release (&mod->cache);
	}
}

int
qfa_find_clip (const char *name)
{
	const char *sep = strchr (name, ':');
	if (!sep) {
		return 0;
	}
	char *lname = alloca (strlen (name) + 1);
	strcpy (lname, name);
	lname[sep - name] = 0;

	qfa_item_t search = {
		.mod_name = lname,
		.name = sep + 1,
	};
	qfa_item_t *clip_item = Hash_FindElement (anim_registry->clip_tab, &search);
	if (clip_item) {
		return qfa_clip_index (anim_registry, clip_item);
	}
	return 0;
}

int
qfa_find_armature (const char *name)
{
	qfa_item_t search = {
		.mod_name = name,
	};
	qfa_item_t *armature_item = Hash_FindElement (anim_registry->armature_tab,
												  &search);
	if (armature_item) {
		return qfa_armature_index (anim_registry, armature_item);
	}
	return 0;
}

static qf_model_t *
get_clip (uint32_t clip_id)
{
	auto clip_item = qfa_clip_get (anim_registry, clip_id);
	if (!clip_item->mod->model) {
		return Cache_Get (&clip_item->mod->cache);
	} else {
		return clip_item->mod->model;
	}
}

static void
release_clip (uint32_t clip_id)
{
	auto clip_item = qfa_clip_get (anim_registry, clip_id);
	if (!clip_item->mod->model) {
		Cache_Release (&clip_item->mod->cache);
	}
}

static qf_model_t *
get_armature (uint32_t armature_id)
{
	auto armature_item = qfa_armature_get (anim_registry, armature_id);
	if (!armature_item->mod->model) {
		return Cache_Get (&armature_item->mod->cache);
	} else {
		return armature_item->mod->model;
	}
}

static void
release_armature (uint32_t armature_id)
{
	auto armature_item = qfa_armature_get (anim_registry, armature_id);
	if (!armature_item->mod->model) {
		Cache_Release (&armature_item->mod->cache);
	}
}

animstate_t *
qfa_create_animation (uint32_t *clips, uint32_t num_clips, uint32_t armature,
					  qf_model_t *model)
{
	auto arm = get_armature (armature);

	size_t size = sizeof (animstate_t)
				+ sizeof (clipstate_t[num_clips])
				+ sizeof (qfm_joint_t[arm->joints.count])
				+ sizeof (qfm_joint_t[arm->joints.count])
				+ sizeof (qfm_motor_t[arm->joints.count])
				+ sizeof (qfm_motor_t[arm->joints.count]);
	animstate_t *anim = malloc (size);
	auto clip_states = (clipstate_t *) &anim[1];
	auto raw_pose = (qfm_joint_t *) &clip_states[num_clips];
	auto local_pose = (qfm_joint_t *) &raw_pose[arm->joints.count];
	auto global_pose = (qfm_motor_t *) &local_pose[arm->joints.count];
	auto matrix_palette = (qfm_motor_t *) &global_pose[arm->joints.count];
	*anim = (animstate_t) {
		.clip_states = {
			.offset = (byte *) clip_states - (byte *) anim,
			.count = num_clips,
		},
		.armature_id = armature,
		.num_joints = arm->joints.count,
		.raw_pose = (byte *) raw_pose - (byte *) anim,
		.local_pose = (byte *) local_pose - (byte *) anim,
		.global_pose = (byte *) global_pose - (byte *) anim,
		.matrix_palette = (byte *) matrix_palette - (byte *) anim,
		.play_rate = 1,
	};
	release_armature (armature);

	for (uint32_t i = 0; i < num_clips; i++) {
		auto cm = get_clip (clips[i]);
		clip_states[i] = (clipstate_t) {
			.type = clips[i] < cm->anim.numclips ? qfc_channel : qfc_morph,
			.clip_id = clips[i],
			.weight = 1.0 / num_clips,
		};
		release_clip (clips[i]);
	}

	return anim;
}

void
qfa_free_animation (animstate_t *anim)
{
	free (anim);
}

static float
calc_t (float end, float len, float time)
{
	float ago = end - time;
	return (len - ago) / len;
}

static int
qfa_keyframe_cmp (const void *_a, const void *_b)
{
	const float *a = _a;
	const keyframe_t *b = _b;
	float diff = *a - b->endtime;
	return diff < 0 ? -1 : diff > 0 ? 1 : 0;
}

static void
qfa_update_clip (clipstate_t *clipstate, clipdesc_t *clipdesc,
				 keyframe_t *keyframes, float time)
{
	auto last_frame = &keyframes[clipdesc->numframes];
	auto first_frame = &keyframes[0];
	float duration = last_frame->endtime;

	if (time >= clipstate->end_time || time < clipstate->end_time - duration) {
		if (!clipstate->end_time || (clipstate->flags & qfc_loop)) {
			int cycles = (time - clipstate->end_time) / duration;
			clipstate->end_time = (cycles + 1) * duration;
		}
	}
	float t = calc_t (clipstate->end_time, duration, time);
	if (time >= 1) {
		clipstate->frame = clipdesc->numframes;
		clipstate->frac = 1;
		return;
	}
	float clip_time = t * duration;
	if (clip_time < first_frame->endtime) {
		clipstate->frame = 0;
	} else {
		keyframe_t *kf = fbsearch (&clip_time, keyframes, clipdesc->numframes,
								   sizeof (keyframe_t), qfa_keyframe_cmp);
		clipstate->frame = (kf - keyframes) + 1;
	}
	auto cur_frame = &keyframes[clipstate->frame];
	float frame_len = cur_frame->endtime;
	if (clipstate->frame) {
		frame_len -= keyframes[clipstate->frame - 1].endtime;
	}
	clipstate->frac = calc_t (cur_frame->endtime, frame_len, clip_time);
}

static void
qfa_apply_channels (qfm_joint_t *pose, qf_model_t *model,
					uint32_t frame1, uint32_t frame2, float blend)
{
	auto base = (byte *) pose;
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

void
qfa_update_anim (animstate_t *anim, float dt)
{
	anim->time += anim->play_rate * dt;

	qfa_item_t *items[anim->clip_states.count + 1];
	model_t *mods[anim->clip_states.count + 1] = {};
	qf_model_t *models[anim->clip_states.count + 1] = {};
	auto clipstates = (clipstate_t *)((byte *) anim + anim->clip_states.offset);
	for (uint32_t i = 0; i < anim->clip_states.count; i++) {
		items[i] = qfa_clip_get (anim_registry, clipstates[i].clip_id);
	}
	items[anim->clip_states.count] = qfa_armature_get (anim_registry,
													   anim->armature_id);

	for (uint32_t i = 0; i < anim->clip_states.count + 1; i++) {
		mods[i] = items[i]->mod;
		if (!mods[i]->model) {
			models[i] = Cache_Get (&mods[i]->cache);
		} else {
			models[i] = mods[i]->model;
		}
	}

	auto raw_pose = qfa_raw_pose (anim);
	auto local_pose = qfa_local_pose (anim);
	auto global_pose = qfa_global_pose (anim);
	auto matrix_palette = qfa_matrix_palette (anim);
	auto arm = models[anim->clip_states.count];
	auto inv_motors = (qfm_motor_t *) ((byte *) arm + arm->inverse.offset);

	memset (local_pose, 0, sizeof (qfm_joint_t[anim->num_joints]));
	for (uint32_t i = 0; i < anim->clip_states.count; i++) {
		auto clipdesc = qfm_clipdesc (models[i], items[i]->id);
		auto keyframes = qfm_keyframe (models[i], clipdesc->firstframe);
		qfa_update_clip (&clipstates[i], clipdesc, keyframes, anim->time);
		memcpy (raw_pose, (byte *) arm + arm->pose.offset,
				sizeof (qfm_joint_t[anim->num_joints]));

		uint32_t frame1 = clipstates[i].frame;
		uint32_t frame2 = clipstates[i].frame;
		if (frame1 > 0) {
			frame1 -= 1;
		}
		frame1 = keyframes[frame1].data;
		frame2 = keyframes[frame2].data;
		float blend = clipstates[i].frac;
		qfa_apply_channels (raw_pose, models[i], frame1, frame2, blend);

		for (uint32_t j = 0; j < anim->num_joints; j++) {
			VectorMultAdd (local_pose[j].translate,
						   clipstates[i].weight, raw_pose[j].translate,
						   local_pose[j].translate);
			QuatMultAdd (local_pose[j].rotate,
						 clipstates[i].weight, raw_pose[j].rotate,
						 local_pose[j].rotate);
			VectorMultAdd (local_pose[j].scale,
						   clipstates[i].weight, raw_pose[j].scale,
						   local_pose[j].scale);
			local_pose[j].parent = raw_pose[j].parent;
		}
	}
	for (uint32_t i = 0; i < anim->num_joints; i++) {
		int parent = local_pose[i].parent;
		global_pose[i] = qfm_make_motor (local_pose[i]);
		if (inv_motors[i].flags & qfm_nonleaf) {
			global_pose[i].flags &= ~qfm_nonuniform;
			global_pose[i].flags |= qfm_nonleaf;
			float *s = global_pose[i].s;
			float scale = (s[0] + s[1] + s[2]) / 3;
			s[2] = s[1] = s[0] = scale;
		}
		if (parent >= 0) {
			global_pose[i] = qfm_motor_mul (global_pose[parent],
											global_pose[i]);
		}
	}
	for (uint32_t i = 0; i < anim->num_joints; i++) {
		matrix_palette[i] = qfm_motor_mul (global_pose[i], inv_motors[i]);
	}

	for (uint32_t i = 0; i < anim->clip_states.count + 1; i++) {
		if (!mods[i]->model) {
			Cache_Release (&mods[i]->cache);
		}
	}
}

void
qfa_reset_anim (animstate_t *anim)
{
	auto clipstates = (clipstate_t *)((byte *) anim + anim->clip_states.offset);
	for (uint32_t i = 0; i < anim->clip_states.count; i++) {
		clipstates[i].end_time = 0;
	}
	anim->time = -1;
	qfa_update_anim (anim, 1);
}
