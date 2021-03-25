/*
	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	See file, 'COPYING', for details.
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#include "QF/sys.h"

#include "compat.h"

#include "tools/qfbsp/include/bsp5.h"
#include "tools/qfbsp/include/region.h"
#include "tools/qfbsp/include/surfaces.h"

/**	\addtogroup qfbsp_region
*/
//@{

/*
input
-----
vertexes
edges
faces

output
------
smaller set of vertexes
smaller set of edges
regions
? triangulated regions
face to region mapping numbers
*/

#define	MAX_EDGES_IN_REGION	32

int         firstedge;
vec3_t      region_mins, region_maxs;

/*
static void
AddPointToRegion (vec3_t p)
{
	int         i;

	for (i = 0; i < 3; i++) {
		if (p[i] < region_mins[i])
			region_mins[i] = p[i];
		if (p[i] > region_maxs[i])
			region_maxs[i] = p[i];
	}
}

static void
AddFaceToRegionSize (face_t *f)
{
	int         i;

	for (i = 0; i < f->points->numpoints; i++)
		AddPointToRegion (f->points->points[i]);
}

static qboolean
CanJoinFaces (face_t *f, face_t *f2)
{
	int         i;
	vec3_t      oldmins, oldmaxs;

	if (f2->planenum != f->planenum
		|| f2->planeside != f->planeside || f2->texturenum != f->texturenum)
		return false;
	if (f2->outputnumber != -1)
		return false;
	if (f2->contents[0] != f->contents[0]) {	// does this ever happen?
												// they shouldn't share.
		printf ("CanJoinFaces: edge with different contents");
		return false;
	}
	// check size constraints
	if (!(bsp->texinfo[f->texturenum].flags & TEX_SPECIAL)) {
		VectorCopy (region_mins, oldmins);
		VectorCopy (region_maxs, oldmaxs);
		AddFaceToRegionSize (f2);
		for (i = 0; i < 3; i++) {
			if (region_maxs[i] - region_mins[i] > options.subdivide_size) {
				VectorCopy (oldmins, region_mins);
				VectorCopy (oldmaxs, region_maxs);
				return false;
			}
		}
	} else {
		if (bsp->numsurfedges - firstedge + f2->points->numpoints
			> MAX_EDGES_IN_REGION)
			return false;				// a huge water or sky polygon
	}

	// check edge count constraints
	return true;
}

static void
RecursiveGrowRegion (dface_t *r, face_t *f)
{
	face_t     *f2;
	int         e, i;

	if (f->outputnumber == bsp->numfaces)
		return;

	if (f->outputnumber != -1)
		Sys_Error ("RecursiveGrowRegion: region collision");
	f->outputnumber = bsp->numfaces;

	// add edges
	for (i = 0; i < f->points->numpoints; i++) {
		e = f->edges[i];
		if (!edgefaces[abs (e)].f[0])
			continue;					// edge has allready been removed
		if (e > 0)
			f2 = edgefaces[e].f[1];
		else
			f2 = edgefaces[-e].f[0];
		if (f2 && f2->outputnumber == bsp->numfaces) {
			edgefaces[abs (e)].f[0] = NULL;
			edgefaces[abs (e)].f[1] = NULL;
			continue;					// allready merged
		}
		if (f2 && CanJoinFaces (f, f2)) {	// remove the edge and merge the
											// faces
			edgefaces[abs (e)].f[0] = NULL;
			edgefaces[abs (e)].f[1] = NULL;
			RecursiveGrowRegion (r, f2);
		} else {
			// emit a surfedge
			BSP_AddSurfEdge (bsp, e);
		}
	}

}
*/

typedef struct {
	int         numedges;
	int         edges[2];
} checkpoint_t;

checkpoint_t checkpoints[MAX_MAP_VERTS];

static void
CountRealNumbers (void)
{
	int         c, i;

	qprintf ("%5i regions\n", bsp->numfaces - firstmodelface);

	c = 0;
	for (i = firstmodelface; i < bsp->numfaces; i++)
		c += bsp->faces[i].numedges;
	qprintf ("%5i real marksurfaces\n", c);

	c = 0;
	for (i = firstmodeledge; i < bsp->numedges; i++)
		if (edgefaces[i].f[0])
			c++;						// not removed

	qprintf ("%5i real edges\n", c);
}

static void
GrowNodeRegion_r (node_t * node)
{
	dface_t     r;
	face_t     *f;
	int         i;

	if (node->planenum == PLANENUM_LEAF)
		return;

	node->firstface = bsp->numfaces;

	for (f = node->faces; f; f = f->next) {
		if (f->texturenum < 0)
			continue;

//		if (f->outputnumber != -1)
//			continue;		// allready grown into an earlier region

		// emit a region
		if (bsp->numfaces == MAX_MAP_FACES)
			Sys_Error ("MAX_MAP_FACES");
		f->outputnumber = bsp->numfaces;

		r.planenum = node->outputplanenum;
		r.side = f->planeside;
		r.texinfo = f->texturenum;
		for (i = 0; i < MAXLIGHTMAPS; i++)
			r.styles[i] = 255;
		r.lightofs = -1;

		// add the face and mergable neighbors to it
#if 0
		ClearRegionSize ();
		AddFaceToRegionSize (f);
		RecursiveGrowRegion (r, f);
#endif
		r.firstedge = firstedge = bsp->numsurfedges;
		for (i = 0; i < f->points->numpoints; i++) {
			BSP_AddSurfEdge (bsp, f->edges[i]);
		}

		r.numedges = bsp->numsurfedges - r.firstedge;

		BSP_AddFace (bsp, &r);
	}

	node->numfaces = bsp->numfaces - node->firstface;

	GrowNodeRegion_r (node->children[0]);
	GrowNodeRegion_r (node->children[1]);
}

void
GrowNodeRegions (node_t *headnode)
{
	qprintf ("---- GrowRegions ----\n");

	GrowNodeRegion_r (headnode);

//	RemoveColinearEdges ();
	CountRealNumbers ();
}

/*
	Turn the faces on a plane into optimal non-convex regions
	The edges may still be split later as a result of tjunctions

	typedef struct
	{
		vec3_t	dir;
		vec3_t	origin;
		vec3_t	p[2];
	}

	for all faces
		for all edges
			for all edges so far
				if overlap
					split
*/

//@}
