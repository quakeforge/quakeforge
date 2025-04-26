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

#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/qfmodel.h"

#define IMPLEMENT_ENTITY_Funcs
#include "QF/animation.h"

typedef struct qfa_item_s {
	struct qfa_item_s *next;
	struct qfa_item_s **prev;
	model_t    *mod;
	const char *name;
	uint32_t    offset;
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

#define RESMAP_OBJ_TYPE qfa_item_t
#define RESMAP_PREFIX qfa_armature
#define RESMAP_MAP_PARAM anim_registry_t *reg
#define RESMAP_MAP reg->armature_map
#define RESMAP_ERROR(msg, ...) Sys_Error(msg __VA_OPT__(,) __VA_ARGS__)
#include "resmap_template.cinc"

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
	uintptr_t hash = Hash_String (item->mod->path);
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
	int cmp = strcmp (a->mod->name, b->mod->name) == 0;
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
	anim_registry->clip_tab = Hash_NewTable (1021, nullptr, nullptr, anim_registry, &anim_registry->hashctx),
	anim_registry->armature_tab = Hash_NewTable (1021, nullptr, nullptr, anim_registry, &anim_registry->hashctx),
	Hash_SetHashCompare (anim_registry->clip_tab, qfa_item_get_hash, qfa_item_cmp);
	Hash_SetHashCompare (anim_registry->armature_tab, qfa_item_get_hash, qfa_item_cmp);
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
		clip_item->name = strdup ((const char *) model + model->text.offset + clip->name);
		clip_item->offset = (byte *) clip - (byte *) model;

		Hash_AddElement (anim_registry->clip_tab, clip_item);
	}

	if (model->joints.count) {
		auto armature_item = alloc_armature_item (anim_registry);
		armature_item->mod = mod;
		armature_item->name = nullptr;
		armature_item->offset = model->joints.offset;

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
			.name = (const char *) model + model->text.offset + clip->name,
		};
		qfa_item_t *clip_item = Hash_DelElement (anim_registry->clip_tab, &search);
		if (clip_item) {
			free_clip_item (anim_registry, clip_item);
		}
	}

	if (model->joints.count) {
		qfa_item_t search = {
			.mod = mod,
		};
		qfa_item_t *armature_item = Hash_DelElement (anim_registry->armature_tab, &search);
		if (armature_item) {
			free_armature_item (anim_registry, armature_item);
		}
	}

	if (cached) {
		Cache_Release (&mod->cache);
	}
}

animstate_t *
qfa_create_animation (uint32_t *clips, uint32_t num_clips, qf_model_t *model)
{
	size_t size = sizeof (animstate_t)
				+ sizeof (clipstate_t[num_clips])
				+ sizeof (qfm_joint_t[model->joints.count])
				+ sizeof (qfm_motor_t[model->joints.count])
				+ sizeof (qfm_motor_t[model->joints.count]);
	animstate_t *anim = malloc (size);
	auto clip_states = (clipstate_t *) &anim[1];
	auto local_pose = (qfm_joint_t *) &clip_states[num_clips];
	auto global_pose = (qfm_motor_t *) &local_pose[model->joints.count];
	auto matrix_palette = (qfm_motor_t *) &global_pose[model->joints.count];
	*anim = (animstate_t) {
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
