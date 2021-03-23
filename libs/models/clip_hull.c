/*
	clip_hull.c

	dynamic bsp clipping hull management

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/7/28

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

#include "QF/clip_hull.h"
#include "QF/model.h"

#include "mod_internal.h"

VISIBLE clip_hull_t *
MOD_Alloc_Hull (int nodes, int planes)
{
	clip_hull_t *ch;
	int			 size, i;

	size = sizeof (hull_t);
	size += sizeof (mclipnode_t) * nodes + sizeof (plane_t) * planes;
	size *= MAX_MAP_HULLS;
	size += sizeof (clip_hull_t);

	ch = calloc (size, 1);
	if (!ch)
		return 0;
	ch->hulls[0] = (hull_t *) &ch[1];
	for (i = 1; i < MAX_MAP_HULLS; i++)
		ch->hulls[i] = &ch->hulls[i - 1][1];
	ch->hulls[0]->clipnodes = (mclipnode_t *) &ch->hulls[i - 1][1];
	ch->hulls[0]->planes = (plane_t *) &ch->hulls[0]->clipnodes[nodes];
	for (i = 1; i < MAX_MAP_HULLS; i++) {
		ch->hulls[i]->clipnodes =
			(mclipnode_t *) &ch->hulls[i - 1]->planes[planes];
		ch->hulls[i]->planes = (plane_t *) &ch->hulls[i]->clipnodes[nodes];
	}
	return ch;
}

VISIBLE void
MOD_Free_Hull (clip_hull_t *ch)
{
	free (ch);
}

static void
recurse_clip_tree (hull_t *hull, int num, int depth)
{
	mclipnode_t *node;

	if (num < 0) {
		if (depth > hull->depth)
			hull->depth = depth;
		return;
	}
	node = hull->clipnodes + num;
	recurse_clip_tree (hull, node->children[0], depth + 1);
	recurse_clip_tree (hull, node->children[1], depth + 1);
}

void
Mod_FindClipDepth (hull_t *hull)
{
	hull->depth = 0;
	if (hull->clipnodes)	// no hull 3?
		recurse_clip_tree (hull, hull->firstclipnode, 1);
}
