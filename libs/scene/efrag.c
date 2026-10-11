/*
	r_efrag.c

	(description)

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

#include <stdlib.h>

#include "QF/render.h"
#include "QF/sys.h"

#include "QF/scene/efrags.h"
#include "QF/scene/entity.h"

#include "qfalloca.h"

typedef struct ef_block_s {
	ef_block_t *next;
} ef_block_t;

static_assert (sizeof (efrag_t) == sizeof (cluster_ref_t));
static_assert (sizeof (efrag_t) >= sizeof (ef_block_t));

/* Efrag database

   Entities in the scene get an efrag id (which is itself an entity id in
   the efrag database). An efrag has a cluster_group_t component, which is
   a list of cluster_ref_t references to the clusters a scene entity
   occupies.

   Each cluster is a cluster_frags_t with a list of efrag_t elements
   holding the scene entity id and its queue type, as well as a list of
   cluster_ref_t references to the reference in the group that refers to
   the efrag (yeah, that's confusing).

   clusters: a b c d e
   entities: 1 2 3 4 5
   groups  : A B C D E
   queues  : B M S L

	clusters: (find entities in an efrag)
	          efrags are entid+queue
			  group_refs are group+index
	    efrags      group_refs
	a  |1B|4L|      |A0|D0|
	b  |2M|3B|4L    |B0|C0|D1|
	c  |4L|         |D2|
	d  |3B|4L|      |C1|D3|
	e  |5S|         |E0|

	NOTE: group ids in group_refs are the efrag id, so need to go through
	the groups.sparse array for lookup.

	groups: (find efrags on an entity)
	        cluster+index
	A  |a0|
	B  |b0|
	C  |b1|d0|
	D  |a1|b2|c0|d1
	E  |e0|
*/

static void
free_block (efrag_db_t *db, ef_block_t *block, uint32_t max)
{
	if (!block) {
		return;
	}
	uint32_t ind = max / 8 - 1;

	block->next = db->free_blocks[ind];
	db->free_blocks[ind] = block;
}

static void
free_efrags (efrag_db_t *db, efrag_t *efrags, uint32_t max)
{
	free_block (db, (ef_block_t *) efrags, max);
}

static void
free_refs (efrag_db_t *db, cluster_ref_t *refs, uint32_t max)
{
	free_block (db, (ef_block_t *) refs, max);
}

static ef_block_t *
realloc_block (efrag_db_t *db, ef_block_t *old,
			   uint32_t old_max, uint32_t new_max)
{
	ef_block_t *new = nullptr;
	uint32_t old_ind = old_max / 8 - 1;
	uint32_t new_ind = new_max / 8 - 1;
	if (new_ind >= db->max_free_blocks) {
		uint32_t new_max_free = new_ind + 1;
		db->free_blocks = realloc (db->free_blocks,
								   sizeof (ef_block_t *[new_max_free]));
		for (uint32_t i = db->max_free_blocks; i < new_max_free; i++) {
			db->free_blocks[i] = nullptr;
		}
		db->max_free_blocks = new_max_free;
		db->num_free_blocks = new_max_free;
	}
	if (!db->free_blocks[new_ind]) {
		db->free_blocks[new_ind] = malloc (sizeof (ef_block_t[new_max]));
		db->free_blocks[new_ind]->next = nullptr;
	}
	new = db->free_blocks[new_ind];
	db->free_blocks[new_ind] = new->next;
	if (old) {
		memcpy (new, old, sizeof (ef_block_t[old_max]));
		old->next = db->free_blocks[old_ind];
		db->free_blocks[old_ind] = old;
	}
	return new;
}

static efrag_t *
realloc_efrags (efrag_db_t *db, efrag_t *old,
				uint32_t old_max, uint32_t new_max)
{
	return (efrag_t *) realloc_block (db, (ef_block_t *) old,
									  old_max, new_max);
}

static cluster_ref_t *
realloc_refs (efrag_db_t *db, cluster_ref_t *old,
			  uint32_t old_max, uint32_t new_max)
{
	return (cluster_ref_t *) realloc_block (db, (ef_block_t *) old,
											old_max, new_max);
}

