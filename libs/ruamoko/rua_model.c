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

#include "QF/model.h"
#include "QF/progs.h"
#include "QF/qfmodel.h"

#include "rua_internal.h"

typedef struct rua_model_s {
	struct rua_model_s *next;
	struct rua_model_s **prev;
	model_t    *model;
} rua_model_t;

typedef struct {
	PR_RESMAP (rua_model_t) model_map;
	rua_model_t *handles;
	progs_t    *pr;
} rua_model_resources_t;

static rua_model_t *
rua_model_handle_new (rua_model_resources_t *res)
{
	qfZoneScoped (true);
	return PR_RESNEW (res->model_map);
}

static void
rua_model_handle_free (rua_model_resources_t *res, rua_model_t *handle)
{
	qfZoneScoped (true);
	PR_RESFREE (res->model_map, handle);
}

static void
rua_model_handle_reset (rua_model_resources_t *res)
{
	qfZoneScoped (true);
	PR_RESRESET (res->model_map);
}

static inline rua_model_t * __attribute__((pure))
rua__model_handle_get (rua_model_resources_t *res, int index, const char *name)
{
	qfZoneScoped (true);
	rua_model_t *handle = 0;

	if (index) {
		handle = PR_RESGET(res->model_map, index);
	}
	if (!handle) {
		PR_RunError (res->pr, "invalid model handle passed to %s", name + 3);
	}
	return handle;
}
#define rua_model_handle_get(res, index) \
	rua__model_handle_get (res, index, __FUNCTION__)

static inline int __attribute__((pure))
rua_model_handle_index (rua_model_resources_t *res, rua_model_t *handle)
{
	qfZoneScoped (true);
	return PR_RESINDEX(res->model_map, handle);
}

static void
bi_rua_model_clear (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	rua_model_resources_t *res = (rua_model_resources_t *) _res;
	rua_model_t    *handle;

	for (handle = res->handles; handle; handle = handle->next)
		Mod_UnloadModel (handle->model);
	res->handles = 0;
	rua_model_handle_reset (res);
}

static void
bi_rua_model_destroy (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	rua_model_resources_t *res = _res;
	PR_RESDELMAP (res->model_map);
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

bi (Model_NumFrames)
{
	auto res = (rua_model_resources_t *) _res;
	int  handle = P_INT (pr, 0);
	auto h = rua_model_handle_get (res, handle);
	auto mod = h->model;

	R_INT (pr) = 0;
	if (mod->type == mod_mesh) {
		bool cached = false;
		auto model = mod->model;
		if (!model) {
			model = Cache_Get (&mod->cache);
			cached = true;
		}
		if (model->anim.numdesc) {
			R_INT (pr) = model->anim.numdesc;
		} else {
			//FIXME assumes only one mesh
			auto mesh = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
			R_INT (pr) = mesh->morph.numdesc;
		}

		if (cached) {
			Cache_Release (&mod->cache);
		}
	}
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

bi (Model_GetBaseMotors)
{
	qfZoneScoped (true);

	auto model = rua_model_get_model (pr, _res);
	if (model) {
		auto motors = (qfm_motor_t *) P_GPOINTER (pr, 1);
		auto m = (qfm_motor_t *) ((byte *) model + model->base.offset);
		memcpy (motors, m, model->base.count * sizeof (motors[0]));

		R_INT (pr) = 1;
	}
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

	R_var (pr, uvec4) = (vec4u_t) {};
	if (mod->type == mod_mesh) {
		bool cached = false;
		auto model = mod->model;
		if (!model) {
			model = Cache_Get (&mod->cache);
			cached = true;
		}
		auto text = (const char *) model + model->text.offset;
		auto clips = (keyframedesc_t*)((byte*)model + model->anim.descriptors);
		if (clip < model->anim.numdesc) {
			R_var (pr, uvec4) = (vec4u_t) {
				PR_SetReturnString(pr, text + clips[clip].name),
				clips[clip].numframes,
				model->channels.count,
				qfm_u16,
			};
		} else {
			// no chanels
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
		auto clips = (keyframedesc_t*)((byte*)model + model->anim.descriptors);
		auto keyframes = (keyframe_t*)((byte*)model + model->anim.keyframes);
		if (clip < model->anim.numdesc) {
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

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Model_Load,             1, p(string)),
	bi(Model_Unload,           1, p(ptr)),
	bi(Model_NumJoints,        1, p(ptr)),
	bi(Model_GetJoints,        1, p(ptr)),
	bi(Model_NumFrames,        1, p(ptr)),
	bi(Model_GetBaseMotors,    2, p(ulong), p(ptr)),
	bi(Model_GetInverseMotors, 2, p(ulong), p(ptr)),
	bi(Model_GetClipInfo,      2, p(ulong), p(uint)),
	bi(Model_GetChannelInfo,   2, p(ulong), p(ptr)),
	bi(Model_GetFrameData,     3, p(ulong), p(uint), p(ptr)),
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
