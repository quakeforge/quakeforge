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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#include <stdlib.h>

#include "QF/render.h"
#include "QF/sys.h"

#include "r_local.h"

mnode_t    *r_pefragtopnode;
vec3_t      r_emins, r_emaxs;

typedef struct s_efrag_list {
	struct s_efrag_list *next;
	efrag_t     efrags[MAX_EFRAGS];
} t_efrag_list;

static efrag_t    *r_free_efrags;
static t_efrag_list *efrag_list;

/* ENTITY FRAGMENT FUNCTIONS */

static efrag_t   **lastlink;
static entity_t   *r_addent;


static inline void
init_efrag_list (t_efrag_list *efl)
{
	int i;

	for (i = 0; i < MAX_EFRAGS - 1; i++)
		efl->efrags[i].entnext = &efl->efrags[i + 1];
	efl->efrags[i].entnext = NULL;
}

void
R_ClearEfrags (void)
{
	t_efrag_list *efl;

	if (!efrag_list)
		efrag_list = calloc (1, sizeof (t_efrag_list));

	r_free_efrags = efrag_list->efrags;;
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

	ef = ent->efrag;

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

	ent->efrag = NULL;
}

#define NODE_STACK_SIZE 1024
static mnode_t *node_stack[NODE_STACK_SIZE];
static mnode_t **node_ptr = node_stack + NODE_STACK_SIZE;

static void
R_SplitEntityOnNode (mnode_t *node)
{
	efrag_t    *ef;
	mplane_t   *splitplane;
	mleaf_t    *leaf;
	int         sides;

	*--node_ptr = 0;

	while (node) {
		// add an efrag if the node is a leaf
		if (__builtin_expect (node->contents < 0, 0)) {
			if (!r_pefragtopnode)
				r_pefragtopnode = node;

			leaf = (mleaf_t *) node;

			// grab an efrag off the free list
			ef = r_free_efrags;
			if (__builtin_expect (!ef, 0)) {
				t_efrag_list *efl = calloc (1, sizeof (t_efrag_list));
				SYS_CHECKMEM (efl);
				efl->next = efrag_list;
				efrag_list = efl;
				ef = r_free_efrags = &efl->efrags[0];
			}
			r_free_efrags = r_free_efrags->entnext;

			ef->entity = r_addent;

			// add the entity link  
			*lastlink = ef;
			lastlink = &ef->entnext;
			ef->entnext = NULL;

			// set the leaf links
			ef->leaf = leaf;
			ef->leafnext = leaf->efrags;
			leaf->efrags = ef;

			node = *node_ptr++;
		} else {
			// NODE_MIXED
			splitplane = node->plane;
			sides = BOX_ON_PLANE_SIDE (r_emins, r_emaxs, splitplane);

			if (sides == 3) {
				// split on this plane
				// if this is the first splitter of this bmodel, remember it
				if (!r_pefragtopnode)
					r_pefragtopnode = node;
			}
			// recurse down the contacted sides
			if (sides & 1 && node->children[0]->contents != CONTENTS_SOLID) {
				if (sides & 2 && node->children[1]->contents != CONTENTS_SOLID)
					*--node_ptr = node->children[1];
				node = node->children[0];
			} else {
				if (sides & 2 && node->children[1]->contents != CONTENTS_SOLID)
					node = node->children[1];
				else
					node = *node_ptr++;
			}
		}
	}
}

void
R_SplitEntityOnNode2 (mnode_t *node)
{
	mplane_t   *splitplane;
	int         sides;

	if (node->visframe != r_visframecount)
		return;

	if (node->contents < 0) {
		if (node->contents != CONTENTS_SOLID)
			r_pefragtopnode = node;		// we've reached a non-solid leaf, so 
										// it's visible and not BSP clipped
		return;
	}

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE (r_emins, r_emaxs, splitplane);

	if (sides == 3) {
		// remember first splitter
		r_pefragtopnode = node;
		return;
	}

	// not split yet; recurse down the contacted side
	if (sides & 1)
		R_SplitEntityOnNode2 (node->children[0]);
	else
		R_SplitEntityOnNode2 (node->children[1]);
}

void
R_AddEfrags (entity_t *ent)
{
	model_t    *entmodel;
	int         i;

	if (!ent->model)
		return;

	if (ent == &r_worldentity)
		return;							// never add the world

	r_addent = ent;

	lastlink = &ent->efrag;
	r_pefragtopnode = NULL;

	entmodel = ent->model;

	for (i = 0; i < 3; i++) {
		r_emins[i] = ent->origin[i] + entmodel->mins[i];
		r_emaxs[i] = ent->origin[i] + entmodel->maxs[i];
	}

	R_SplitEntityOnNode (r_worldentity.model->nodes);

	ent->topnode = r_pefragtopnode;
}

/*
	R_StoreEfrags

	FIXME: a lot of this goes away with edge-based
*/
void
R_StoreEfrags (efrag_t **ppefrag)
{
	entity_t   *pent;
	model_t    *model;
	efrag_t    *pefrag;


	while ((pefrag = *ppefrag) != NULL) {
		pent = pefrag->entity;
		model = pent->model;

		switch (model->type) {
			case mod_alias:
			case mod_brush:
			case mod_sprite:
				pent = pefrag->entity;

				if (pent->visframe != r_framecount) {
					entity_t **ent = R_NewEntity ();
					if (!ent)
						return;
					*ent = pent;

					// mark that we've recorded this entity for this frame
					pent->visframe = r_framecount;
				}

				ppefrag = &pefrag->leafnext;
				break;

			default:
				Sys_Error ("R_StoreEfrags: Bad entity type %d",
						   model->type);
		}
	}
}
