/*
	r_tent.c

	rendering entities

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <math.h>
#include <stdlib.h>

#include "QF/model.h"
#include "QF/msg.h"
#include "QF/render.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "r_internal.h"

#define ENT_POOL_SIZE 32

typedef struct entity_pool_s {
	struct entity_pool_s *next;
	entity_t    entities[ENT_POOL_SIZE];
} entity_pool_t;

entity_t   *r_ent_queue;
static entity_t **vis_tail = &r_ent_queue;

static entity_pool_t *entity_pools = 0;
static entity_pool_t **entpool_tail = &entity_pools;
static entity_t *free_entities;

entity_t *
R_AllocEntity (void)
{
	entity_pool_t *pool;
	entity_t   *ent;
	int         i;

	if ((ent = free_entities)) {
		free_entities = ent->next;
		ent->next = 0;
		ent->transform = 0;
		return ent;
	}

	pool = malloc (sizeof (entity_pool_t));
	pool->next = 0;
	*entpool_tail = pool;
	entpool_tail = &pool->next;

	for (ent = pool->entities, i = 0; i < ENT_POOL_SIZE - 1; i++, ent++) {
		ent->next = ent + 1;
		ent->transform = 0;
	}
	ent->next = 0;
	ent->transform = 0;
	free_entities = pool->entities;

	return R_AllocEntity ();
}

void
R_FreeAllEntities (void)
{
	entity_pool_t *pool;
	entity_t   *ent;
	int         i;

	for (pool = entity_pools; pool; pool = pool->next) {
		for (ent = pool->entities, i = 0; i < ENT_POOL_SIZE - 1; i++, ent++) {
			ent->next = ent + 1;
			if (ent->transform) {
				Transform_Delete (ent->transform);
				ent->transform = 0;
			}
		}
		ent->next = pool->next ? pool->next->entities : 0;
		if (ent->transform) {
			Transform_Delete (ent->transform);
			ent->transform = 0;
		}
	}
	free_entities = entity_pools ? entity_pools->entities : 0;
}

void
R_ClearEnts (void)
{
	r_ent_queue = 0;
	vis_tail = &r_ent_queue;
}

void
R_EnqueueEntity (entity_t *ent)
{
	ent->next = 0;
	*vis_tail = ent;
	vis_tail = &ent->next;
}

float
R_EntityBlend (animation_t *animation, int pose, float interval)
{
	float       blend;

	if (animation->nolerp) {
		animation->nolerp = 0;
		animation->pose1 = pose;
		animation->pose2 = pose;
		return 0.0;
	}
	animation->frame_interval = interval;
	if (animation->pose2 != pose) {
		animation->frame_start_time = vr_data.realtime;
		if (animation->pose2 == -1) {
			animation->pose1 = pose;
		} else {
			animation->pose1 = animation->pose2;
		}
		animation->pose2 = pose;
		blend = 0.0;
	} else if (vr_data.paused) {
		blend = 1.0;
	} else {
		blend = (vr_data.realtime - animation->frame_start_time)
				/ animation->frame_interval;
		blend = min (blend, 1.0);
	}
	return blend;
}
