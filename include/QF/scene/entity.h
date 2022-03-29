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

#include "QF/scene/hierarchy.h"
#include "QF/scene/transform.h"

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
	struct entity_s *entity;	// owning entity
	struct efrag_s *efrag;		// linked list of efrags
	struct mnode_s *topnode;	// bmodels, first world node that
								// splits bmodel, or NULL if not split
								// applies to other models, too
								// found in an active leaf
	int         trivial_accept;	// view clipping (frustum and depth)
} visibility_t;

typedef struct renderer_s {
	struct model_s *model;			// NULL = no model
	struct skin_s *skin;
	float       colormod[4];	// color tint and alpha for model
	int         skinnum;		// for Alias models
	int         fullbright;
	float       min_light;
	mat4_t      full_transform;
} renderer_t;

typedef struct entity_s {
	struct entity_s *next;
	struct transform_s *transform;
	int         id;		///< scene id
	animation_t animation;
	visibility_t visibility;
	renderer_t  renderer;
	int         active;
	//XXX FIXME XXX should not be here
	vec4f_t     old_origin;
} entity_t;

typedef struct entqueue_s {
	set_t      *queued_ents;
	entityset_t *ent_queues;
	int         num_queues;
} entqueue_t;

#define ENTINLINE GNU89INLINE inline

entqueue_t *EntQueue_New (int num_queues);
void EntQueue_Delete (entqueue_t *queue);
ENTINLINE void EntQueue_AddEntity (entqueue_t *queue, entity_t *ent,
								   int queue_num);
void EntQueue_Clear (entqueue_t *queue);

#undef ENTINLINE
#ifndef IMPLEMENT_ENTITY_Funcs
#define ENTINLINE GNU89INLINE inline
#else
#define ENTINLINE VISIBLE
#endif

ENTINLINE
void
EntQueue_AddEntity (entqueue_t *queue, entity_t *ent, int queue_num)
{
	if (!set_is_member (queue->queued_ents, ent->id)) {
		// entity ids are negative (ones-complement)
		set_add (queue->queued_ents, -ent->id);//FIXME use ~
		DARRAY_APPEND (&queue->ent_queues[queue_num], ent);
	}
}
struct mod_brush_s;
void R_AddEfrags (struct mod_brush_s *, entity_t *ent);
void R_RemoveEfrags (entity_t *ent);

///@}

#endif//__QF_scene_entity_h
