/*
	entity.h

	Entity management

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/02/26

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

#ifndef __QF_scene_entity_h
#define __QF_scene_entity_h

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/set.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

/** \defgroup scene_entity Entity management
	\ingroup scene
*/
///@{

#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

typedef struct entity_s {
	ecs_registry_t *reg;
	uint32_t    id;
	uint32_t    base;
} entity_t;

#define nullentity ((entity_t) { .id = nullent })

typedef struct animation_s {
	int         frame;
	float       syncbase;	// randomize time base for local animations
	float       frame_start_time;
	float       frame_interval;
	int         pose1;
	int         pose2;
	float       blend;
	int         nolerp;		// don't lerp this frame (pose data invalid)
} animation_t;

typedef struct visibility_s {
	struct efrag_s *efrag;		// linked list of efrags
	int         topnode_id;		// bmodels, first world node that
								// splits bmodel, or NULL if not split
								// applies to other models, too
								// found in an active leaf
	int         trivial_accept;	// view clipping (frustum and depth)
} visibility_t;

typedef struct renderer_s {
	struct model_s *model;			// NULL = no model
	uint32_t    skin;				//FIXME these two likely to be confusing
	uint32_t    skindesc;			//FIXME ^^
	struct trail_s *trail;
	unsigned    fullbright:1;
	unsigned    noshadows:1;
	unsigned    onlyshadows:1;
	unsigned    depthhack:1;
	float       colormod[4];	// color tint and alpha for model
	int         skinnum;		// for Alias models
	float       min_light;
	int         render_id;
} renderer_t;

typedef struct entityset_s DARRAY_TYPE (entity_t) entityset_t;

typedef struct efrag_s {
	struct mleaf_s *leaf;
	struct efrag_s *leafnext;
	entity_t    entity;
	uint32_t    queue_num;
	struct efrag_s *entnext;
} efrag_t;

typedef struct entqueue_s {
	set_t      *queued_ents;
	entityset_t *ent_queues;
	int         num_queues;
} entqueue_t;

typedef struct colormap_s {
	byte        top;
	byte        bottom;
} colormap_t;

#define ENTINLINE GNU89INLINE inline

entqueue_t *EntQueue_New (int num_queues);
void EntQueue_Delete (entqueue_t *queue);
ENTINLINE void EntQueue_AddEntity (entqueue_t *queue, entity_t ent,
								   int queue_num);
void EntQueue_Clear (entqueue_t *queue);
ENTINLINE int Entity_Valid (entity_t ent);
ENTINLINE transform_t Entity_Transform (entity_t ent);
ENTINLINE colormap_t *Entity_GetColormap (entity_t ent);
ENTINLINE void Entity_SetColormap (entity_t ent, colormap_t *colormap);
ENTINLINE void Entity_RemoveColormap (entity_t ent);
ENTINLINE animation_t *Entity_GetAnimation (entity_t ent);
ENTINLINE void Entity_SetAnimation (entity_t ent, animation_t *animation);
ENTINLINE renderer_t *Entity_GetRenderer (entity_t ent);
ENTINLINE void Entity_SetRenderer (entity_t ent, renderer_t *renderer);

#undef ENTINLINE
#ifndef IMPLEMENT_ENTITY_Funcs
#define ENTINLINE GNU89INLINE inline
#else
#define ENTINLINE VISIBLE
#endif

ENTINLINE
void
EntQueue_AddEntity (entqueue_t *queue, entity_t ent, int queue_num)
{
	int         id = Ent_Index (ent.id);
	if (!set_is_member (queue->queued_ents, id)) {
		// entity ids are negative (ones-complement)
		set_add (queue->queued_ents, id);
		DARRAY_APPEND (&queue->ent_queues[queue_num], ent);
	}
}

ENTINLINE
int
Entity_Valid (entity_t ent)
{
	return ent.reg && ECS_EntValid (ent.id, ent.reg);
}

ENTINLINE
transform_t
Entity_Transform (entity_t ent)
{
	// The transform hierarchy reference is a component on the entity thus
	// the entity id is the transform id.
	return (transform_t) {
		.reg = ent.reg,
		.id = ent.id,
		.comp = ent.base + scene_href,
	};
}

ENTINLINE
colormap_t *
Entity_GetColormap (entity_t ent)
{
	if (Ent_HasComponent (ent.id, ent.base + scene_colormap, ent.reg)) {
		return Ent_GetComponent (ent.id, ent.base + scene_colormap, ent.reg);
	}
	return nullptr;
}

ENTINLINE
void
Entity_SetColormap (entity_t ent, colormap_t *colormap)
{
	Ent_SetComponent (ent.id, ent.base + scene_colormap, ent.reg, colormap);
}

ENTINLINE
void
Entity_RemoveColormap (entity_t ent)
{
	return Ent_RemoveComponent (ent.id, ent.base + scene_colormap, ent.reg);
}

ENTINLINE
animation_t *
Entity_GetAnimation (entity_t ent)
{
	if (Ent_HasComponent (ent.id, ent.base + scene_animation, ent.reg)) {
		return Ent_GetComponent (ent.id, ent.base + scene_animation, ent.reg);
	}
	return nullptr;
}

ENTINLINE
void
Entity_SetAnimation (entity_t ent, animation_t *animation)
{
	Ent_SetComponent (ent.id, ent.base + scene_animation, ent.reg, animation);
}

ENTINLINE
renderer_t *
Entity_GetRenderer (entity_t ent)
{
	if (Ent_HasComponent (ent.id, ent.base + scene_renderer, ent.reg)) {
		return Ent_GetComponent (ent.id, ent.base + scene_renderer, ent.reg);
	}
	return nullptr;
}

ENTINLINE
void
Entity_SetRenderer (entity_t ent, renderer_t *renderer)
{
	Ent_SetComponent (ent.id, ent.base + scene_renderer, ent.reg, renderer);
}

struct mod_brush_s;
efrag_t **R_LinkEfrag (struct mleaf_s *leaf, entity_t ent, uint32_t queue,
					   efrag_t **lastlink);
void R_AddEfrags (struct mod_brush_s *, entity_t ent);
void R_ShutdownEfrags (void);
void R_ClearEfragChain (efrag_t *ef);

typedef struct ecs_pool_s ecs_pool_t;
void Anim_Update (double time, const ecs_pool_t *animpool,
				  const ecs_pool_t *rendpool);


///@}

#endif//__QF_scene_entity_h
