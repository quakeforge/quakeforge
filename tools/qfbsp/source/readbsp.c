/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2003 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "QF/dstring.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/wad.h"

#include "bsp5.h"
#include "options.h"

face_t     *mfaces;
node_t     *nodes;
node_t     *leafs;
texinfo_t  *texinfo;
dvertex_t  *vertices;
dedge_t    *edges;
int        *surfedges;
unsigned short *marksurfaces;
plane_t    *mplanes;
static brushset_t bs;

static void
load_texinfo (void)
{
	texinfo = bsp->texinfo;
}

static void
load_entities (void)
{
}

static void
load_clipnodes (void)
{
}

static void
load_surfedges (void)
{
	surfedges = bsp->surfedges;
}

static void
load_edges (void)
{
	edges = bsp->edges;
}

static void
load_planes (void)
{
	dplane_t   *p;
	int         i;

	mplanes = malloc (bsp->numplanes * sizeof (plane_t));
	for (i = 0; i < bsp->numplanes; i++) {
		p = bsp->planes + i;
		VectorCopy (p->normal, mplanes[i].normal);
		mplanes[i].dist = p->dist;
		mplanes[i].type = p->type;
	}
}

static void
load_marksurfaces (void)
{
	marksurfaces = bsp->marksurfaces;
}

static void
load_faces (void)
{
	dface_t    *f;
	int         i, j;
	winding_t  *points;

	mfaces = calloc (bsp->numfaces, sizeof (face_t));
	for (i = 0; i < bsp->numfaces; i++) {
		f = bsp->faces + i;
		mfaces[i].planenum = f->planenum;
		mfaces[i].planeside = f->side;
		mfaces[i].texturenum = f->texinfo;
		mfaces[i].points = NewWinding (f->numedges);
		mfaces[i].edges = surfedges + f->firstedge;

		points = mfaces[i].points;
		points->numpoints = f->numedges;
		for (j = 0; j < points->numpoints; j++) {
			int         e = mfaces[i].edges[j];
			int         v;

			if (e < 0) {
				v = edges[-e].v[1];
			} else {
				v = edges[e].v[0];
			}
			VectorCopy (vertices[v].point, points->points[j]);
			printf ("%g %g %g\n", points->points[j][0], points->points[j][1], points->points[j][2]);
		}
	}
}

static void
load_vertices (void)
{
	vertices = bsp->vertexes;
}

static void
load_leafs (void)
{
	dleaf_t    *l;
	int         i, j;

	leafs = calloc (bsp->numleafs, sizeof (node_t));
	for (i = 0; i < bsp->numleafs; i++) {
		l = bsp->leafs + i;
		leafs[i].planenum = -1;
		leafs[i].contents = l->contents;
		VectorCopy (l->mins, leafs[i].mins);
		VectorCopy (l->maxs, leafs[i].maxs);
		leafs[i].markfaces = calloc (l->nummarksurfaces + 1,
									 sizeof (face_t *));
		for (j = 0; j < l->nummarksurfaces; j++) {
			unsigned short ms = l->firstmarksurface + j;
			leafs[i].markfaces[j] = mfaces + marksurfaces[ms];
		}
	}
}

static void
load_nodes (void)
{
	dnode_t    *n;
	face_t     *f;
	int         i, j;

	nodes = calloc (bsp->numnodes, sizeof (node_t));
	for (i = 0; i < bsp->numnodes; i++) {
		n = bsp->nodes + i;
		VectorCopy (n->mins, nodes[i].mins);
		VectorCopy (n->maxs, nodes[i].maxs);
		nodes[i].outputplanenum = n->planenum;
		nodes[i].planenum = nodes[i].outputplanenum;
		nodes[i].firstface = n->firstface;
		nodes[i].numfaces = n->numfaces;
		for (j = 0; j < 2; j++) {
			if (n->children[j] < 0) {
				nodes[i].children[j] = leafs - n->children[j] - 1;
			} else {
				nodes[i].children[j] = nodes + n->children[j];
			}
		}
		if (nodes[i].numfaces) {
			nodes[i].faces = mfaces + nodes[i].firstface;
			for (j = 0, f = nodes[i].faces; j < n->numfaces - 1; j++, f++) {
				f->next = f + 1;
			}
		}
	}
}

static void
load_models (void)
{
}

static void
load_textures (void)
{
}

void
LoadBSP (void)
{
	QFile      *f;

	f = Qopen (options.bspfile, "rb");
	if (!f)
		Sys_Error ("couldn't open %s. %s", options.bspfile, strerror(errno));
	bsp = LoadBSPFile (f, Qfilesize (f));
	Qclose (f);

	load_texinfo ();
	load_entities ();
	load_clipnodes ();
	load_surfedges ();
	load_edges ();
	load_planes ();
	load_marksurfaces ();
	load_vertices ();
	load_faces ();
	load_leafs ();
	load_nodes ();
	load_models ();
	load_textures ();

		memcpy (planes, mplanes, bsp->numplanes * sizeof (plane_t));
		VectorCopy (bsp->models[0].mins, bs.mins)
		VectorCopy (bsp->models[0].maxs, bs.maxs)
		brushset = &bs;
		PortalizeWorldDetail (nodes);
		WritePortalfile (nodes);
}
