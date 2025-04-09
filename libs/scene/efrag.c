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

#include "QF/scene/entity.h"

#include "qfalloca.h"
#include "r_internal.h"

typedef struct s_efrag_list {
	struct s_efrag_list *next;
	efrag_t     efrags[MAX_EFRAGS];
} t_efrag_list;

static efrag_t    *r_free_efrags;
static t_efrag_list *efrag_list;

entqueue_t *r_ent_queue;

/* ENTITY FRAGMENT FUNCTIONS */

static inline void
init_efrag_list (t_efrag_list *efl)
{
	int i;

	for (i = 0; i < MAX_EFRAGS - 1; i++)
		efl->efrags[i].entnext = &efl->efrags[i + 1];
	efl->efrags[i].entnext = 0;
}

static efrag_t *
new_efrag (void)
{
	efrag_t    *ef;

	if (__builtin_expect (!r_free_efrags, 0)) {
		t_efrag_list *efl = calloc (1, sizeof (t_efrag_list));
		SYS_CHECKMEM (efl);
		efl->next = efrag_list;
		efrag_list = efl;
		init_efrag_list (efl);
		r_free_efrags = &efl->efrags[0];
	}
	ef = r_free_efrags;
	r_free_efrags = ef->entnext;
	ef->entnext = 0;
	return ef;
}

void
R_ClearEfrags (void)
{
	t_efrag_list *efl;

	if (!efrag_list)
		efrag_list = calloc (1, sizeof (t_efrag_list));

	r_free_efrags = efrag_list->efrags;
	for (efl = efrag_list; efl; efl = efl->next) {
		init_efrag_list (efl);
		if (efl->next)
			efl->efrags[MAX_EFRAGS - 1].entnext = &efl->next->efrags[0];
	}
}

void
R_ShutdownEfrags (void)
{
	while (efrag_list) {
		t_efrag_list *efl = efrag_list->next;
		free (efrag_list);
		efrag_list = efl;
	}
}

void
R_ClearEfragChain (efrag_t *ef)
{
	efrag_t    *old, *walk, **prev;

	while (ef) {
		prev = &ef->leaf->efrags;
		while ((walk = *prev)) {
			if (walk == ef) {			// remove this fragment
				*prev = ef->leafnext;
				break;
			} else {
				prev = &walk->leafnext;
			}
		}

		old = ef;
		ef = ef->entnext;

		// put it on the free list
		old->entnext = r_free_efrags;
		r_free_efrags = old;
	}
}

efrag_t **
R_LinkEfrag (mleaf_t *leaf, entity_t ent, uint32_t queue, efrag_t **lastlink)
{
	efrag_t    *ef = new_efrag ();	// ensures ef->entnext is 0

	// add the link to the chain of links on the entity
	ef->entity = ent;
	ef->queue_num = queue;
	*lastlink = ef;

	// add the link too the chain of links on the leaf
	ef->leaf = leaf;
	ef->leafnext = leaf->efrags;
	leaf->efrags = ef;

	return &ef->entnext;
}

static void
R_SplitEntityOnNode (mod_brush_t *brush, entity_t ent, uint32_t queue,
					 visibility_t *visibility, vec3_t emins, vec3_t emaxs)
{
	plane_t    *splitplane;
	mleaf_t    *leaf;
	int         sides;
	efrag_t   **lastlink;
	int        *node_stack;
	int32_t    *node_ptr;

	node_stack = alloca ((brush->depth + 2) * sizeof (int32_t *));
	node_ptr = node_stack;

	lastlink = &visibility->efrag;

	*node_ptr++ = brush->numnodes;

	int32_t     node_id = 0;
	while (node_id != (int) brush->numnodes) {
		// add an efrag if the node is a leaf
		if (__builtin_expect (node_id < 0, 0)) {
			if (visibility->topnode_id == -1) {
				visibility->topnode_id = node_id;
			}

			leaf = brush->leafs + ~node_id;

			lastlink = R_LinkEfrag (leaf, ent, queue, lastlink);

			node_id = *--node_ptr;
		} else {
			mnode_t    *node = brush->nodes + node_id;
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
R_AddEfrags (mod_brush_t *brush, entity_t ent)
{
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
		R_ClearEfragChain (vis->efrag);
	} else {
		vis = Ent_AddComponent (ent.id, ent.base + scene_visibility, ent.reg);
	}
	vis->efrag = 0;

	entmodel = rend->model;

	vec4f_t     org = Transform_GetWorldPosition (transform);
	VectorAdd (org, entmodel->mins, emins);
	VectorAdd (org, entmodel->maxs, emaxs);

	vis->topnode_id = -1;	// leaf 0 (solid space)
	R_SplitEntityOnNode (brush, ent, rend->model->type, vis, emins, emaxs);
}

void
R_StoreEfrags (const efrag_t *efrag)
{
	while (efrag) {
		entity_t    ent = efrag->entity;
		EntQueue_AddEntity (r_ent_queue, ent, efrag->queue_num);
		efrag = efrag->leafnext;
	}
}
