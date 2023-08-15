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
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/wad.h"

#include "tools/qfbsp/include/brush.h"
#include "tools/qfbsp/include/bsp5.h"
#include "tools/qfbsp/include/options.h"
#include "tools/qfbsp/include/portals.h"
#include "tools/qfbsp/include/readbsp.h"

/**	\addtogroup qfbsp_readbsp
*/
//@{

dmodel_t   *models;
face_t     *mfaces;
node_t     *nodes;
node_t     *leafs;
texinfo_t  *texinfo;
dvertex_t  *vertices;
dedge_t    *edges;
int        *surfedges;
uint32_t   *marksurfaces;
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
	const dplane_t *p;
	plane_t     tp = { };

	planes.size = 0;
	for (size_t i = 0; i < bsp->numplanes; i++) {
		p = bsp->planes + i;
		VectorCopy (p->normal, tp.normal);
		tp.dist = p->dist;
		tp.type = p->type;
		DARRAY_APPEND (&planes, tp);
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
	const dface_t *f;
	winding_t  *points;

	mfaces = calloc (bsp->numfaces, sizeof (face_t));
	for (size_t i = 0; i < bsp->numfaces; i++) {
		f = bsp->faces + i;
		mfaces[i].planenum = f->planenum;
		mfaces[i].planeside = f->side;
		mfaces[i].texturenum = f->texinfo;
		mfaces[i].points = NewWinding (f->numedges);
		mfaces[i].edges = surfedges + f->firstedge;

		points = mfaces[i].points;
		points->numpoints = f->numedges;
		for (int j = 0; j < points->numpoints; j++) {
			int         e = mfaces[i].edges[j];
			int         v;

			if (e < 0) {
				v = edges[-e].v[1];
			} else {
				v = edges[e].v[0];
			}
			VectorCopy (vertices[v].point, points->points[j]);
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
	const dleaf_t *l;

	leafs = calloc (bsp->numleafs, sizeof (node_t));
	for (size_t i = 0; i < bsp->numleafs; i++) {
		l = bsp->leafs + i;
		leafs[i].planenum = -1;
		leafs[i].contents = l->contents;
		VectorCopy (l->mins, leafs[i].mins);
		VectorCopy (l->maxs, leafs[i].maxs);
		leafs[i].markfaces = calloc (l->nummarksurfaces + 1,
									 sizeof (face_t *));
		for (uint32_t j = 0; j < l->nummarksurfaces; j++) {
			unsigned short ms = l->firstmarksurface + j;
			leafs[i].markfaces[j] = mfaces + marksurfaces[ms];
		}
	}
}

static void
load_nodes (void)
{
	const dnode_t *n;

	nodes = calloc (bsp->numnodes, sizeof (node_t));
	for (size_t i = 0; i < bsp->numnodes; i++) {
		n = bsp->nodes + i;
		VectorCopy (n->mins, nodes[i].mins);
		VectorCopy (n->maxs, nodes[i].maxs);
		nodes[i].planenum = n->planenum;
		nodes[i].firstface = n->firstface;
		nodes[i].numfaces = n->numfaces;
		for (int j = 0; j < 2; j++) {
			if (n->children[j] < 0) {
				nodes[i].children[j] = leafs - n->children[j] - 1;
			} else {
				nodes[i].children[j] = nodes + n->children[j];
			}
		}
		if (nodes[i].numfaces) {
			nodes[i].faces = mfaces + nodes[i].firstface;
			for (uint32_t j = 0; j < n->numfaces - 1; j++) {
				face_t     *f = nodes[i].faces + j;
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

	f = Qopen (options.bspfile, "rbz");
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
}

static char *
output_file (const char *ext)
{
	dstring_t  *name;

	name = dstring_newstr ();
	if (options.output_file) {
		dstring_copystr (name, options.output_file);
		if (strcmp (options.output_file, "-") != 0)
			QFS_DefaultExtension (name, ext);
	} else {
		dstring_copystr (name, options.bspfile);
		QFS_StripExtension (name->str, name->str);
		QFS_DefaultExtension (name, ext);
	}
	return dstring_freeze (name);
}

void
bsp2prt (void)
{
	vec3_t      ooo = {1, 1, 1};

	options.portfile = output_file (".prt");

	VectorSubtract (bsp->models[0].mins, ooo, bs.mins);
	VectorAdd (bsp->models[0].maxs, ooo, bs.maxs);
	brushset = &bs;
	PortalizeWorld (nodes);
	WritePortalfile (nodes);
}

static byte default_palette[] = {
	0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x1F, 0x1F, 0x1F, 0x2F, 0x2F, 0x2F,
	0x3F, 0x3F, 0x3F, 0x4B, 0x4B, 0x4B, 0x5B, 0x5B, 0x5B, 0x6B, 0x6B, 0x6B,
	0x7B, 0x7B, 0x7B, 0x8B, 0x8B, 0x8B, 0x9B, 0x9B, 0x9B, 0xAB, 0xAB, 0xAB,
	0xBB, 0xBB, 0xBB, 0xCB, 0xCB, 0xCB, 0xDB, 0xDB, 0xDB, 0xEB, 0xEB, 0xEB,
	0x0F, 0x0B, 0x07, 0x17, 0x0F, 0x0B, 0x1F, 0x17, 0x0B, 0x27, 0x1B, 0x0F,
	0x2F, 0x23, 0x13, 0x37, 0x2B, 0x17, 0x3F, 0x2F, 0x17, 0x4B, 0x37, 0x1B,
	0x53, 0x3B, 0x1B, 0x5B, 0x43, 0x1F, 0x63, 0x4B, 0x1F, 0x6B, 0x53, 0x1F,
	0x73, 0x57, 0x1F, 0x7B, 0x5F, 0x23, 0x83, 0x67, 0x23, 0x8F, 0x6F, 0x23,
	0x0B, 0x0B, 0x0F, 0x13, 0x13, 0x1B, 0x1B, 0x1B, 0x27, 0x27, 0x27, 0x33,
	0x2F, 0x2F, 0x3F, 0x37, 0x37, 0x4B, 0x3F, 0x3F, 0x57, 0x47, 0x47, 0x67,
	0x4F, 0x4F, 0x73, 0x5B, 0x5B, 0x7F, 0x63, 0x63, 0x8B, 0x6B, 0x6B, 0x97,
	0x73, 0x73, 0xA3, 0x7B, 0x7B, 0xAF, 0x83, 0x83, 0xBB, 0x8B, 0x8B, 0xCB,
	0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x0B, 0x0B, 0x00, 0x13, 0x13, 0x00,
	0x1B, 0x1B, 0x00, 0x23, 0x23, 0x00, 0x2B, 0x2B, 0x07, 0x2F, 0x2F, 0x07,
	0x37, 0x37, 0x07, 0x3F, 0x3F, 0x07, 0x47, 0x47, 0x07, 0x4B, 0x4B, 0x0B,
	0x53, 0x53, 0x0B, 0x5B, 0x5B, 0x0B, 0x63, 0x63, 0x0B, 0x6B, 0x6B, 0x0F,
	0x07, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x17, 0x00, 0x00, 0x1F, 0x00, 0x00,
	0x27, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x37, 0x00, 0x00, 0x3F, 0x00, 0x00,
	0x47, 0x00, 0x00, 0x4F, 0x00, 0x00, 0x57, 0x00, 0x00, 0x5F, 0x00, 0x00,
	0x67, 0x00, 0x00, 0x6F, 0x00, 0x00, 0x77, 0x00, 0x00, 0x7F, 0x00, 0x00,
	0x13, 0x13, 0x00, 0x1B, 0x1B, 0x00, 0x23, 0x23, 0x00, 0x2F, 0x2B, 0x00,
	0x37, 0x2F, 0x00, 0x43, 0x37, 0x00, 0x4B, 0x3B, 0x07, 0x57, 0x43, 0x07,
	0x5F, 0x47, 0x07, 0x6B, 0x4B, 0x0B, 0x77, 0x53, 0x0F, 0x83, 0x57, 0x13,
	0x8B, 0x5B, 0x13, 0x97, 0x5F, 0x1B, 0xA3, 0x63, 0x1F, 0xAF, 0x67, 0x23,
	0x23, 0x13, 0x07, 0x2F, 0x17, 0x0B, 0x3B, 0x1F, 0x0F, 0x4B, 0x23, 0x13,
	0x57, 0x2B, 0x17, 0x63, 0x2F, 0x1F, 0x73, 0x37, 0x23, 0x7F, 0x3B, 0x2B,
	0x8F, 0x43, 0x33, 0x9F, 0x4F, 0x33, 0xAF, 0x63, 0x2F, 0xBF, 0x77, 0x2F,
	0xCF, 0x8F, 0x2B, 0xDF, 0xAB, 0x27, 0xEF, 0xCB, 0x1F, 0xFF, 0xF3, 0x1B,
	0x0B, 0x07, 0x00, 0x1B, 0x13, 0x00, 0x2B, 0x23, 0x0F, 0x37, 0x2B, 0x13,
	0x47, 0x33, 0x1B, 0x53, 0x37, 0x23, 0x63, 0x3F, 0x2B, 0x6F, 0x47, 0x33,
	0x7F, 0x53, 0x3F, 0x8B, 0x5F, 0x47, 0x9B, 0x6B, 0x53, 0xA7, 0x7B, 0x5F,
	0xB7, 0x87, 0x6B, 0xC3, 0x93, 0x7B, 0xD3, 0xA3, 0x8B, 0xE3, 0xB3, 0x97,
	0xAB, 0x8B, 0xA3, 0x9F, 0x7F, 0x97, 0x93, 0x73, 0x87, 0x8B, 0x67, 0x7B,
	0x7F, 0x5B, 0x6F, 0x77, 0x53, 0x63, 0x6B, 0x4B, 0x57, 0x5F, 0x3F, 0x4B,
	0x57, 0x37, 0x43, 0x4B, 0x2F, 0x37, 0x43, 0x27, 0x2F, 0x37, 0x1F, 0x23,
	0x2B, 0x17, 0x1B, 0x23, 0x13, 0x13, 0x17, 0x0B, 0x0B, 0x0F, 0x07, 0x07,
	0xBB, 0x73, 0x9F, 0xAF, 0x6B, 0x8F, 0xA3, 0x5F, 0x83, 0x97, 0x57, 0x77,
	0x8B, 0x4F, 0x6B, 0x7F, 0x4B, 0x5F, 0x73, 0x43, 0x53, 0x6B, 0x3B, 0x4B,
	0x5F, 0x33, 0x3F, 0x53, 0x2B, 0x37, 0x47, 0x23, 0x2B, 0x3B, 0x1F, 0x23,
	0x2F, 0x17, 0x1B, 0x23, 0x13, 0x13, 0x17, 0x0B, 0x0B, 0x0F, 0x07, 0x07,
	0xDB, 0xC3, 0xBB, 0xCB, 0xB3, 0xA7, 0xBF, 0xA3, 0x9B, 0xAF, 0x97, 0x8B,
	0xA3, 0x87, 0x7B, 0x97, 0x7B, 0x6F, 0x87, 0x6F, 0x5F, 0x7B, 0x63, 0x53,
	0x6B, 0x57, 0x47, 0x5F, 0x4B, 0x3B, 0x53, 0x3F, 0x33, 0x43, 0x33, 0x27,
	0x37, 0x2B, 0x1F, 0x27, 0x1F, 0x17, 0x1B, 0x13, 0x0F, 0x0F, 0x0B, 0x07,
	0x6F, 0x83, 0x7B, 0x67, 0x7B, 0x6F, 0x5F, 0x73, 0x67, 0x57, 0x6B, 0x5F,
	0x4F, 0x63, 0x57, 0x47, 0x5B, 0x4F, 0x3F, 0x53, 0x47, 0x37, 0x4B, 0x3F,
	0x2F, 0x43, 0x37, 0x2B, 0x3B, 0x2F, 0x23, 0x33, 0x27, 0x1F, 0x2B, 0x1F,
	0x17, 0x23, 0x17, 0x0F, 0x1B, 0x13, 0x0B, 0x13, 0x0B, 0x07, 0x0B, 0x07,
	0xFF, 0xF3, 0x1B, 0xEF, 0xDF, 0x17, 0xDB, 0xCB, 0x13, 0xCB, 0xB7, 0x0F,
	0xBB, 0xA7, 0x0F, 0xAB, 0x97, 0x0B, 0x9B, 0x83, 0x07, 0x8B, 0x73, 0x07,
	0x7B, 0x63, 0x07, 0x6B, 0x53, 0x00, 0x5B, 0x47, 0x00, 0x4B, 0x37, 0x00,
	0x3B, 0x2B, 0x00, 0x2B, 0x1F, 0x00, 0x1B, 0x0F, 0x00, 0x0B, 0x07, 0x00,
	0x00, 0x00, 0xFF, 0x0B, 0x0B, 0xEF, 0x13, 0x13, 0xDF, 0x1B, 0x1B, 0xCF,
	0x23, 0x23, 0xBF, 0x2B, 0x2B, 0xAF, 0x2F, 0x2F, 0x9F, 0x2F, 0x2F, 0x8F,
	0x2F, 0x2F, 0x7F, 0x2F, 0x2F, 0x6F, 0x2F, 0x2F, 0x5F, 0x2B, 0x2B, 0x4F,
	0x23, 0x23, 0x3F, 0x1B, 0x1B, 0x2F, 0x13, 0x13, 0x1F, 0x0B, 0x0B, 0x0F,
	0x2B, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x4B, 0x07, 0x00, 0x5F, 0x07, 0x00,
	0x6F, 0x0F, 0x00, 0x7F, 0x17, 0x07, 0x93, 0x1F, 0x07, 0xA3, 0x27, 0x0B,
	0xB7, 0x33, 0x0F, 0xC3, 0x4B, 0x1B, 0xCF, 0x63, 0x2B, 0xDB, 0x7F, 0x3B,
	0xE3, 0x97, 0x4F, 0xE7, 0xAB, 0x5F, 0xEF, 0xBF, 0x77, 0xF7, 0xD3, 0x8B,
	0xA7, 0x7B, 0x3B, 0xB7, 0x9B, 0x37, 0xC7, 0xC3, 0x37, 0xE7, 0xE3, 0x57,
	0x7F, 0xBF, 0xFF, 0xAB, 0xE7, 0xFF, 0xD7, 0xFF, 0xFF, 0x67, 0x00, 0x00,
	0x8B, 0x00, 0x00, 0xB3, 0x00, 0x00, 0xD7, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0xF3, 0x93, 0xFF, 0xF7, 0xC7, 0xFF, 0xFF, 0xFF, 0x9F, 0x5B, 0x53,
};

static const char *
unique_name (wad_t *wad, const char *name)
{
	char        uname[MIPTEXNAME];
	int         i = 0;
	const char *tag;

	if (!wad_find_lump (wad, name))
		return name;
	do {
		strncpy (uname, name, MIPTEXNAME);
		uname[(MIPTEXNAME - 1)] = 0;
		tag = va (0, "~%x", i++);
		if (strlen (uname) + strlen (tag) <= (MIPTEXNAME - 1))
			strcat (uname, tag);
		else
			strcpy (uname + (MIPTEXNAME - 1) - strlen (tag), tag);
	} while (wad_find_lump (wad, uname));
	return va (0, "%s", uname);	// just to make a safe returnable that doesn't
								// need to be freed
}

void
extract_textures (void)
{
	const dmiptexlump_t *miptexlump = (dmiptexlump_t *) bsp->texdata;
	miptex_t   *miptex;
	int         mtsize, pixels;
	const char *wadfile;
	wad_t      *wad;
	const char *uname;

	wadfile = output_file (".wad");

	wad = wad_create (wadfile);

	wad_add_data (wad, "PALETTE", TYP_PALETTE, default_palette,
				  sizeof (default_palette));

	for (size_t i = 0; i < miptexlump->nummiptex; i++) {
		if (miptexlump->dataofs[i] == ~0u)
			continue;
		miptex = (miptex_t *)(bsp->texdata + miptexlump->dataofs[i]);
		pixels = miptex->width * miptex->height / 64 * 85;
		mtsize = sizeof (miptex_t) + pixels;
		uname = unique_name (wad, miptex->name);
#if 1
		printf ("%3zd %6d ", i, miptexlump->dataofs[i]);
		printf ("%16.16s %16.16s %3dx%-3d %d %d %d %d %d %d\n",
				miptex->name, uname, miptex->width,
				miptex->height, miptex->offsets[0], miptex->offsets[1],
				miptex->offsets[2], miptex->offsets[3], pixels, mtsize);
#endif
		wad_add_data (wad, uname, TYP_MIPTEX, miptex, mtsize);
	}
	wad_close (wad);
}

void
extract_entities (void)
{
	const char *entfile;
	int         i;
	QFile      *ef;

	entfile = output_file (".ent");

	for (i = bsp->entdatasize; i > 0; i--)
		if (bsp->entdata[i - 1])
			break;

	if (strcmp (entfile, "-") == 0)
		ef = Qdopen (1, "wt");
	else
		ef = Qopen (entfile, "wt");
	Qwrite (ef, bsp->entdata, i);
	Qclose (ef);
}

void
extract_hull (void)
{
//	hullfile = output_file (".c");
	const char *hullfile;
	QFile      *hf;

	hullfile = output_file (".c");
	if (strcmp (hullfile, "-") == 0)
		hf = Qdopen (1, "wt");
	else
		hf = Qopen (hullfile, "wt");

	printf ("%zd\n", bsp->nummodels);
	for (size_t i = 0; i < bsp->nummodels; i++) {
		dmodel_t   *m = bsp->models + i;
		printf ("mins: (%g, %g, %g)\n", m->mins[0], m->mins[1], m->mins[2]);
		printf ("maxs: (%g, %g, %g)\n", m->maxs[0], m->maxs[1], m->maxs[2]);
		printf ("origin: (%g, %g, %g)\n",
				m->origin[0], m->origin[1], m->origin[2]);
		for (int j = 0; j < MAX_MAP_HULLS; j++)
			printf ("headnodes[%d]: %d\n", j, m->headnode[j]);
		printf ("visleafs: %d\n", m->visleafs);
		printf ("firstface: %d\n", m->firstface);
		printf ("numfaces: %d\n", m->numfaces);
		printf ("\n");
	}
	Qprintf (hf, "dclipnode_t clipnodes[] = {\n");
	for (size_t i = 0; i < bsp->numnodes; i++) {
		int         c0, c1;
		c0 = bsp->nodes[i].children[0];
		c1 = bsp->nodes[i].children[1];
		if (c0 < 0)
			c0 = bsp->leafs[-1 - c0].contents;
		if (c1 < 0)
			c1 = bsp->leafs[-1 - c1].contents;
		Qprintf (hf, "\t{%d, {%d, %d}},\t// %zd\n", bsp->nodes[i].planenum,
				 c0, c1, i);
	}
	Qprintf (hf, "};\n");
	Qprintf (hf, "mplane_t planes[] = {\n");
	for (size_t i = 0; i < bsp->numplanes; i++) {
		Qprintf (hf, "\t{{%g, %g, %g}, %g, %d, 0, {0, 0}},\t// %zd\n",
				 bsp->planes[i].normal[0], bsp->planes[i].normal[1],
				 bsp->planes[i].normal[2],
				 bsp->planes[i].dist, bsp->planes[i].type,
				 i);
	}
	Qprintf (hf, "};\n");
}

static void
setparent (int32_t node_id, int32_t parent_id,
		   int32_t *leaf_parents, int32_t *node_parents)
{
	if (node_id < 0) {
		leaf_parents[~node_id] = parent_id;
		return;
	}
	node_parents[node_id] = parent_id;
	auto node = bsp->nodes + node_id;
	setparent (node->children[0], node_id, leaf_parents, node_parents);
	setparent (node->children[1], node_id, leaf_parents, node_parents);
}

void
extract_model (void)
{
//	hullfile = output_file (".c");
	const char *hullfile;
	QFile      *hf;

	hullfile = output_file (".c");
	if (strcmp (hullfile, "-") == 0)
		hf = Qdopen (1, "wt");
	else
		hf = Qopen (hullfile, "wt");

	Qprintf (hf, "static mleaf_t leafs[] = {\n");
	// skip leaf 0
	for (uint32_t i = 1; i < bsp->numleafs; i++) {
		auto leaf = bsp->leafs[i];
		Qprintf (hf, "\t[%d] = {\n", i);
		Qprintf (hf, "\t\t.contents = %d,\n", leaf.contents);
		Qprintf (hf, "\t\t.mins = { %g, %g, %g },\n", VectorExpand (leaf.mins));
		Qprintf (hf, "\t\t.maxs = { %g, %g, %g },\n", VectorExpand (leaf.maxs));
		//FIXME vis
		Qprintf (hf, "\t},\n");
	}
	Qprintf (hf, "};\n");
	Qprintf (hf, "\n");
	Qprintf (hf, "static mnode_t nodes[] = {\n");
	for (uint32_t i = 0; i < bsp->numnodes; i++) {
		auto node = bsp->nodes[i];
		auto plane = bsp->planes[node.planenum];
		Qprintf (hf, "\t[%d] = {\n", i);
		Qprintf (hf, "\t\t.plane = { %g, %g, %g, %g },\n",
				 VectorExpand (plane.normal), -plane.dist);
		Qprintf (hf, "\t\t.type = %d,\n", plane.type);
		Qprintf (hf, "\t\t.children = { %d, %d },\n",
				 node.children[0], node.children[1]);
		Qprintf (hf, "\t\t.minmaxs = { %g, %g, %g,\n",
				 VectorExpand (node.maxs));
		Qprintf (hf, "\t\t\t\t\t %g, %g, %g },\n", VectorExpand (node.maxs));
		Qprintf (hf, "\t},\n");
	}
	Qprintf (hf, "};\n");
	Qprintf (hf, "\n");

	int32_t *leaf_parents = malloc (sizeof (int32_t[bsp->numleafs]));
	int32_t *node_parents = malloc (sizeof (int32_t[bsp->numnodes]));
	setparent (0, -1, leaf_parents, node_parents);
	Qprintf (hf, "static int32_t leaf_parents[] = {\n");
	for (uint32_t i = 0; i < bsp->numleafs; i++) {
		Qprintf (hf, "\t[%d] = %d,\n", i, leaf_parents[i]);
	}
	Qprintf (hf, "};\n");
	Qprintf (hf, "\n");
	Qprintf (hf, "static int32_t node_parents[] = {\n");
	for (uint32_t i = 0; i < bsp->numnodes; i++) {
		Qprintf (hf, "\t[%d] = %d,\n", i, node_parents[i]);
	}
	Qprintf (hf, "};\n");
	Qprintf (hf, "\n");
	Qprintf (hf, "static int32_t leaf_flags[] = {\n");
	Qprintf (hf, "\t//FIXME\n");
	Qprintf (hf, "};\n");
	Qprintf (hf, "\n");
	Qprintf (hf, "static char entities[] = { 0 };\n");

	Qprintf (hf, "\n");
	Qprintf (hf, "static model_t models[] = {\n");
	for (uint32_t i = 0; i < bsp->nummodels; i++) {
		auto model = bsp->models[i];
		float r1 = sqrt (DotProduct (model.mins, model.mins));
		float r2 = sqrt (DotProduct (model.maxs, model.maxs));
		Qprintf (hf, "\t[%d] = {\n", i);
		Qprintf (hf, "\t\t.type = mod_brush,\n");
		Qprintf (hf, "\t\t.radius = %g,\n", max (r1, r2));
		Qprintf (hf, "\t\t.mins = { %g, %g, %g },\n", VectorExpand(model.maxs));
		Qprintf (hf, "\t\t.maxs = { %g, %g, %g },\n", VectorExpand(model.maxs));
		Qprintf (hf, "\t\t.brush = {\n");
		Qprintf (hf, "\t\t\t.modleafs = %d,\n", model.visleafs + 1);
		Qprintf (hf, "\t\t\t.visleafs = %d,\n", model.visleafs);
		Qprintf (hf, "\t\t\t.numnodes = %zd,\n", bsp->numnodes);
		Qprintf (hf, "\t\t\t.nodes = nodes,\n");
		Qprintf (hf, "\t\t\t.leafs = leafs,\n");
		Qprintf (hf, "\t\t\t.entities = entities,\n");
		Qprintf (hf, "\t\t\t.leaf_parents = leaf_parents,\n");
		Qprintf (hf, "\t\t\t.node_parents = node_parents,\n");
		Qprintf (hf, "\t\t\t.leaf_flags = leaf_flags,\n");
		Qprintf (hf, "\t\t},\n");
		Qprintf (hf, "\t},\n");
	}
	Qprintf (hf, "};\n");
}

//@}
