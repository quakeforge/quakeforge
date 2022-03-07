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

/*
  R_RemoveEfrags

  Call when removing an object from the world or moving it to another position
*/
void
R_RemoveEfrags (entity_t *ent)
{
	efrag_t    *ef, *old, *walk, **prev;

	ef = ent->visibility.efrag;

	while (ef) {
		prev = &ef->leaf->efrags;
		while (1) {
			walk = *prev;
			if (!walk)
				break;
			if (walk == ef) {			// remove this fragment
				*prev = ef->leafnext;
				break;
			} else
				prev = &walk->leafnext;
		}

		old = ef;
		ef = ef->entnext;

		// put it on the free list
		old->entnext = r_free_efrags;
		r_free_efrags = old;
	}

	ent->visibility.efrag = 0;
}

static void
R_SplitEntityOnNode (mod_brush_t *brush, entity_t *ent,
					 vec3_t emins, vec3_t emaxs)
{
	efrag_t    *ef;
	plane_t    *splitplane;
	mleaf_t    *leaf;
	int         sides;
	efrag_t   **lastlink;
	mnode_t   **node_stack;
	mnode_t   **node_ptr;
	mnode_t    *node = brush->nodes;

	node_stack = alloca ((brush->depth + 2) * sizeof (mnode_t *));
	node_ptr = node_stack;

	lastlink = &ent->visibility.efrag;

	*node_ptr++ = 0;

	while (node) {
		// add an efrag if the node is a leaf
		if (__builtin_expect (node->contents < 0, 0)) {
			if (!ent->visibility.topnode) {
				ent->visibility.topnode = node;
			}

			leaf = (mleaf_t *) node;

			ef = new_efrag ();	// ensures ef->entnext is 0

			// add the link to the chain of links on the entity
			ef->entity = ent;
			*lastlink = ef;
			lastlink = &ef->entnext;

			// add the link too the chain of links on the leaf
			ef->leaf = leaf;
			ef->leafnext = leaf->efrags;
			leaf->efrags = ef;

			node = *--node_ptr;
		} else {
			// NODE_MIXED
			splitplane = node->plane;
			sides = BOX_ON_PLANE_SIDE (emins, emaxs, splitplane);

			if (sides == 3) {
				// split on this plane
				// if this is the first splitter of this bmodel, remember it
				if (!ent->visibility.topnode) {
					ent->visibility.topnode = node;
				}
			}
			// recurse down the contacted sides
			if (sides & 1 && node->children[0]->contents != CONTENTS_SOLID) {
				if (sides & 2 && node->children[1]->contents != CONTENTS_SOLID)
					*node_ptr++ = node->children[1];
				node = node->children[0];
			} else {
				if (sides & 2 && node->children[1]->contents != CONTENTS_SOLID)
					node = node->children[1];
				else
					node = *--node_ptr;
			}
		}
	}
}

void
R_AddEfrags (mod_brush_t *brush, entity_t *ent)
{
	model_t    *entmodel;
	vec3_t      emins, emaxs;

	if (!ent->renderer.model) {
		return;
	}

	entmodel = ent->renderer.model;

	vec4f_t     org = Transform_GetWorldPosition (ent->transform);
	VectorAdd (org, entmodel->mins, emins);
	VectorAdd (org, entmodel->maxs, emaxs);

	ent->visibility.topnode = 0;
	R_SplitEntityOnNode (brush, ent, emins, emaxs);
}

void
R_StoreEfrags (const efrag_t *efrag)
{
	entity_t   *ent;
	model_t    *model;

	while (efrag) {
		ent = efrag->entity;
		model = ent->renderer.model;

		switch (model->type) {
			case mod_alias:
			case mod_brush:
			case mod_sprite:
			case mod_iqm:
				EntQueue_AddEntity (r_ent_queue, ent, model->type);
				efrag = efrag->leafnext;
				break;

			default:
		}
	}
}
