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
RecursiveTestLine (lightinfo_t *l, int num, vec3_t p1, vec3_t p2)
{
	int         side, ret;
	vec_t       t1, t2, frac;
	vec3_t      mid;
	tnode_t    *tnode;

	// check for empty
loc0:
	if (num < 0) {
		if (num == CONTENTS_SOLID)
			return TESTLINESTATE_SOLID;
		else
			return TESTLINESTATE_EMPTY;
	}

	// find the point distances
	tnode = tnodes + num;

	if (tnode->type < 3) {
		t1 = p1[tnode->type] - tnode->dist;
		t2 = p2[tnode->type] - tnode->dist;
	} else {
		t1 = DotProduct (tnode->normal, p1) - tnode->dist;
		t2 = DotProduct (tnode->normal, p2) - tnode->dist;
	}

	if (t1 >= 0) {
		if (t2 >= 0) {
			num = tnode->children[0];
			goto loc0;
		}
		side = 0;
	} else {
		if (t2 < 0) {
			num = tnode->children[1];
			goto loc0;
		}
		side = 1;
	}

	frac = t1 / (t1 - t2);
	mid[0] = p1[0] + frac * (p2[0] - p1[0]);
	mid[1] = p1[1] + frac * (p2[1] - p1[1]);
	mid[2] = p1[2] + frac * (p2[2] - p1[2]);

	// front side first
	ret = RecursiveTestLine (l, tnode->children[side], p1, mid);
	if (ret != TESTLINESTATE_EMPTY)
		return ret; // solid or blocked
	ret = RecursiveTestLine (l, tnode->children[!side], mid, p2);
	if (ret != TESTLINESTATE_SOLID)
		return ret; // empty or blocked
	VectorCopy (mid, l->testlineimpact);
	return TESTLINESTATE_BLOCKED;
}

qboolean
TestLine (lightinfo_t *l, vec3_t start, vec3_t end)
{
	return RecursiveTestLine (l, 0, start, end) != TESTLINESTATE_BLOCKED;
}
