/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/11/14

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
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <stdio.h>

#include "QF/model.h"
#include "QF/winding.h"

#include "world.h"

static clipport_t *
alloc_portal (void)
{
	return calloc (1, sizeof (clipport_t));
}

static void
free_portal (clipport_t *portal)
{
	FreeWinding (portal->winding);
	FreeWinding (portal->edges);
	free (portal);
}

static void
remove_portal (clipport_t *portal, clipleaf_t *leaf)
{
	clipport_t **p;
	int         side;

	for (p = &leaf->portals; *p; p = &(*p)->next[side]) {
		side = (*p)->leafs[1] == leaf;
		if (*p == portal) {
			*p = portal->next[side];
			portal->next[side] = 0;
			break;
		}
	}
}

static clipleaf_t *
alloc_leaf (void)
{
	return calloc (1, sizeof (clipleaf_t));
}

static void
free_leaf (clipleaf_t *leaf)
{
	if (!leaf)
		return;
	while (leaf->portals) {
		clipport_t *portal = leaf->portals;
		int         side = portal->leafs[1] == leaf;
		leaf->portals = portal->next[side];
		remove_portal (portal, portal->leafs[side ^ 1]);
		free_portal (portal);
	}
	free (leaf);
}

static void
add_portal (clipport_t *portal, clipleaf_t *front, clipleaf_t *back)
{
	portal->leafs[0] = front;
	portal->next[0] = front->portals;
	front->portals = portal;

	portal->leafs[1] = back;
	portal->next[1] = back->portals;
	back->portals = portal;
}

static clipleaf_t *
carve_leaf (hull_t *hull, nodeleaf_t *nodeleafs, clipleaf_t *leaf, int num)
{
	dclipnode_t *node;
	plane_t    *plane;
	winding_t  *winding, *fw, *bw;
	clipport_t *portal;
	clipport_t *new_portal;
	clipport_t *next_portal;
	clipleaf_t *other_leaf;
	clipleaf_t *new_leaf;
	plane_t     clipplane;
	int         side;

	if (num < 0) {
		// we've hit a leaf. all done
		leaf->contents = num;
		return leaf;
	}
	node = hull->clipnodes + num;
	plane = hull->planes + node->planenum;

	winding = BaseWindingForPlane (plane);
	for (portal = leaf->portals; portal; portal = portal->next[side]) {
		clipplane = hull->planes[portal->planenum];
		side = (portal->leafs[1] == leaf);
		if (side)
			PlaneFlip (&clipplane, &clipplane);
		winding = ClipWinding (winding, &clipplane, true);
	}
	new_leaf = alloc_leaf ();
	portal = leaf->portals;
	leaf->portals = 0;
	for (; portal; portal = next_portal) {
		side = (portal->leafs[1] == leaf);
		next_portal = portal->next[side];
		other_leaf = portal->leafs[!side];
		remove_portal (portal, other_leaf);

		DivideWinding (portal->winding, plane, &fw, &bw);
		if (!fw) {
			if (side)
				add_portal (portal, other_leaf, new_leaf);
			else
				add_portal (portal, new_leaf, other_leaf);
			continue;
		}
		if (!bw) {
			if (side)
				add_portal (portal, other_leaf, leaf);
			else
				add_portal (portal, leaf, other_leaf);
			continue;
		}
		new_portal = alloc_portal ();
		new_portal->planenum = portal->planenum;
		new_portal->winding = bw;
		FreeWinding (portal->winding);
		portal->winding = fw;

		if (side) {
			add_portal (portal, other_leaf, leaf);
			add_portal (new_portal, other_leaf, new_leaf);
		} else {
			add_portal (portal, leaf, other_leaf);
			add_portal (new_portal, new_leaf, other_leaf);
		}
	}
	new_portal = alloc_portal ();
	new_portal->planenum = node->planenum;
	new_portal->winding = winding;
	add_portal (new_portal, leaf, new_leaf);

	nodeleafs[num].leafs[0] = carve_leaf (hull, nodeleafs, leaf,
										  node->children[0]);
	nodeleafs[num].leafs[1] = carve_leaf (hull, nodeleafs, new_leaf,
										  node->children[1]);
	return 0;
}

nodeleaf_t *
MOD_BuildBrushes (hull_t *hull)
{
	int         numnodes = hull->lastclipnode + 1;
	int         i, j, side;
	nodeleaf_t *nodeleafs;
	clipleaf_t *root;		// this will be carved into all the actual leafs

	nodeleafs = calloc (numnodes, sizeof (nodeleaf_t));
	root = alloc_leaf ();
	carve_leaf (hull, nodeleafs, root, hull->firstclipnode);
	for (i = 0; i < numnodes; i++) {
		for (j = 0; j < 2; j++) {
			clipleaf_t *leaf = nodeleafs[i].leafs[j];
			clipport_t *p;

			if (!leaf)
				continue;
			for (p = leaf->portals; p; p = p->next[side]) {
				side = p->leafs[1] == leaf;
				if (p->edges)
					continue;
				p->edges = WindingVectors (p->winding, 0);
			}
		}
	}
	return nodeleafs;
}

void
MOD_FreeBrushes (hull_t *hull)
{
	int         i, j;
	int         numnodes;

	if (!hull || !hull->nodeleafs)
		return;
	numnodes = hull->lastclipnode + 1;
	for (i = 0; i < numnodes; i++) {
		for (j = 0; j < 2; j++)
			free_leaf (hull->nodeleafs[i].leafs[j]);
	}
	free (hull->nodeleafs);
	hull->nodeleafs = 0;
}
