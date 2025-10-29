/*
	efrags.h

	Entity fragment management

	Copyright (C) 2021-2025 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2025/10/28

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

#ifndef __QF_scene_efrags_h
#define __QF_scene_efrags_h

/** \defgroup scene_efrags Entity fragment management
	\ingroup scene
*/
///@{

#include "QF/ecs.h"

typedef struct entity_s entity_t;

typedef struct efrag_s {
	uint32_t    entity;
	uint32_t    queue_num;
} efrag_t;

typedef struct cluster_ref_s {
	uint32_t    id;
	uint32_t    index;
} cluster_ref_t;

typedef struct cluster_frags_s {
	uint32_t    num_efrags;
	uint32_t    max_efrags;
	efrag_t    *efrags;
	cluster_ref_t *group_refs;
} cluster_frags_t;

typedef struct cluster_group_s {
	uint32_t    num_refs;
	uint32_t    max_refs;
	cluster_ref_t *refs;
} cluster_group_t;

typedef struct efrag_db_s {
	ecs_idpool_t idpool;
	ecs_pool_t  groups;			// cluster_group_t
	int         num_clusters;
	cluster_frags_t *clusters;
} efrag_db_t;

typedef struct scene_s scene_t;
typedef struct mleaf_s mleaf_t;
uint32_t R_LinkEfrag (scene_t *scene, mleaf_t *leaf, entity_t ent,
					  uint32_t queue, uint32_t efrag);
void R_AddEfrags (scene_t *scene, entity_t ent);
void R_StoreEfrags (scene_t *scene, mleaf_t *leaf);

void Efrags_DelEfrag (efrag_db_t *db, uint32_t efragid);
void Efrags_InitDB (efrag_db_t *db, int num_clusters);
void Efrags_ClearDB (efrag_db_t *db);


///@}

#endif//__QF_scene_efrags_h
