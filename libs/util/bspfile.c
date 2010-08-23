/*
	bspfile.c

	DESCRIPTION
	
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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/bspfile.h"
#include "QF/qendian.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

static void
swap_bsp (bsp_t *bsp, int todisk)
{
	int             c, i, j;
	dmiptexlump_t  *mtl;
	dmodel_t       *d;

	if (bsp->header) {
		bsp->header->version = LittleLong (bsp->header->version);
		for (i = 0; i < HEADER_LUMPS; i++) {
			bsp->header->lumps[i].fileofs =
				LittleLong (bsp->header->lumps[i].fileofs);
			bsp->header->lumps[i].filelen =
				LittleLong (bsp->header->lumps[i].filelen);
		}
	}

	// models
	for (i=0 ; i<bsp->nummodels ; i++) {
		d = &bsp->models[i];

		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			d->headnode[j] = LittleLong (d->headnode[j]);

		d->visleafs = LittleLong (d->visleafs);
		d->firstface = LittleLong (d->firstface);
		d->numfaces = LittleLong (d->numfaces);
		
		for (j=0 ; j<3 ; j++) {
			d->mins[j] = LittleFloat(d->mins[j]);
			d->maxs[j] = LittleFloat(d->maxs[j]);
			d->origin[j] = LittleFloat(d->origin[j]);
		}
	}

	// vertexes
	for (i=0 ; i<bsp->numvertexes ; i++) {
		dvertex_t  *vertex = &bsp->vertexes[i];
		for (j=0 ; j<3 ; j++)
			vertex->point[j] = LittleFloat (vertex->point[j]);
	}
		
	// planes
	for (i=0 ; i<bsp->numplanes ; i++) {
		dplane_t   *plane = &bsp->planes[i];
		for (j=0 ; j<3 ; j++)
			plane->normal[j] = LittleFloat (plane->normal[j]);
		plane->dist = LittleFloat (plane->dist);
		plane->type = LittleLong (plane->type);
	}
	
	// texinfos
	for (i=0 ; i<bsp->numtexinfo ; i++) {
		texinfo_t  *texinfo = &bsp->texinfo[i];
		for (j=0 ; j<8 ; j++)
			texinfo->vecs[0][j] = LittleFloat (texinfo->vecs[0][j]);
		texinfo->miptex = LittleLong (texinfo->miptex);
		texinfo->flags = LittleLong (texinfo->flags);
	}
	
	// faces
	for (i=0 ; i<bsp->numfaces ; i++) {
		dface_t    *face = &bsp->faces[i];
		face->texinfo = LittleShort (face->texinfo);
		face->planenum = LittleShort (face->planenum);
		face->side = LittleShort (face->side);
		face->lightofs = LittleLong (face->lightofs);
		face->firstedge = LittleLong (face->firstedge);
		face->numedges = LittleShort (face->numedges);
	}

	// nodes
	for (i=0 ; i<bsp->numnodes ; i++) {
		dnode_t    *node = &bsp->nodes[i];
		node->planenum = LittleLong (node->planenum);
		for (j=0 ; j<3 ; j++) {
			node->mins[j] = LittleShort (node->mins[j]);
			node->maxs[j] = LittleShort (node->maxs[j]);
		}
		node->children[0] = LittleShort (node->children[0]);
		node->children[1] = LittleShort (node->children[1]);
		node->firstface = LittleShort (node->firstface);
		node->numfaces = LittleShort (node->numfaces);
	}

	// leafs
	for (i=0 ; i<bsp->numleafs ; i++) {
		dleaf_t    *leaf = &bsp->leafs[i];
		leaf->contents = LittleLong (leaf->contents);
		for (j=0 ; j<3 ; j++) {
			leaf->mins[j] = LittleShort (leaf->mins[j]);
			leaf->maxs[j] = LittleShort (leaf->maxs[j]);
		}

		leaf->firstmarksurface = LittleShort (leaf->firstmarksurface);
		leaf->nummarksurfaces = LittleShort (leaf->nummarksurfaces);
		leaf->visofs = LittleLong (leaf->visofs);
	}

	// clipnodes
	for (i=0 ; i<bsp->numclipnodes ; i++) {
		dclipnode_t *clipnode = &bsp->clipnodes[i];
		clipnode->planenum = LittleLong (clipnode->planenum);
		clipnode->children[0] = LittleShort (clipnode->children[0]);
		clipnode->children[1] = LittleShort (clipnode->children[1]);
	}

	// miptex
	if (bsp->texdatasize) {
		mtl = (dmiptexlump_t *)bsp->texdata;
		if (todisk)
			c = mtl->nummiptex;
		else
			c = LittleLong(mtl->nummiptex);
		mtl->nummiptex = LittleLong (mtl->nummiptex);
		for (i=0 ; i<c ; i++)
			mtl->dataofs[i] = LittleLong(mtl->dataofs[i]);
	}
	
	// marksurfaces
	for (i=0 ; i<bsp->nummarksurfaces ; i++) {
		uint16_t   *marksurface = &bsp->marksurfaces[i];
		*marksurface = LittleShort (*marksurface);
	}

	// surfedges
	for (i=0 ; i<bsp->numsurfedges ; i++) {
		int32_t    *surfedge = &bsp->surfedges[i];
		*surfedge = LittleLong (*surfedge);
	}

	// edges
	for (i=0 ; i<bsp->numedges ; i++) {
		dedge_t    *edge = &bsp->edges[i];
		edge->v[0] = LittleShort (edge->v[0]);
		edge->v[1] = LittleShort (edge->v[1]);
	}
}

bsp_t *
LoadBSPMem (void *mem, size_t mem_size)
{
	byte       *data = mem;
	bsp_t      *bsp;
	size_t      size;

	bsp = calloc (sizeof (bsp_t), 1);

	bsp->header = mem;

	if (LittleLong (bsp->header->version) != BSPVERSION)
		Sys_Error ("version %i, not %i", LittleLong (bsp->header->version),
				   BSPVERSION);

#undef SET_LUMP
#define SET_LUMP(l,n) \
do { \
	size = LittleLong (bsp->header->lumps[l].filelen); \
	data = (byte *) mem + LittleLong (bsp->header->lumps[l].fileofs); \
	if ((data + size) > ((byte *) mem + mem_size)) { \
		Sys_Error ("invalid lump"); \
	} \
	bsp->n = 0; \
	if (size) \
		bsp->n = (void *) data; \
	bsp->num##n = size / sizeof (bsp->n[0]); \
} while (0)

	SET_LUMP (LUMP_PLANES, planes);
	SET_LUMP (LUMP_LEAFS, leafs);
	SET_LUMP (LUMP_VERTEXES, vertexes);
	SET_LUMP (LUMP_NODES, nodes);
	SET_LUMP (LUMP_TEXINFO, texinfo);
	SET_LUMP (LUMP_FACES, faces);
	SET_LUMP (LUMP_CLIPNODES, clipnodes);
	SET_LUMP (LUMP_MARKSURFACES, marksurfaces);
	SET_LUMP (LUMP_SURFEDGES, surfedges);
	SET_LUMP (LUMP_EDGES, edges);
	SET_LUMP (LUMP_MODELS, models);

#undef SET_LUMP
#define SET_LUMP(l,n) \
do { \
	size = LittleLong (bsp->header->lumps[l].filelen); \
	data = (byte *) mem + LittleLong (bsp->header->lumps[l].fileofs); \
	if ((data + size) > ((byte *) mem + mem_size)) { \
		Sys_Error ("invalid lump"); \
	} \
	bsp->n = 0; \
	if (size) \
		bsp->n = (void *) data; \
	bsp->n##size = size; \
} while (0)

	SET_LUMP (LUMP_LIGHTING, lightdata);
	SET_LUMP (LUMP_VISIBILITY, visdata);
	SET_LUMP (LUMP_ENTITIES, entdata);
	SET_LUMP (LUMP_TEXTURES, texdata);

	swap_bsp (bsp, 0);
	return bsp;
}

VISIBLE bsp_t *
LoadBSPFile (QFile *file, size_t size)
{
	void       *buf;
	bsp_t      *bsp;

	buf = malloc (size);
	Qread (file, buf, size);
	bsp = LoadBSPMem (buf, size);
	return bsp;
}

/*
	WriteBSPFile
*/
VISIBLE void
WriteBSPFile (const bsp_t *bsp, QFile *file)
{
	size_t      size;
	byte       *data;
	bsp_t       tbsp;

#define ROUND(x) (((x) + 3) & ~3)

	size = ROUND (sizeof (dheader_t));
	size += ROUND (bsp->nummodels * sizeof (dmodel_t));
	size += ROUND (bsp->visdatasize);
	size += ROUND (bsp->lightdatasize);
	size += ROUND (bsp->texdatasize);
	size += ROUND (bsp->entdatasize);
	size += ROUND (bsp->numleafs * sizeof (dleaf_t));
	size += ROUND (bsp->numplanes * sizeof (dplane_t));
	size += ROUND (bsp->numvertexes * sizeof (dvertex_t));
	size += ROUND (bsp->numnodes * sizeof (dnode_t));
	size += ROUND (bsp->numtexinfo * sizeof (texinfo_t));
	size += ROUND (bsp->numfaces * sizeof (dface_t));
	size += ROUND (bsp->numclipnodes * sizeof (dclipnode_t));
	size += ROUND (bsp->numedges * sizeof (dedge_t));
	size += ROUND (bsp->nummarksurfaces * sizeof (uint16_t));
	size += ROUND (bsp->numsurfedges * sizeof (uint32_t));

	tbsp.header = calloc (size, 1);

#undef SET_LUMP
#define SET_LUMP(l,n) \
do { \
	tbsp.num##n = bsp->num##n; \
	if (tbsp.num##n) {\
		tbsp.n = (void *) data; \
		tbsp.header->lumps[l].fileofs = data - (byte *) tbsp.header; \
		tbsp.header->lumps[l].filelen = tbsp.num##n * sizeof (bsp->n[0]); \
		memcpy (data, bsp->n, tbsp.header->lumps[l].filelen); \
		data += ROUND (tbsp.header->lumps[l].filelen); \
	} else {\
		tbsp.n = 0; \
		tbsp.header->lumps[l].fileofs = 0; \
		tbsp.header->lumps[l].filelen = 0; \
	} \
} while (0)

	tbsp.header->version = BSPVERSION;

	data = (byte *) &tbsp.header[1];
	SET_LUMP (LUMP_PLANES, planes);
	SET_LUMP (LUMP_LEAFS, leafs);
	SET_LUMP (LUMP_VERTEXES, vertexes);
	SET_LUMP (LUMP_NODES, nodes);
	SET_LUMP (LUMP_TEXINFO, texinfo);
	SET_LUMP (LUMP_FACES, faces);
	SET_LUMP (LUMP_CLIPNODES, clipnodes);
	SET_LUMP (LUMP_MARKSURFACES, marksurfaces);
	SET_LUMP (LUMP_SURFEDGES, surfedges);
	SET_LUMP (LUMP_EDGES, edges);
	SET_LUMP (LUMP_MODELS, models);

#undef SET_LUMP
#define SET_LUMP(l,n) \
do { \
	tbsp.n##size = bsp->n##size; \
	if (tbsp.n##size) { \
		tbsp.n = (void *) data; \
		tbsp.header->lumps[l].fileofs = data - (byte *) tbsp.header; \
		tbsp.header->lumps[l].filelen = tbsp.n##size; \
		memcpy (data, bsp->n, bsp->n##size); \
		data += ROUND (tbsp.header->lumps[l].filelen); \
	} else {\
		tbsp.n = 0; \
		tbsp.header->lumps[l].fileofs = 0; \
		tbsp.header->lumps[l].filelen = 0; \
	} \
} while (0)

	SET_LUMP (LUMP_LIGHTING, lightdata);
	SET_LUMP (LUMP_VISIBILITY, visdata);
	SET_LUMP (LUMP_ENTITIES, entdata);
	SET_LUMP (LUMP_TEXTURES, texdata);

	swap_bsp (&tbsp, 1);

	Qwrite (file, tbsp.header, size);
	free (tbsp.header);
}

VISIBLE bsp_t *
BSP_New (void)
{
	return calloc (1, sizeof (bsp_t));
}

VISIBLE void
BSP_Free (bsp_t *bsp)
{
#define FREE(X) \
	do { \
		if (bsp->own_##X && bsp->X) \
			free (bsp->X); \
	} while (0)

	FREE (models);
	FREE (visdata);
	FREE (lightdata);
	FREE (texdata);
	FREE (entdata);
	FREE (leafs);
	FREE (planes);
	FREE (vertexes);
	FREE (nodes);
	FREE (texinfo);
	FREE (faces);
	FREE (clipnodes);
	FREE (edges);
	FREE (marksurfaces);
	FREE (surfedges);
	FREE (header);

	free (bsp);
}

#define REALLOC(X) \
	do { \
		if (!bsp->own_##X) { \
			bsp->own_##X = 1; \
			bsp->X = 0; \
		} \
		bsp->X = realloc (bsp->X, (bsp->num##X + 1) * sizeof (bsp->X[0])); \
	} while (0)

VISIBLE void
BSP_AddPlane (bsp_t *bsp, dplane_t *plane)
{
	REALLOC (planes);
	bsp->planes[bsp->numplanes++] = *plane;
}

VISIBLE void
BSP_AddLeaf (bsp_t *bsp, dleaf_t *leaf)
{
	REALLOC (leafs);
	bsp->leafs[bsp->numleafs++] = *leaf;
}

VISIBLE void
BSP_AddVertex (bsp_t *bsp, dvertex_t *vertex)
{
	REALLOC (vertexes);
	bsp->vertexes[bsp->numvertexes++] = *vertex;
}

VISIBLE void
BSP_AddNode (bsp_t *bsp, dnode_t *node)
{
	REALLOC (nodes);
	bsp->nodes[bsp->numnodes++] = *node;
}

VISIBLE void
BSP_AddTexinfo (bsp_t *bsp, texinfo_t *texinfo)
{
	REALLOC (texinfo);
	bsp->texinfo[bsp->numtexinfo++] = *texinfo;
}

VISIBLE void
BSP_AddFace (bsp_t *bsp, dface_t *face)
{
	REALLOC (faces);
	bsp->faces[bsp->numfaces++] = *face;
}

VISIBLE void
BSP_AddClipnode (bsp_t *bsp, dclipnode_t *clipnode)
{
	REALLOC (clipnodes);
	bsp->clipnodes[bsp->numclipnodes++] = *clipnode;
}

VISIBLE void
BSP_AddMarkSurface (bsp_t *bsp, int marksurface)
{
	REALLOC (marksurfaces);
	bsp->marksurfaces[bsp->nummarksurfaces++] = marksurface;
}

VISIBLE void
BSP_AddSurfEdge (bsp_t *bsp, int surfedge)
{
	REALLOC (surfedges);
	bsp->surfedges[bsp->numsurfedges++] = surfedge;
}

VISIBLE void
BSP_AddEdge (bsp_t *bsp, dedge_t *edge)
{
	REALLOC (edges);
	bsp->edges[bsp->numedges++] = *edge;
}

VISIBLE void
BSP_AddModel (bsp_t *bsp, dmodel_t *model)
{
	REALLOC (models);
	bsp->models[bsp->nummodels++] = *model;
}

#define OWN(X) \
	do { \
		FREE(X); \
		bsp->own_##X = 1; \
	} while (0)

VISIBLE void
BSP_AddLighting (bsp_t *bsp, byte *lightdata, size_t lightdatasize)
{
	OWN (lightdata);
	bsp->lightdatasize = lightdatasize;
	bsp->lightdata = malloc (lightdatasize);
	memcpy (bsp->lightdata, lightdata, lightdatasize);
}

VISIBLE void
BSP_AddVisibility (bsp_t *bsp, byte *visdata, size_t visdatasize)
{
	OWN (visdata);
	bsp->visdatasize = visdatasize;
	bsp->visdata = malloc (visdatasize);
	memcpy (bsp->visdata, visdata, visdatasize);
}

VISIBLE void
BSP_AddEntities (bsp_t *bsp, char *entdata, size_t entdatasize)
{
	OWN (entdata);
	bsp->entdatasize = entdatasize;
	bsp->entdata = malloc (entdatasize);
	memcpy (bsp->entdata, entdata, entdatasize);
}

VISIBLE void
BSP_AddTextures (bsp_t *bsp, byte *texdata, size_t texdatasize)
{
	OWN (texdata);
	bsp->texdatasize = texdatasize;
	bsp->texdata = malloc (texdatasize);
	memcpy (bsp->texdata, texdata, texdatasize);
}