static void
destroy_cluster_group (void *_grp, ecs_registry_t *reg, uint32_t ent,
					   const component_t *component)
{
	qfZoneScoped (true);
	auto db = (efrag_db_t *) reg;
	cluster_group_t *grp = _grp;
	cluster_group_t *groups = db->groups.data;
	for (uint32_t i = 0; db->clusters && i < grp->num_refs; i++) {
		auto ref = &grp->refs[i];
		auto frags = &db->clusters[ref->id];
		if (ref->index + 1 < frags->num_efrags) {
			uint32_t dst = ref->index;
			uint32_t src = frags->num_efrags - 1;
			frags->efrags[dst] = frags->efrags[src];
			auto grp_ref = &frags->group_refs[dst];
			*grp_ref = frags->group_refs[src];
			uint32_t gind = db->groups.sparse[Ent_Index (grp_ref->id)];
			groups[gind].refs[grp_ref->index].index = dst;
		}
		frags->num_efrags--;
	}
	free_refs (db, grp->refs, grp->max_refs);
}

static component_t cluster_group_component = {
	.size = sizeof (cluster_group_t),
	.destroy = destroy_cluster_group,
};

void
Efrags_InitDB (efrag_db_t *db, int num_clusters)
{
	qfZoneScoped (true);
	*db = (efrag_db_t) {
		.num_clusters = num_clusters,
		.clusters = calloc (num_clusters, sizeof (cluster_frags_t)),
	};
}

void
Efrags_ClearDB (efrag_db_t *db)
{
	qfZoneScoped (true);
	if (db->clusters) {
		for (int i = 0; i < db->num_clusters; i++) {
			auto cluster = &db->clusters[i];
			free_efrags (db, cluster->efrags, cluster->max_efrags);
			free_refs (db, cluster->group_refs, cluster->max_efrags);
		}
	}
	Component_DestroyElements (&cluster_group_component, db->groups.data,
							   0, db->groups.count, db->groups.dense,
							   (ecs_registry_t *) db);
};

void
Efrags_DestroyDB (efrag_db_t *db)
{
	qfZoneScoped (true);
	free (db->clusters);
	for (uint32_t i = 0; i < db->max_free_blocks; i++) {
		while (db->free_blocks[i]) {
			auto tmp = db->free_blocks[i];
			db->free_blocks[i] = tmp->next;
			free (tmp);
		}
	}
	free (db->free_blocks);
	free (db->groups.sparse);
	free (db->groups.dense);
	free (db->groups.data);
	free (db->idpool.ids);
	*db = (efrag_db_t) {};
};

void
Efrags_DelEfrag (efrag_db_t *db, uint32_t efragid)
{
	qfZoneScoped (true);
	if (!ECS_IdValid (&db->idpool, efragid)) {
		return;
	}

	auto pool = &db->groups;
	auto c = &cluster_group_component;

	uint32_t    id = Ent_Index (efragid);
	uint32_t    ind = pool->sparse[id];
	uint32_t    last = pool->count - 1;
	Component_DestroyElements (c, pool->data, ind, 1, &efragid,
							   (ecs_registry_t *) db);
	if (last > ind) {
		pool->sparse[Ent_Index (pool->dense[last])] = ind;
		pool->dense[ind] = pool->dense[last];
		Component_MoveElements (c, pool->data, ind, last, 1);
	}
	pool->count--;
	pool->sparse[id] = nullent;

	ECS_DelId (&db->idpool, efragid);
}

uint32_t
R_LinkEfrag (scene_t *scene, uint32_t cluster_num, entity_t ent,
			 uint32_t queue, uint32_t efrag)
{
	qfZoneScoped (true);
	if (cluster_num == 0) {
		return efrag;
	}
	auto db = scene->efrag_db;
	if (efrag == nullent) {
		efrag = ECS_NewId (&db->idpool);
	}
	auto pool = &db->groups;
	uint32_t    id = Ent_Index (efrag);
	ecs_expand_sparse (pool, id);
	uint32_t    ind = pool->sparse[id];
	if (ind >= pool->count || pool->dense[ind] != efrag) {
		ind = ecs_expand_pool (&db->groups, 1, &cluster_group_component);
		pool->sparse[id] = ind;
		pool->dense[ind] = efrag;
		Component_CreateElements (&cluster_group_component, db->groups.data,
								  ind, 1, &efrag, (ecs_registry_t *) db);
	}
	cluster_group_t *grp = Component_Address (&cluster_group_component,
											  db->groups.data, ind);
	cluster_ref_t grpref = {
		.id = efrag,
		.index = grp->num_refs,
	};
	if (grp->num_refs == grp->max_refs) {
		uint32_t    new_max = grp->max_refs + 8;
		grp->refs = realloc_refs (db, grp->refs, grp->max_refs, new_max);
		grp->max_refs = new_max;
	}
	auto frags = &db->clusters[cluster_num];
	if (frags->num_efrags == frags->max_efrags) {
		uint32_t    new_max = frags->max_efrags + 8;
		frags->efrags = realloc_efrags (db, frags->efrags,
										frags->max_efrags, new_max);
		frags->group_refs = realloc_refs (db, frags->group_refs,
										  frags->max_efrags, new_max);
		frags->max_efrags = new_max;
	}
	grp->refs[grp->num_refs++] = (cluster_ref_t) {
		.id = cluster_num,
		.index = frags->num_efrags,
	};
	frags->efrags[frags->num_efrags] = (efrag_t) {
		.entity = ent.id,
		.queue_num = queue,
	};
	frags->group_refs[frags->num_efrags] = grpref;
	frags->num_efrags++;
	return efrag;
}

