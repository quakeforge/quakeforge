/*
	bi_scene.c

	Ruamoko scene builtins

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

#include "QF/cmem.h"
#include "QF/hash.h"
#include "QF/model.h"
#include "QF/progs.h"
#include "QF/render.h"

#include "QF/plugin/vid_render.h"

#include "QF/scene/entity.h"
#include "QF/scene/light.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

#include "rua_internal.h"

typedef struct rua_scene_s {
	struct rua_scene_s *next;
	struct rua_scene_s **prev;
	scene_t    *scene;
} rua_scene_t;

typedef struct rua_lighting_s {
	struct rua_lighting_s *next;
	struct rua_lighting_s **prev;
	lightingdata_t *ldata;
} rua_lighting_t;

typedef struct rua_scene_resources_s {
	progs_t    *pr;
	PR_RESMAP (rua_scene_t) scene_map;
	PR_RESMAP (rua_lighting_t) lighting_map;
	rua_scene_t *scenes;
	rua_lighting_t *ldatas;
} rua_scene_resources_t;

static rua_scene_t *
rua_scene_new (rua_scene_resources_t *res)
{
	return PR_RESNEW (res->scene_map);
}

static void
rua_scene_free (rua_scene_resources_t *res, rua_scene_t *scene)
{
	if (scene->next) {
		scene->next->prev = scene->prev;
	}
	*scene->prev = scene->next;
	scene->prev = 0;
	PR_RESFREE (res->scene_map, scene);
}

static rua_scene_t * __attribute__((pure))
rua__scene_get (rua_scene_resources_t *res, pr_ulong_t id, const char *name)
{
	rua_scene_t *scene = 0;

	id &= 0xffffffffu;
	scene = PR_RESGET (res->scene_map, (pr_int_t) id);

	// scene->prev will be null if the handle is unallocated
	if (!scene || !scene->prev) {
		PR_RunError (res->pr, "invalid scene passed to %s", name + 3);
	}
	return scene;
}
#define rua_scene_get(res, id) rua__scene_get(res, id, __FUNCTION__)

static int __attribute__((pure))
rua_scene_index (rua_scene_resources_t *res, rua_scene_t *scene)
{
	return PR_RESINDEX (res->scene_map, scene);
}

static entity_t __attribute__((pure))
rua__entity_get (rua_scene_resources_t *res, pr_ulong_t id, const char *name)
{
	pr_ulong_t  scene_id = id & 0xffffffff;
	entity_t    ent = nullentity;

	rua_scene_t *scene = rua__scene_get (res, scene_id, name);
	if (scene) {
		pr_int_t     entity_id = id >> 32;
		ent.id = entity_id;
		ent.reg = scene->scene->reg;
	}

	if (!Entity_Valid (ent)) {
		PR_RunError (res->pr, "invalid entity passed to %s", name + 3);
	}
	return ent;
}
#define rua_entity_get(res, id) rua__entity_get(res, id, __FUNCTION__)

static transform_t  __attribute__((pure))
rua__transform_get (rua_scene_resources_t *res, pr_ulong_t id, const char *name)
{
	pr_ulong_t  scene_id = id & 0xffffffff;
	transform_t transform = nulltransform;

	rua_scene_t *scene = rua_scene_get (res, scene_id);
	if (scene) {
		entity_t    transform_id = { .reg = scene->scene->reg, .id = id >> 32 };
		transform = Entity_Transform (transform_id);
	}

	if (!Transform_Valid (transform)) {
		PR_RunError (res->pr, "invalid transform passed to %s", name + 3);
	}
	return transform;
}
#define rua_transform_get(res, id) rua__transform_get(res, id, __FUNCTION__)

static rua_lighting_t *
rua_lighting_new (rua_scene_resources_t *res)
{
	return PR_RESNEW (res->lighting_map);
}

static void
rua_lighting_free (rua_scene_resources_t *res, rua_lighting_t *ldata)
{
	if (ldata->next) {
		ldata->next->prev = ldata->prev;
	}
	*ldata->prev = ldata->next;
	ldata->prev = 0;
	PR_RESFREE (res->lighting_map, ldata);
}

static rua_lighting_t * __attribute__((pure))
rua__lighting_get (rua_scene_resources_t *res, pr_ulong_t id, const char *name)
{
	rua_lighting_t *ldata = 0;

	if (id <= 0xffffffffu) {
		ldata = PR_RESGET (res->lighting_map, (pr_int_t) id);
	}

	// ldata->prev will be null if the handle is unallocated
	if (!ldata || !ldata->prev) {
		PR_RunError (res->pr, "invalid lighting passed to %s", name + 3);
	}
	return ldata;
}
#define rua_lighting_get(res, id) rua__lighting_get(res, id, __FUNCTION__)

static int __attribute__((pure))
rua_lighting_index (rua_scene_resources_t *res, rua_lighting_t *ldata)
{
	return PR_RESINDEX (res->lighting_map, ldata);
}

#define MAKE_ID(id, sc_id) ((((pr_ulong_t) (id)) << 32) \
							| ((sc_id) & 0xffffffff))

static void
bi_Scene_NewScene (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;

	rua_scene_t *scene = rua_scene_new (res);

	scene->scene = Scene_NewScene (0);

	scene->next = res->scenes;
	if (res->scenes) {
		res->scenes->prev = &scene->next;
	}
	scene->prev = &res->scenes;
	res->scenes = scene;

	// scene id in lower 32-bits for all handles
	// zero upper 32-bits zero means scene, otherwise transform or entity
	R_ULONG (pr) = MAKE_ID (0, rua_scene_index (res, scene));
}

static void
rua_delete_scene (rua_scene_resources_t *res, rua_scene_t *scene)
{
	Scene_DeleteScene (scene->scene);
	rua_scene_free (res, scene);
}

static void
bi_Scene_DeleteScene (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	rua_scene_t *scene = rua_scene_get (res, P_ULONG (pr, 0));

	rua_delete_scene (res, scene);
}

scene_t *
Scene_GetScene (progs_t *pr, pr_ulong_t handle)
{
	if (!handle) {
		return 0;
	}
	rua_scene_resources_t *res = PR_Resources_Find (pr, "Scene");
	rua_scene_t *scene = rua_scene_get (res, P_ULONG (pr, 0));
	return scene->scene;
}

static void
bi_Scene_CreateEntity (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	pr_ulong_t   scene_id = P_ULONG (pr, 0);
	rua_scene_t *scene = rua_scene_get (res, scene_id);
	entity_t    ent = Scene_CreateEntity (scene->scene);
	R_ULONG (pr) = MAKE_ID (ent.id, scene_id);
}

static void
bi_Scene_DestroyEntity (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	pr_ulong_t  id = P_ULONG (pr, 0);
	entity_t    ent = rua_entity_get (res, id);
	pr_ulong_t  scene_id = id & 0xffffffff;
	rua_scene_t *scene = rua_scene_get (res, scene_id);

	// bad scene caught above
	Scene_DestroyEntity (scene->scene, ent);
}

static void
bi_Scene_SetLighting (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	pr_ulong_t   scene_id = P_ULONG (pr, 0);
	rua_scene_t *scene = rua_scene_get (res, scene_id);
	pr_ulong_t   ldata_id = P_ULONG (pr, 1);
	rua_lighting_t *ldata = 0;

	if (ldata_id) {
		ldata = rua_lighting_get (res, ldata_id);
	}

	scene->scene->lights = ldata->ldata;
}

static void
bi_Entity_GetTransform (progs_t *pr, void *_res)
{
	pr_ulong_t  ent_id = P_ULONG (pr, 0);

	// ent_id is used to fetch the transform every time
	R_ULONG (pr) = ent_id;
}

static void
bi_Entity_SetModel (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	pr_ulong_t  ent_id = P_ULONG (pr, 0);
	pr_int_t    model_id = P_INT (pr, 1);
	entity_t    ent = rua_entity_get (res, ent_id);
	model_t    *model = Model_GetModel (pr, model_id);
	pr_ulong_t  scene_id = ent_id & 0xffffffff;
	// bad scene caught above
	rua_scene_t *scene = rua_scene_get (res, scene_id);

	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	renderer->model = model;
	R_AddEfrags (&scene->scene->worldmodel->brush, ent);
}

static void
bi_Transform_ChildCount (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));

	R_UINT (pr) = Transform_ChildCount (transform);
}

static void
bi_Transform_GetChild (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	pr_ulong_t  transform_id = P_ULONG (pr, 0);
	transform_t transform = rua_transform_get (res, transform_id);
	transform_t child = Transform_GetChild (transform, P_UINT (pr, 2));
	R_ULONG (pr) = Transform_Valid (child) ? MAKE_ID (child.id, transform_id)
										   : 0;
}

static void
bi_Transform_SetParent (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	transform_t parent = rua_transform_get (res, P_ULONG (pr, 1));

	Transform_SetParent (transform, parent);
}

static void
bi_Transform_GetParent (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	pr_ulong_t  transform_id = P_ULONG (pr, 0);
	transform_t transform = rua_transform_get (res, transform_id);
	transform_t parent = Transform_GetParent (transform);

	// transform_id contains scene id
	R_ULONG (pr) = Transform_Valid (parent) ? MAKE_ID (parent.id, transform_id)
											: 0;
}

static void
bi_Transform_SetTag (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	pr_uint_t   tag = P_UINT (pr, 2);
	Transform_SetTag (transform, tag);
}

static void
bi_Transform_GetTag (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));

	R_UINT (pr) = Transform_GetTag (transform);
}

static void
bi_Transform_GetLocalMatrix (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	Transform_GetLocalMatrix (transform, &R_PACKED (pr, pr_vec4_t));
}

static void
bi_Transform_GetLocalInverse (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	Transform_GetLocalInverse (transform, &R_PACKED (pr, pr_vec4_t));
}

static void
bi_Transform_GetWorldMatrix (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	Transform_GetWorldMatrix (transform, &R_PACKED (pr, pr_vec4_t));
}

static void
bi_Transform_GetWorldInverse (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	Transform_GetWorldInverse (transform, &R_PACKED (pr, pr_vec4_t));
}

static void
bi_Transform_SetLocalPosition (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	Transform_SetLocalPosition (transform, P_PACKED (pr, pr_vec4_t, 1));
}

static void
bi_Transform_GetLocalPosition (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	R_PACKED (pr, pr_vec4_t) = Transform_GetLocalPosition (transform);
}

static void
bi_Transform_SetLocalRotation (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	Transform_SetLocalRotation (transform, P_PACKED (pr, pr_vec4_t, 1));
}

static void
bi_Transform_GetLocalRotation (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	R_PACKED (pr, pr_vec4_t) = Transform_GetLocalRotation (transform);
}

static void
bi_Transform_SetLocalScale (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	Transform_SetLocalScale (transform, P_PACKED (pr, pr_vec4_t, 1));
}

static void
bi_Transform_GetLocalScale (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	R_PACKED (pr, pr_vec4_t) = Transform_GetLocalScale (transform);
}

static void
bi_Transform_SetWorldPosition (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	Transform_SetWorldPosition (transform, P_PACKED (pr, pr_vec4_t, 1));
}

static void
bi_Transform_GetWorldPosition (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	R_PACKED (pr, pr_vec4_t) = Transform_GetWorldPosition (transform);
}

static void
bi_Transform_SetWorldRotation (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	Transform_SetWorldRotation (transform, P_PACKED (pr, pr_vec4_t, 1));
}

static void
bi_Transform_GetWorldRotation (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	R_PACKED (pr, pr_vec4_t) = Transform_GetWorldRotation (transform);
}

static void
bi_Transform_GetWorldScale (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	R_PACKED (pr, pr_vec4_t) = Transform_GetWorldScale (transform);
}

static void
bi_Transform_SetLocalTransform (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	Transform_SetLocalTransform (transform, P_PACKED (pr, pr_vec4_t, 1),
			P_PACKED (pr, pr_vec4_t, 2), P_PACKED (pr, pr_vec4_t, 3));
}

static void
bi_Transform_Forward (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	R_PACKED (pr, pr_vec4_t) = Transform_Forward (transform);
}

static void
bi_Transform_Right (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	R_PACKED (pr, pr_vec4_t) = Transform_Right (transform);
}

static void
bi_Transform_Up (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	transform_t transform = rua_transform_get (res, P_ULONG (pr, 0));
	R_PACKED (pr, pr_vec4_t) = Transform_Up (transform);
}

static void
bi_Light_CreateLightingData (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	pr_ulong_t   scene_id = P_ULONG (pr, 0);
	rua_scene_t *scene = rua_scene_get (res, scene_id);

	rua_lighting_t *ldata = rua_lighting_new (res);

	ldata->ldata = Light_CreateLightingData (scene->scene);

	ldata->next = res->ldatas;
	if (res->ldatas) {
		res->ldatas->prev = &ldata->next;
	}
	ldata->prev = &res->ldatas;
	res->ldatas = ldata;

	// ldata id in lower 32-bits for all handles
	// upper 32-bits reserved
	R_ULONG (pr) = MAKE_ID (0, rua_lighting_index (res, ldata));
}

static void
rua_delete_lighting (rua_scene_resources_t *res, rua_lighting_t *ldata)
{
	Light_DestroyLightingData (ldata->ldata);
	rua_lighting_free (res, ldata);
}

static void
bi_Light_DestroyLightingData (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	rua_lighting_t *ldata = rua_lighting_get (res, P_ULONG (pr, 0));

	rua_delete_lighting (res, ldata);
}

static void
bi_Light_ClearLights (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	rua_lighting_t *ldata = rua_lighting_get (res, P_ULONG (pr, 0));

	Light_ClearLights (ldata->ldata);
}

static void
bi_Light_AddLight (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	rua_lighting_t *ldata = rua_lighting_get (res, P_ULONG (pr, 0));
	light_t    *light = &P_PACKED (pr, light_t, 1);
	int         style = P_INT (pr, 5);

	Light_AddLight (ldata->ldata, light, style);
}

static void
bi_Light_EnableSun (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	rua_lighting_t *ldata = rua_lighting_get (res, P_ULONG (pr, 0));

	Light_EnableSun (ldata->ldata);
}

#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
static builtin_t builtins[] = {
	bi(Scene_NewScene,              0),
	bi(Scene_DeleteScene,           1, p(ulong)),
	bi(Scene_CreateEntity,          1, p(ulong)),
	bi(Scene_DestroyEntity,         1, p(ulong)),
	bi(Scene_SetLighting  ,         2, p(ulong), p(ulong)),

	bi(Entity_GetTransform,         1, p(ulong)),
	bi(Entity_SetModel,             2, p(ulong), p(int)),

	bi(Transform_ChildCount,        1, p(ulong)),
	bi(Transform_GetChild,          2, p(ulong), p(int)),
	bi(Transform_SetParent,         2, p(ulong), p(ulong)),
	bi(Transform_GetParent,         1, p(ulong)),

	bi(Transform_SetTag,            2, p(ulong), p(uint)),
	bi(Transform_GetTag,            1, p(ulong)),

	bi(Transform_GetLocalMatrix,    1, p(ulong)),
	bi(Transform_GetLocalInverse,   1, p(ulong)),
	bi(Transform_GetWorldMatrix,    1, p(ulong)),
	bi(Transform_GetWorldInverse,   1, p(ulong)),

	bi(Transform_SetLocalPosition,  2, p(ulong), p(vec4)),
	bi(Transform_GetLocalPosition,  1, p(ulong)),
	bi(Transform_SetLocalRotation,  2, p(ulong), p(vec4)),
	bi(Transform_GetLocalRotation,  1, p(ulong)),
	bi(Transform_SetLocalScale,     2, p(ulong), p(vec4)),
	bi(Transform_GetLocalScale,     1, p(ulong)),

	bi(Transform_SetWorldPosition,  2, p(ulong), p(vec4)),
	bi(Transform_GetWorldPosition,  1, p(ulong)),
	bi(Transform_SetWorldRotation,  2, p(ulong), p(vec4)),
	bi(Transform_GetWorldRotation,  1, p(ulong)),
	bi(Transform_GetWorldScale,     1, p(ulong)),

	bi(Transform_SetLocalTransform, 4, p(ulong), p(vec4), p(vec4), p(vec4)),
	bi(Transform_Forward,           1, p(ulong)),
	bi(Transform_Right,             1, p(ulong)),
	bi(Transform_Up,                1, p(ulong)),

	bi(Light_CreateLightingData,    1, p(ulong)),
	bi(Light_DestroyLightingData,   1, p(ulong)),
	bi(Light_ClearLights,           1, p(ulong)),
	bi(Light_AddLight,              5, p(ulong),// really, 3
				p(vec4), p(vec4), p(vec4), p(vec4), p(int)),
	bi(Light_EnableSun,             1, p(ulong)),

	{0}
};

static void
bi_scene_clear (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;

	while (res->ldatas) {
		rua_delete_lighting (res, res->ldatas);
	}
	while (res->scenes) {
		rua_delete_scene (res, res->scenes);
	}
}

static void
bi_scene_destroy (progs_t *pr, void *_res)
{
	rua_scene_resources_t *res = _res;
	PR_RESDELMAP (res->scene_map);
	PR_RESDELMAP (res->lighting_map);
	free (res);
}

void
RUA_Scene_Init (progs_t *pr, int secure)
{
	rua_scene_resources_t *res = calloc (sizeof (rua_scene_resources_t), 1);

	res->pr = pr;

	PR_Resources_Register (pr, "Scene", res, bi_scene_clear, bi_scene_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
