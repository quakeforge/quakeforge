/*
	trace.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2002 Colin Thompson

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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/bspfile.h"
#include "QF/mathlib.h"
#include "QF/qtypes.h"
#include "QF/dstring.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "light.h"

typedef struct {
	int type;
	vec3_t normal;
	float dist;
	int children[2];
	int pad;
} tnode_t;

typedef struct {
	vec3_t backpt;
	int side;
	int node;
} tracestack_t;

tnode_t *tnodes, *tnode_p;

/*
LINE TRACING

The major lighting operation is a point to point visibility test, performed
by recursive subdivision of the line by the BSP tree.
*/


/*
	MakeTnode

	Converts the disk node structure into the efficient tracing structure
*/
static void
MakeTnode (int nodenum)
{
	dnode_t		*node;
	dplane_t	*plane;
	int			i;
	tnode_t		*t;

	t = tnode_p++;

	node = bsp->nodes + nodenum;
	plane = bsp->planes + node->planenum;

	t->type = plane->type;
	VectorCopy (plane->normal, t->normal);
	t->dist = plane->dist;

	for (i = 0; i < 2; i++) {
		if (node->children[i] < 0)
			t->children[i] = bsp->leafs[-node->children[i] - 1].contents;
		else {
			t->children[i] = tnode_p - tnodes;
			MakeTnode (node->children[i]);
		}
	}
}

/*
	MakeTnodes

	Loads the node structure out of a .bsp file to be used for light occlusion
*/
void
MakeTnodes (dmodel_t *bm)
{
	tnode_p = tnodes = malloc (bsp->numnodes * sizeof (tnode_t));

	MakeTnode (0);
}

qboolean
TestLine (vec3_t start, vec3_t stop)
{
	float		front, back, frontx, fronty, frontz, backx, backy, backz;
	int			node, side;
	tracestack_t *tstack_p;
	tracestack_t tracestack[64];
	tnode_t		*tnode;

	frontx = start[0];
	fronty = start[1];
	frontz = start[2];
	backx = stop[0];
	backy = stop[1];
	backz = stop[2];

	tstack_p = tracestack;
	node = 0;

	while (1) {
		while (node < 0 && node != CONTENTS_SOLID) {
			// pop up the stack for a back side
			tstack_p--;
			if (tstack_p < tracestack)
				return true;
			node = tstack_p->node;

			// set the hit point for this plane
			frontx = backx;
			fronty = backy;
			frontz = backz;

			// go down the back side
			backx = tstack_p->backpt[0];
			backy = tstack_p->backpt[1];
			backz = tstack_p->backpt[2];

			node = tnodes[tstack_p->node].children[!tstack_p->side];
		}

		if (node == CONTENTS_SOLID)
			return false;	// DONE!

		tnode = &tnodes[node];

		switch (tnode->type) {
			case PLANE_X:
				front = frontx - tnode->dist;
				back = backx - tnode->dist;
				break;
			case PLANE_Y:
				front = fronty - tnode->dist;
				back = backy - tnode->dist;
				break;
			case PLANE_Z:
				front = frontz - tnode->dist;
				back = backz - tnode->dist;
				break;
			default:
				front =	(frontx * tnode->normal[0] + 
						 fronty * tnode->normal[1] + 
						 frontz * tnode->normal[2]) - tnode->dist;
				back = (backx * tnode->normal[0] + 
						backy * tnode->normal[1] +
						backz * tnode->normal[2]) - tnode->dist;
				break;
		}

		if (front > -ON_EPSILON && back > -ON_EPSILON) {
//		if (front > 0 && back > 0) {
			node = tnode->children[0];
			continue;
		}

		if (front < ON_EPSILON && back < ON_EPSILON) {
//		if (front <= 0 && back <= 0) {
			node = tnode->children[1];
			continue;
		}

		side = front < 0;

		front = front / (front - back);

		tstack_p->node = node;
		tstack_p->side = side;
		tstack_p->backpt[0] = backx;
		tstack_p->backpt[1] = backy;
		tstack_p->backpt[2] = backz;

		tstack_p++;

		backx = frontx + front * (backx - frontx);
		backy = fronty + front * (backy - fronty);
		backz = frontz + front * (backz - frontz);

		node = tnode->children[side];
	}
}