static void
R_SplitEntityOnNode (scene_t *scene, entity_t ent, uint32_t queue,
					 visibility_t *visibility, vec3_t emins, vec3_t emaxs)
{
	qfZoneScoped (true);
	plane_t    *splitplane;
	int         sides;
	int        *node_stack;
	int32_t    *node_ptr;

	auto brush = scene->worldmodel->brush;

	node_stack = alloca ((brush->depth + 2) * sizeof (int32_t *));
	node_ptr = node_stack;

	*node_ptr++ = brush->numnodes;

	int32_t     node_id = 0;
	while (node_id != (int) brush->numnodes) {
		// add an efrag if the node is a leaf (cluster)
		if (__builtin_expect (node_id < 0, 0)) {
			if (visibility->topnode_id == -1) {
				visibility->topnode_id = node_id;
			}

			visibility->efrag = R_LinkEfrag (scene, ~node_id, ent, queue,
											 visibility->efrag);

			node_id = *--node_ptr;
		} else {
			mnode_t    *node = brush->cluster_nodes + node_id;
			// NODE_MIXED
			splitplane = (plane_t *) &node->plane;
			sides = BOX_ON_PLANE_SIDE (emins, emaxs, splitplane);

			if (sides == 3) {
				// split on this plane
				// if this is the first splitter of this bmodel, remember it
				if (visibility->topnode_id == -1) {
					visibility->topnode_id = node_id;
				}
			}
			// recurse down the contacted sides
			if (sides & 1) {
				if (sides & 2)
					*node_ptr++ = node->children[1];
				node_id = node->children[0];
			} else {
				if (sides & 2)
					node_id = node->children[1];
				else
					node_id = *--node_ptr;
			}
		}
	}
}

void
R_AddEfrags (scene_t *scene, entity_t ent)
{
	qfZoneScoped (true);
	model_t    *entmodel;
	vec3_t      emins, emaxs;
	transform_t transform = Entity_Transform (ent);
	auto rend = Entity_GetRenderer (ent);

	if (!rend->model) {
		Ent_RemoveComponent (ent.id, ent.base + scene_visibility, ent.reg);
		return;
	}
	visibility_t *vis;
	if (Ent_HasComponent (ent.id, ent.base + scene_visibility, ent.reg)) {
		vis = Ent_GetComponent (ent.id, ent.base + scene_visibility, ent.reg);
		Efrags_DelEfrag (scene->efrag_db, vis->efrag);
	} else {
		vis = Ent_AddComponent (ent.id, ent.base + scene_visibility, ent.reg);
	}
	vis->efrag = nullent;
	vis->topnode_id = -1;	// leaf 0 (solid space)

	entmodel = rend->model;

	vec4f_t     org = Transform_GetWorldPosition (transform);
	VectorAdd (org, entmodel->mins, emins);
	VectorAdd (org, entmodel->maxs, emaxs);

	int queue = rend->model->type;
	int *entqueue = Entity_GetEntqueue (ent);
	if (entqueue) {
		queue = *entqueue;
	}
	R_SplitEntityOnNode (scene, ent, queue, vis, emins, emaxs);
}

void
R_StoreEfrags (scene_t *scene, uint32_t cluster_num)
{
	qfZoneScoped (true);
	auto db = scene->efrag_db;
	auto frags = &db->clusters[cluster_num];
	for (uint32_t i = 0; i < frags->num_efrags; i++) {
		auto efrag = &frags->efrags[i];
		entity_t    ent = {
			.reg = scene->reg,
			.id = efrag->entity,
			.base = scene->base,
		};
		EntQueue_AddEntity (scene->ent_queue, ent, efrag->queue_num);
	}
}
