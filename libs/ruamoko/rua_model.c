/*
	bi_model.c

	Ruamkoko model builtins

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/animation.h"
#include "QF/model.h"
#include "QF/progs.h"
#include "QF/qfmodel.h"

#include "rua_internal.h"

typedef struct clipinfo_s {
	pr_string_t name;
	uint        num_frames;
	uint        num_channels;
	qfm_type_t  channel_type;
} clipinfo_t;

typedef struct rua_model_s {
	struct rua_model_s *next;
	struct rua_model_s **prev;
	model_t    *model;
} rua_model_t;

typedef struct rua_animstate_s {
	struct rua_animstate_s *next;
	struct rua_animstate_s **prev;
	animstate_t    *animstate;
} rua_animstate_t;

typedef struct {
	PR_RESMAP (rua_model_t) model_map;
	rua_model_t *handles;

	PR_RESMAP (rua_animstate_t) animstate_map;
	rua_animstate_t *anims;

	progs_t    *pr;
} rua_model_resources_t;

#define RESMAP_OBJ_TYPE rua_model_t
#define RESMAP_PREFIX rua_model_handle
#define RESMAP_MAP_PARAM rua_model_resources_t *res
#define RESMAP_MAP res->model_map
#define RESMAP_ERROR(msg, ...) \
	PR_RunError (res->pr, msg __VA_OPT__(,) __VA_ARGS__)
#include "resmap_template.cinc"
#define rua_model_handle_get(reg,index) \
	_rua_model_handle_get(reg,index,__FUNCTION__)

#define RESMAP_OBJ_TYPE rua_animstate_t
#define RESMAP_PREFIX rua_animstate
#define RESMAP_MAP_PARAM rua_model_resources_t *res
#define RESMAP_MAP res->animstate_map
#define RESMAP_ERROR(msg, ...) \
	PR_RunError (res->pr, msg __VA_OPT__(,) __VA_ARGS__)
#include "resmap_template.cinc"
#define rua_animstate_get(reg,index) \
	_rua_animstate_get(reg,index,__FUNCTION__)

static void
bi_rua_model_clear (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	rua_model_resources_t *res = (rua_model_resources_t *) _res;

	for (auto handle = res->handles; handle; handle = handle->next) {
		Mod_UnloadModel (handle->model);
	}
	res->handles = 0;
	rua_model_handle_reset (res);

	for (auto anim = res->anims; anim; anim = anim->next) {
		qfa_free_animation (anim->animstate);
	}
	res->anims = 0;
	rua_animstate_reset (res);
}

static void
bi_rua_model_destroy (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	rua_model_resources_t *res = _res;
	PR_RESDELMAP (res->model_map);
	PR_RESDELMAP (res->animstate_map);
	free (res);
}

static int
alloc_handle (rua_model_resources_t *res, model_t *model)
{
	qfZoneScoped (true);
	rua_model_t    *handle = rua_model_handle_new (res);

	if (!handle)
		return 0;

	handle->next = res->handles;
	handle->prev = &res->handles;
	if (res->handles)
		res->handles->prev = &handle->next;
	res->handles = handle;
	handle->model = model;
	return rua_model_handle_index (res, handle);
}

static int
alloc_animstate (rua_model_resources_t *res, animstate_t *anim)
{
	qfZoneScoped (true);
	auto rua_anim = rua_animstate_new (res);

	if (!rua_anim) {
		return 0;
	}

	rua_anim->next = res->anims;
	rua_anim->prev = &res->anims;
	if (res->anims) {
		res->anims->prev = &rua_anim->next;
	}
	res->anims = rua_anim;
	rua_anim->animstate = anim;
	return rua_animstate_index (res, rua_anim);
}

#define bi(x) static void bi_##x (progs_t *pr, void *_res)

bi (Model_Load)
{
	qfZoneScoped (true);
	auto res = (rua_model_resources_t *) _res;
	const char *path = P_GSTRING (pr, 0);
	model_t    *model;

	R_INT (pr) = 0;
	if (!(model = Mod_ForName (path, 0)))
		return;
	if (!(R_INT (pr) = alloc_handle (res, model)))
		Mod_UnloadModel (model);
}

model_t *
Model_GetModel (progs_t *pr, int handle)
{
	qfZoneScoped (true);
	if (!handle) {
		return 0;
	}
	rua_model_resources_t *res = PR_Resources_Find (pr, "Model");
	rua_model_t    *h = rua_model_handle_get (res, handle);

	return h->model;
}

bi (Model_Unload)
{
	qfZoneScoped (true);
	auto res = (rua_model_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	rua_model_t    *h = rua_model_handle_get (res, handle);

	Mod_UnloadModel (h->model);
	*h->prev = h->next;
	if (h->next)
		h->next->prev = h->prev;
	rua_model_handle_free (res, h);
}

bi (Model_NumJoints)
{
	auto res = (rua_model_resources_t *) _res;
	int  handle = P_INT (pr, 0);
	auto h = rua_model_handle_get (res, handle);
	auto mod = h->model;

	R_INT (pr) = 0;
	if (mod->type == mod_mesh) {
		auto model = mod->model;
		R_INT (pr) = model->joints.count;
	}
}

bi (Model_GetJoints)
{
	auto res = (rua_model_resources_t *) _res;
	int  handle = P_INT (pr, 0);
	auto h = rua_model_handle_get (res, handle);
	auto mod = h->model;
	auto model = mod->model;
	auto joints = (qfm_joint_t *) P_GPOINTER (pr, 1);

	if (mod->type != mod_mesh || !model->joints.count) {
		R_INT (pr) = 0;
		return;
	}
	auto j = (qfm_joint_t *) ((byte *) model + model->joints.offset);

	memcpy (joints, j, model->joints.count * sizeof (qfm_joint_t));
	auto text = (const char *) ((byte *) model + model->text.offset);
	for (uint32_t i = 0; i < model->joints.count; i++) {
		auto name = text + joints[i].name;
		joints[i].name = PR_SetString (pr, name);
	}
	R_INT (pr) = 1;
}

bi (Model_NumClips)
{
	auto res = (rua_model_resources_t *) _res;
	int  handle = P_INT (pr, 0);
	auto h = rua_model_handle_get (res, handle);
	auto mod = h->model;

	uint32_t numclips = 0;
	if (mod->type == mod_mesh) {
		bool cached = false;
		auto model = mod->model;
		if (!model) {
			model = Cache_Get (&mod->cache);
			cached = true;
		}

		numclips = model->anim.numclips;
		for (uint32_t i = 0; i < model->meshes.count; i++) {
			auto mesh = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
			numclips += mesh->morph.numclips;
		}

		if (cached) {
			Cache_Release (&mod->cache);
		}
	}
	R_UINT (pr) = numclips;
}

static qf_model_t *
rua_model_get_model (progs_t *pr, rua_model_resources_t *res)
{
	R_INT (pr) = 0;

	int  handle = P_INT (pr, 0);
	auto h = rua_model_handle_get (res, handle);
	auto mod = h->model;
	auto model = mod->model;

	if (mod->type != mod_mesh || !model->joints.count) {
		return nullptr;
	}
	return model;
}

bi (Model_GetInverseMotors)
{
	qfZoneScoped (true);

	auto model = rua_model_get_model (pr, _res);
	if (model) {
		auto motors = (qfm_motor_t *) P_GPOINTER (pr, 1);
		auto m = (qfm_motor_t *) ((byte *) model + model->inverse.offset);
		memcpy (motors, m, model->inverse.count * sizeof (motors[0]));

		R_INT (pr) = 1;
	}
}

bi (Model_GetClipInfo)
{
	auto res = (rua_model_resources_t *) _res;
	int  handle = P_INT (pr, 0);
	auto h = rua_model_handle_get (res, handle);
	auto mod = h->model;
	unsigned clip = P_UINT (pr, 1);

	R_PACKED (pr, clipinfo_t) = (clipinfo_t) {};
	if (mod->type == mod_mesh) {
		bool cached = false;
		auto model = mod->model;
		if (!model) {
			model = Cache_Get (&mod->cache);
			cached = true;
		}
		auto text = (const char *) model + model->text.offset;
		if (clip < model->anim.numclips) {
			auto clips = (clipdesc_t *) ((byte *) model + model->anim.clips);
			R_PACKED (pr, clipinfo_t) = (clipinfo_t) {
				.name = PR_SetReturnString (pr, text + clips[clip].name),
				.num_frames = clips[clip].numframes,
				.num_channels = model->channels.count,
				.channel_type = qfm_u16,
			};
		} else {
			clip -= model->anim.numclips;
			bool found = false;
			for (uint32_t i = 0; !found && i < model->meshes.count; i++) {
				auto mesh = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
				if (clip < mesh->morph.numclips) {
					auto clips = (clipdesc_t *) ((byte *) mesh + mesh->morph.clips);
					R_PACKED (pr, clipinfo_t) = (clipinfo_t) {
						.name = PR_SetReturnString (pr, text + clips[clip].name),
						.num_frames = clips[clip].numframes,
						.channel_type = qfm_special,
					};
					found = true;
				}
				clip -= mesh->morph.numclips;
			}
		}

		if (cached) {
			Cache_Release (&mod->cache);
		}
	}
}

bi (Model_GetChannelInfo)
{
	auto res = (rua_model_resources_t *) _res;
	int  handle = P_INT (pr, 0);
	auto h = rua_model_handle_get (res, handle);
	auto mod = h->model;

	R_POINTER (pr) = 0;
	if (mod->type == mod_mesh) {
		bool cached = false;
		auto model = mod->model;
		if (!model) {
			model = Cache_Get (&mod->cache);
			cached = true;
		}
		auto channels = (qfm_channel_t*)((byte*)model + model->channels.offset);
		auto data = (uint16_t *) P_GPOINTER (pr, 1);
		memcpy (data, channels, model->channels.count * sizeof (qfm_channel_t));

		if (cached) {
			Cache_Release (&mod->cache);
		}
	}
}

bi (Model_GetFrameData)
{
	auto res = (rua_model_resources_t *) _res;
	int  handle = P_INT (pr, 0);
	auto h = rua_model_handle_get (res, handle);
	auto mod = h->model;
	unsigned clip = P_UINT (pr, 1);

	R_POINTER (pr) = 0;
	if (mod->type == mod_mesh) {
		bool cached = false;
		auto model = mod->model;
		if (!model) {
			model = Cache_Get (&mod->cache);
			cached = true;
		}
		auto clips = (clipdesc_t*)((byte*)model + model->anim.clips);
		auto keyframes = (keyframe_t*)((byte*)model + model->anim.keyframes);
		if (clip < model->anim.numclips) {
			uint32_t numchannels = model->channels.count;
			uint32_t numframes = clips[clip].numframes;
			uint32_t frame = keyframes[clips[clip].firstframe].data;
			auto data = (uint16_t *) P_GPOINTER (pr, 2);
			auto f = (uint16_t *) ((byte *) model + frame);
			memcpy (data, f, numchannels * numframes * sizeof (uint16_t));
		} else {
			// no chanels
		}

		if (cached) {
			Cache_Release (&mod->cache);
		}
	}
}

bi (qfa_find_clip)
{
	const char *name = P_GSTRING (pr, 0);
	R_INT (pr) = qfa_find_clip (name);
}

bi (qfa_find_armature)
{
	const char *name = P_GSTRING (pr, 0);
	R_INT (pr) = qfa_find_armature (name);
}

bi (qfa_create_animation)
{
	auto res = (rua_model_resources_t *) _res;
	uint32_t   *clips = &P_STRUCT (pr, uint32_t, 0);
	uint32_t    num_clips = P_UINT (pr, 1);
	uint32_t    armature = P_UINT (pr, 2);
	int         model_handle = P_INT (pr, 3);
	model_t    *mod = nullptr;
	qf_model_t *model = nullptr;
	bool        cached = false;

	if (model_handle) {
		auto h = rua_model_handle_get (res, model_handle);
		mod = h->model;
	}
	if (mod) {
		model = mod->model;
		if (!model) {
			model = Cache_Get (&mod->cache);
			cached = true;
		}
	}

	auto anim = qfa_create_animation (clips, num_clips, armature, model);
	R_INT (pr) = alloc_animstate (res, anim);

	if (cached) {
		Cache_Release (&mod->cache);
	}
}

bi (qfa_free_animation)
{
	auto res = (rua_model_resources_t *) _res;
	auto rua_anim = rua_animstate_get (res, P_INT (pr, 0));

	qfa_free_animation (rua_anim->animstate);
	*rua_anim->prev = rua_anim->next;
	if (rua_anim->next) {
		rua_anim->next->prev = rua_anim->prev;
	}
	rua_animstate_free (res, rua_anim);
}

bi (qfa_update_anim)
{
	auto res = (rua_model_resources_t *) _res;
	auto rua_anim = rua_animstate_get (res, P_INT (pr, 0));
	qfa_update_anim (rua_anim->animstate, P_FLOAT (pr, 1));
}

bi (qfa_reset_anim)
{
	auto res = (rua_model_resources_t *) _res;
	auto rua_anim = rua_animstate_get (res, P_INT (pr, 0));
	qfa_reset_anim (rua_anim->animstate);
}

bi (qfa_set_clip_weight)
{
	auto res = (rua_model_resources_t *) _res;
	auto rua_anim = rua_animstate_get (res, P_INT (pr, 0));
	uint32_t clip = P_UINT (pr, 1);
	float weight = P_FLOAT (pr, 2);

	auto clip_states = qfa_clip_states (rua_anim->animstate);
	if (clip < rua_anim->animstate->clip_states.count) {
		clip_states[clip].weight = weight;
	}
}

bi (qfa_set_clip_loop)
{
	auto res = (rua_model_resources_t *) _res;
	auto rua_anim = rua_animstate_get (res, P_INT (pr, 0));
	uint32_t clip = P_UINT (pr, 1);
	bool loop = P_INT (pr, 2);

	auto clip_states = qfa_clip_states (rua_anim->animstate);
	if (clip < rua_anim->animstate->clip_states.count) {
		clip_states[clip].flags &= ~qfc_loop;
		if (loop) {
			clip_states[clip].flags |= qfc_loop;
		}
	}
}

bi (qfa_set_clip_disabled)
{
	auto res = (rua_model_resources_t *) _res;
	auto rua_anim = rua_animstate_get (res, P_INT (pr, 0));
	uint32_t clip = P_UINT (pr, 1);
	bool disabled = P_INT (pr, 2);

	auto clip_states = qfa_clip_states (rua_anim->animstate);
	if (clip < rua_anim->animstate->clip_states.count) {
		clip_states[clip].flags &= ~qfc_disabled;
		if (disabled) {
			clip_states[clip].flags |= qfc_disabled;
		}
	}
}

bi (qfa_get_pose_motors)
{
	auto res = (rua_model_resources_t *) _res;
	auto rua_anim = rua_animstate_get (res, P_INT (pr, 0));
	auto anim = rua_anim->animstate;

	auto pose = (qfm_motor_t *) P_GPOINTER (pr, 1);
	auto motors = qfa_matrix_palette (anim);
	memcpy (pose, motors, anim->num_joints * sizeof (pose[0]));
}

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Model_Load,             1, p(string)),
	bi(Model_Unload,           1, p(int)),
	bi(Model_NumJoints,        1, p(int)),
	bi(Model_GetJoints,        2, p(int), p(ptr)),
	bi(Model_NumClips,         1, p(int)),
	bi(Model_GetInverseMotors, 2, p(int), p(ptr)),
	bi(Model_GetClipInfo,      2, p(int), p(uint)),
	bi(Model_GetChannelInfo,   2, p(int), p(ptr)),
	bi(Model_GetFrameData,     3, p(int), p(uint), p(ptr)),

	bi(qfa_find_clip,          1, p(string)),
	bi(qfa_find_armature,      1, p(string)),
	bi(qfa_create_animation,   4, p(ptr), p(uint), p(uint), p(int)),
	bi(qfa_free_animation,     1, p(int)),
	bi(qfa_update_anim,        2, p(int), p(float)),
	bi(qfa_reset_anim,         2, p(int)),
	bi(qfa_set_clip_weight,    2, p(int), p(float)),
	bi(qfa_set_clip_loop,      2, p(int), p(int)),
	bi(qfa_set_clip_disabled,  2, p(int), p(int)),
	bi(qfa_get_pose_motors,    2, p(int), p(ptr)),

	{0}
};

void
RUA_Model_Init (progs_t *pr, int secure)
{
	qfZoneScoped (true);
	rua_model_resources_t *res = calloc (sizeof (rua_model_resources_t), 1);
	res->pr = pr;

	PR_Resources_Register (pr, "Model", res, bi_rua_model_clear,
						   bi_rua_model_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
