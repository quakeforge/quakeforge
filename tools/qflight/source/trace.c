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

#include "tools/qflight/include/light.h"
#include "tools/qflight/include/options.h"

typedef struct {
	int         type;
	vec3_t      normal;
	float       dist;
	int         children[2];
	int         pad;
} tnode_t;

typedef struct {
	vec3_t     backpt;
	int        side;
	int        node;
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
		if (node->children[i] < 0) {
			t->children[i] = bsp->leafs[-node->children[i] - 1].contents;
			if (options.solid_sky && t->children[i] == CONTENTS_SOLID) {
				dface_t    *face = &bsp->faces[node->firstface];
				if (!strncmp (get_tex_name (face->texinfo), "sky", 3))
					t->children[i] = CONTENTS_SKY;	// Simulate real sky
			}
		} else {
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

/*	LordHavoc: this function operates by doing depth-first front-to-back
	recursion through the BSP tree, checking at every split for a empty to
	solid transition (impact) in the children, and returns false if one is
	found.
	note: 'no impact' does not mean it is empty, it occurs when there is no
	transition from empty to solid; all solid or a transition from solid to
	empty are not considered impacts. (this does mean that tracing is not
	symmetrical; point A to point B may have different results than point B to
	point A, if either start in solid)
*/

#define TESTLINESTATE_BLOCKED 0
#define TESTLINESTATE_EMPTY 1
#define TESTLINESTATE_SOLID 2

static qboolean
TestLineOrSky (lightinfo_t *l, const vec3_t start, const vec3_t end,
			   qboolean sky_test)
{
	vec_t       front, back;
	vec3_t      frontpt, backpt;
	int         node, side, empty;
	tracestack_t *tstack;
	tracestack_t tracestack[64];
	tnode_t    *tnode;

	VectorCopy (start, frontpt);
	VectorCopy (end, backpt);

	tstack = tracestack;
	node = 0;
	empty = 0;

	while (1) {
		while (node < 0) {
			if (sky_test && node == CONTENTS_SKY)
				return true;
			if (node != CONTENTS_SOLID)
				empty = 1;
			else if (empty) {
				// DONE!
				VectorCopy (backpt, l->testlineimpact);
				return false;
			}

			// pop up the stack for a back side
			if (tstack-- == tracestack)
				return !sky_test;

			// set the hit point for this plane
			VectorCopy (backpt, frontpt);

			// go down the back side
			VectorCopy (tstack->backpt, backpt);

			node = tnodes[tstack->node].children[tstack->side ^ 1];
		}

		tnode = &tnodes[node];

		if (tnode->type < 3) {
			front = frontpt[tnode->type] - tnode->dist;
			back = backpt[tnode->type] - tnode->dist;
		} else {
			front = DotProduct (tnode->normal, frontpt) - tnode->dist;
			back = DotProduct (tnode->normal, backpt) - tnode->dist;
		}

		if (front >= 0 && back >= 0) {
			node = tnode->children[0];
			continue;
		}

		if (front < 0 && back < 0) {
			node = tnode->children[1];
			continue;
		}

		side = front < 0;

		front = front / (front - back);

		tstack->node = node;
		tstack->side = side;
		VectorCopy (backpt, tstack->backpt);

		tstack++;

		backpt[0] = frontpt[0] + front * (backpt[0] - frontpt[0]);
		backpt[1] = frontpt[1] + front * (backpt[1] - frontpt[1]);
		backpt[2] = frontpt[2] + front * (backpt[2] - frontpt[2]);

		node = tnode->children[side];
	}
}

qboolean
TestLine (lightinfo_t *l, const vec3_t start, const vec3_t stop)
{
	return TestLineOrSky (l, start, stop, false);
}

qboolean
TestSky (lightinfo_t *l, const vec3_t start, const vec3_t dir)
{
	vec3_t      stop;
	VectorAdd (dir, start, stop);
	return TestLineOrSky (l, start, stop, true);
}
