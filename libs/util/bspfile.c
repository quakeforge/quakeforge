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

typedef struct dnode29_s {
	int32_t     planenum;
	int16_t     children[2];	// negative numbers are -(leafs+1), not nodes
	int16_t     mins[3];		// for sphere culling
	int16_t     maxs[3];
	uint16_t    firstface;
	uint16_t    numfaces;		// counting both sides
} dnode29_t;

typedef struct dclipnode29_s {
	int32_t     planenum;
	int16_t     children[2];	// negative numbers are contents
} dclipnode29_t;

typedef struct dedge29_s {
	uint16_t    v[2];			// vertex numbers
} dedge29_t;

typedef struct dface29_s {
	int16_t     planenum;
	int16_t     side;

	int32_t     firstedge;		// we must support > 64k edges
	int16_t     numedges;
	int16_t     texinfo;

// lighting info
	byte        styles[MAXLIGHTMAPS];
	int32_t     lightofs;		// start of [numstyles*surfsize] samples
} dface29_t;

typedef struct dleaf29_s {
	int32_t     contents;
	int32_t     visofs;				// -1 = no visibility info

	int16_t     mins[3];			// for frustum culling
	int16_t     maxs[3];

	uint16_t    firstmarksurface;
	uint16_t    nummarksurfaces;

	byte        ambient_level[NUM_AMBIENTS];
} dleaf29_t;

typedef struct bsp29_s {
	int         own_header;
	dheader_t  *header;

	int         own_models;
	int         nummodels;
	dmodel_t   *models;

	int         own_visdata;
	size_t      visdatasize;
	byte       *visdata;

	int         own_lightdata;
	size_t      lightdatasize;
	byte       *lightdata;

	int         own_texdata;
	size_t      texdatasize;
	byte       *texdata;			// (dmiptexlump_t)

	int         own_entdata;
	size_t      entdatasize;
	char       *entdata;

	int         own_leafs;
	int         numleafs;
	dleaf29_t    *leafs;

	int         own_planes;
	int         numplanes;
	dplane_t   *planes;

	int         own_vertexes;
	int         numvertexes;
	dvertex_t  *vertexes;

	int         own_nodes;
	int         numnodes;
	dnode29_t  *nodes;

	int         own_texinfo;
	int         numtexinfo;
	texinfo_t  *texinfo;

	int         own_faces;
	int         numfaces;
	dface29_t    *faces;

	int         own_clipnodes;
	int         numclipnodes;
	dclipnode29_t *clipnodes;

	int         own_edges;
	int         numedges;
	dedge29_t  *edges;

	int         own_marksurfaces;
	int         nummarksurfaces;
	uint16_t   *marksurfaces;

	int         own_surfedges;
	int         numsurfedges;
	int32_t    *surfedges;
} bsp29_t;

static void
swap_to_bsp29 (bsp29_t *bsp29, const bsp_t *bsp2)
{
	int             c, i, j;
	dmiptexlump_t  *mtl;
	dmodel_t       *d;

	if (bsp29->header) {
		bsp29->header->version = LittleLong (bsp29->header->version);
		for (i = 0; i < HEADER_LUMPS; i++) {
			bsp29->header->lumps[i].fileofs =
				LittleLong (bsp29->header->lumps[i].fileofs);
			bsp29->header->lumps[i].filelen =
				LittleLong (bsp29->header->lumps[i].filelen);
		}
	}

	// models
	for (i=0 ; i<bsp29->nummodels ; i++) {
		d = &bsp29->models[i];

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
	for (i=0 ; i<bsp29->numvertexes ; i++) {
		dvertex_t  *vertex = &bsp29->vertexes[i];
		for (j=0 ; j<3 ; j++)
			vertex->point[j] = LittleFloat (vertex->point[j]);
	}

	// planes
	for (i=0 ; i<bsp29->numplanes ; i++) {
		dplane_t   *plane = &bsp29->planes[i];
		for (j=0 ; j<3 ; j++)
			plane->normal[j] = LittleFloat (plane->normal[j]);
		plane->dist = LittleFloat (plane->dist);
		plane->type = LittleLong (plane->type);
	}

	// texinfos
	for (i=0 ; i<bsp29->numtexinfo ; i++) {
		texinfo_t  *texinfo = &bsp29->texinfo[i];
		for (j=0 ; j < 4 ; j++) {
			texinfo->vecs[0][j] = LittleFloat (texinfo->vecs[0][j]);
			texinfo->vecs[1][j] = LittleFloat (texinfo->vecs[1][j]);
		}
		texinfo->miptex = LittleLong (texinfo->miptex);
		texinfo->flags = LittleLong (texinfo->flags);
	}

	// faces
	for (i=0 ; i<bsp29->numfaces ; i++) {
		const dface_t *face2 = &bsp2->faces[i];
		dface29_t  *face29 = &bsp29->faces[i];
		face29->planenum = LittleShort (face2->planenum);
		face29->side = LittleShort (face2->side);
		face29->firstedge = LittleLong (face2->firstedge);
		face29->numedges = LittleShort (face2->numedges);
		face29->texinfo = LittleShort (face2->texinfo);
		memcpy (face29->styles, face2->styles, sizeof(face29->styles));
		face29->lightofs = LittleLong (face2->lightofs);
	}

	// nodes
	for (i=0 ; i<bsp29->numnodes ; i++) {
		const dnode_t *node2 = &bsp2->nodes[i];
		dnode29_t  *node29 = &bsp29->nodes[i];
		node29->planenum = LittleLong (node2->planenum);
		for (j=0 ; j<3 ; j++) {
			node29->mins[j] = LittleShort ((int16_t) node2->mins[j]);
			node29->maxs[j] = LittleShort ((int16_t) node2->maxs[j]);
		}
		node29->children[0] = LittleShort (node2->children[0]);
		node29->children[1] = LittleShort (node2->children[1]);
		node29->firstface = LittleShort (node2->firstface);
		node29->numfaces = LittleShort (node2->numfaces);
	}

	// leafs
	for (i=0 ; i<bsp29->numleafs ; i++) {
		const dleaf_t *leaf2 = &bsp2->leafs[i];
		dleaf29_t  *leaf29 = &bsp29->leafs[i];
		leaf29->contents = LittleLong (leaf2->contents);
		for (j=0 ; j<3 ; j++) {
			leaf29->mins[j] = LittleShort ((int16_t) leaf2->mins[j]);
			leaf29->maxs[j] = LittleShort ((int16_t) leaf2->maxs[j]);
		}

		leaf29->firstmarksurface = LittleShort (leaf2->firstmarksurface);
		leaf29->nummarksurfaces = LittleShort (leaf2->nummarksurfaces);
		leaf29->visofs = LittleLong (leaf2->visofs);
		memcpy (leaf29->ambient_level, leaf2->ambient_level,
				sizeof(leaf29->ambient_level));
	}

	// clipnodes
	for (i=0 ; i<bsp29->numclipnodes ; i++) {
		const dclipnode_t *clipnode2 = &bsp2->clipnodes[i];
		dclipnode29_t *clipnode29 = (dclipnode29_t *) &bsp29->clipnodes[i];
		clipnode29->planenum = LittleLong (clipnode2->planenum);
		clipnode29->children[0] = LittleShort (clipnode2->children[0]);
		clipnode29->children[1] = LittleShort (clipnode2->children[1]);
	}

	// miptex
	if (bsp29->texdatasize) {
		mtl = (dmiptexlump_t *)bsp29->texdata;
		//miptexlumps have not yet been swapped
		c = mtl->nummiptex;
		mtl->nummiptex = LittleLong (mtl->nummiptex);
		for (i=0 ; i<c ; i++)
			mtl->dataofs[i] = LittleLong(mtl->dataofs[i]);
	}

	// marksurfaces
	for (i=0 ; i<bsp29->nummarksurfaces ; i++) {
		const uint32_t *marksurface2 = &bsp2->marksurfaces[i];
		uint16_t   *marksurface29 = (uint16_t *) &bsp29->marksurfaces[i];
		*marksurface29 = LittleShort (*marksurface2);
	}

	// surfedges
	for (i=0 ; i<bsp29->numsurfedges ; i++) {
		int32_t    *surfedge = &bsp29->surfedges[i];
		*surfedge = LittleLong (*surfedge);
	}

	// edges
	for (i=0 ; i<bsp29->numedges ; i++) {
		const dedge_t *edge2 = &bsp2->edges[i];
		dedge29_t  *edge29 = (dedge29_t *) &bsp29->edges[i];
		edge29->v[0] = LittleShort (edge2->v[0]);
		edge29->v[1] = LittleShort (edge2->v[1]);
	}
}

static void
swap_from_bsp29 (bsp_t *bsp2, const bsp29_t *bsp29,
				 void (*cb) (const bsp_t *, void *), void *cbdata)
{
	int             c, i, j;
	dmiptexlump_t  *mtl;
	dmodel_t       *d;

	if (bsp2->header) {
		bsp2->header->version = LittleLong (bsp2->header->version);
		for (i = 0; i < HEADER_LUMPS; i++) {
			bsp2->header->lumps[i].fileofs =
				LittleLong (bsp2->header->lumps[i].fileofs);
			bsp2->header->lumps[i].filelen =
				LittleLong (bsp2->header->lumps[i].filelen);
		}
		if (cb)
			cb (bsp2, cbdata);
	}

	// models
	for (i=0 ; i<bsp2->nummodels ; i++) {
		d = &bsp2->models[i];

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
	for (i=0 ; i<bsp2->numvertexes ; i++) {
		dvertex_t  *vertex = &bsp2->vertexes[i];
		for (j=0 ; j<3 ; j++)
			vertex->point[j] = LittleFloat (vertex->point[j]);
	}

	// planes
	for (i=0 ; i<bsp2->numplanes ; i++) {
		dplane_t   *plane = &bsp2->planes[i];
		for (j=0 ; j<3 ; j++)
			plane->normal[j] = LittleFloat (plane->normal[j]);
		plane->dist = LittleFloat (plane->dist);
		plane->type = LittleLong (plane->type);
	}

	// texinfos
	for (i=0 ; i<bsp2->numtexinfo ; i++) {
		texinfo_t  *texinfo = &bsp2->texinfo[i];
		for (j=0 ; j < 4 ; j++) {
			texinfo->vecs[0][j] = LittleFloat (texinfo->vecs[0][j]);
			texinfo->vecs[1][j] = LittleFloat (texinfo->vecs[1][j]);
		}
		texinfo->miptex = LittleLong (texinfo->miptex);
		texinfo->flags = LittleLong (texinfo->flags);
	}

	// faces
	for (i=0 ; i<bsp2->numfaces ; i++) {
		dface_t    *face2 = &bsp2->faces[i];
		const dface29_t *face29 = &bsp29->faces[i];
		face2->planenum = LittleShort (face29->planenum);
		face2->side = LittleShort (face29->side);
		face2->firstedge = LittleLong (face29->firstedge);
		face2->numedges = LittleShort (face29->numedges);
		face2->texinfo = LittleShort (face29->texinfo);
		memcpy (face2->styles, face29->styles, sizeof(face2->styles));
		face2->lightofs = LittleLong (face29->lightofs);
	}

	// nodes
	for (i=0 ; i<bsp2->numnodes ; i++) {
		dnode_t    *node2 = &bsp2->nodes[i];
		const dnode29_t  *node29 = &bsp29->nodes[i];
		node2->planenum = LittleLong (node29->planenum);
		for (j=0 ; j<3 ; j++) {
			node2->mins[j] = (int16_t) LittleShort (node29->mins[j]);
			node2->maxs[j] = (int16_t) LittleShort (node29->maxs[j]);
		}
		for (j = 0; j < 2; j++) {
			node2->children[j] = LittleShort (node29->children[j]);
			if (node2->children[j] >= bsp2->numnodes)
				node2->children[j] = (int16_t) node2->children[j];
		}
		node2->firstface = LittleShort (node29->firstface);
		node2->numfaces = LittleShort (node29->numfaces);
	}

	// leafs
	for (i=0 ; i<bsp2->numleafs ; i++) {
		dleaf_t    *leaf2 = &bsp2->leafs[i];
		const dleaf29_t *leaf29 = &bsp29->leafs[i];
		leaf2->contents = LittleLong (leaf29->contents);
		for (j=0 ; j<3 ; j++) {
			leaf2->mins[j] = (int16_t) LittleShort (leaf29->mins[j]);
			leaf2->maxs[j] = (int16_t) LittleShort (leaf29->maxs[j]);
		}

		leaf2->firstmarksurface = LittleShort (leaf29->firstmarksurface);
		leaf2->nummarksurfaces = LittleShort (leaf29->nummarksurfaces);
		leaf2->visofs = LittleLong (leaf29->visofs);
		memcpy (leaf2->ambient_level, leaf29->ambient_level,
				sizeof(leaf2->ambient_level));
	}

	// clipnodes
	for (i=0 ; i<bsp2->numclipnodes ; i++) {
		dclipnode_t *clipnode2 = &bsp2->clipnodes[i];
		const dclipnode29_t *clipnode29 = &bsp29->clipnodes[i];
		clipnode2->planenum = LittleLong (clipnode29->planenum);
		clipnode2->children[0] = LittleShort (clipnode29->children[0]);
		clipnode2->children[1] = LittleShort (clipnode29->children[1]);
	}

	// miptex
	if (bsp2->texdatasize) {
		mtl = (dmiptexlump_t *)bsp2->texdata;
		c = LittleLong(mtl->nummiptex);
		mtl->nummiptex = LittleLong (mtl->nummiptex);
		for (i=0 ; i<c ; i++)
			mtl->dataofs[i] = LittleLong(mtl->dataofs[i]);
	}

	// marksurfaces
	for (i=0 ; i<bsp2->nummarksurfaces ; i++) {
		uint32_t   *marksurface2 = &bsp2->marksurfaces[i];
		const uint16_t *marksurface29 = &bsp29->marksurfaces[i];
		*marksurface2 = LittleShort (*marksurface29);
	}

	// surfedges
	for (i=0 ; i<bsp2->numsurfedges ; i++) {
		int32_t    *surfedge = &bsp2->surfedges[i];
		*surfedge = LittleLong (*surfedge);
	}

	// edges
	for (i=0 ; i<bsp2->numedges ; i++) {
		dedge_t    *edge2 = &bsp2->edges[i];
		const dedge29_t *edge29 = &bsp29->edges[i];
		edge2->v[0] = LittleShort (edge29->v[0]);
		edge2->v[1] = LittleShort (edge29->v[1]);
	}
}

static void
swap_bsp (bsp_t *bsp, int todisk, void (*cb) (const bsp_t *, void *),
		  void *cbdata)
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
		if (cb)
			cb (bsp, cbdata);
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
		for (j=0 ; j < 4 ; j++) {
			texinfo->vecs[0][j] = LittleFloat (texinfo->vecs[0][j]);
			texinfo->vecs[1][j] = LittleFloat (texinfo->vecs[1][j]);
		}
		texinfo->miptex = LittleLong (texinfo->miptex);
		texinfo->flags = LittleLong (texinfo->flags);
	}

	// faces
	for (i=0 ; i<bsp->numfaces ; i++) {
		dface_t    *face = &bsp->faces[i];
		face->texinfo = LittleLong (face->texinfo);
		face->planenum = LittleLong (face->planenum);
		face->side = LittleLong (face->side);
		face->lightofs = LittleLong (face->lightofs);
		face->firstedge = LittleLong (face->firstedge);
		face->numedges = LittleLong (face->numedges);
	}

	// nodes
	for (i=0 ; i<bsp->numnodes ; i++) {
		dnode_t    *node = &bsp->nodes[i];
		node->planenum = LittleLong (node->planenum);
		for (j=0 ; j<3 ; j++) {
			node->mins[j] = LittleFloat (node->mins[j]);
			node->maxs[j] = LittleFloat (node->maxs[j]);
		}
		node->children[0] = LittleLong (node->children[0]);
		node->children[1] = LittleLong (node->children[1]);
		node->firstface = LittleLong (node->firstface);
		node->numfaces = LittleLong (node->numfaces);
	}

	// leafs
	for (i=0 ; i<bsp->numleafs ; i++) {
		dleaf_t    *leaf = &bsp->leafs[i];
		leaf->contents = LittleLong (leaf->contents);
		for (j=0 ; j<3 ; j++) {
			leaf->mins[j] = LittleFloat (leaf->mins[j]);
			leaf->maxs[j] = LittleFloat (leaf->maxs[j]);
		}

		leaf->firstmarksurface = LittleLong (leaf->firstmarksurface);
		leaf->nummarksurfaces = LittleLong (leaf->nummarksurfaces);
		leaf->visofs = LittleLong (leaf->visofs);
	}

	// clipnodes
	for (i=0 ; i<bsp->numclipnodes ; i++) {
		dclipnode_t *clipnode = &bsp->clipnodes[i];
		clipnode->planenum = LittleLong (clipnode->planenum);
		clipnode->children[0] = LittleLong (clipnode->children[0]);
		clipnode->children[1] = LittleLong (clipnode->children[1]);
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
		uint32_t   *marksurface = &bsp->marksurfaces[i];
		*marksurface = LittleLong (*marksurface);
	}

	// surfedges
	for (i=0 ; i<bsp->numsurfedges ; i++) {
		int32_t    *surfedge = &bsp->surfedges[i];
		*surfedge = LittleLong (*surfedge);
	}

	// edges
	for (i=0 ; i<bsp->numedges ; i++) {
		dedge_t    *edge = &bsp->edges[i];
		edge->v[0] = LittleLong (edge->v[0]);
		edge->v[1] = LittleLong (edge->v[1]);
	}
}

static void
set_bsp2_read (void *mem, size_t mem_size, bsp_t *bsp)
{
#undef SET_LUMP
#define SET_LUMP(l,n) \
do { \
	size_t      size = LittleLong (bsp->header->lumps[l].filelen); \
	size_t      offs = LittleLong (bsp->header->lumps[l].fileofs); \
	void       *data = (byte *) mem + offs; \
	if (offs >= mem_size || (offs + size) > mem_size) \
		Sys_Error ("invalid lump"); \
	if (size % sizeof (bsp->n[0])) \
		Sys_Error ("funny lump size"); \
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
	size_t      size = LittleLong (bsp->header->lumps[l].filelen); \
	size_t      offs = LittleLong (bsp->header->lumps[l].fileofs); \
	void       *data = (byte *) mem + offs; \
	if (offs >= mem_size || (offs + size) > mem_size) \
		Sys_Error ("invalid lump"); \
	bsp->n = 0; \
	if (size) \
		bsp->n = (void *) data; \
	bsp->n##size = size; \
} while (0)

	SET_LUMP (LUMP_LIGHTING, lightdata);
	SET_LUMP (LUMP_VISIBILITY, visdata);
	SET_LUMP (LUMP_ENTITIES, entdata);
	SET_LUMP (LUMP_TEXTURES, texdata);
}

static void
set_bsp29_read (void *mem, size_t mem_size, bsp29_t *bsp29)
{
#undef SET_LUMP
#define SET_LUMP(l,n) \
do { \
	size_t      size = LittleLong (bsp29->header->lumps[l].filelen); \
	size_t      offs = LittleLong (bsp29->header->lumps[l].fileofs); \
	void       *data = (byte *) mem + offs; \
	if (offs >= mem_size || (offs + size) > mem_size) \
		Sys_Error ("invalid lump"); \
	if (size % sizeof (bsp29->n[0])) \
		Sys_Error ("funny lump size"); \
	bsp29->n = 0; \
	if (size) \
		bsp29->n = (void *) data; \
	bsp29->num##n = size / sizeof (bsp29->n[0]); \
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
	size_t      size = LittleLong (bsp29->header->lumps[l].filelen); \
	size_t      offs = LittleLong (bsp29->header->lumps[l].fileofs); \
	void       *data = (byte *) mem + offs; \
	if (offs >= mem_size || (offs + size) > mem_size) \
		Sys_Error ("invalid lump"); \
	bsp29->n = 0; \
	if (size) \
		bsp29->n = (void *) data; \
	bsp29->n##size = size; \
} while (0)

	SET_LUMP (LUMP_LIGHTING, lightdata);
	SET_LUMP (LUMP_VISIBILITY, visdata);
	SET_LUMP (LUMP_ENTITIES, entdata);
	SET_LUMP (LUMP_TEXTURES, texdata);
}

bsp_t *
LoadBSPMem (void *mem, size_t mem_size, void (*cb) (const bsp_t *, void *),
			void *cbdata)
{
	bsp_t      *bsp;
	int         version;
	qboolean    bsp2 = false;

	bsp = calloc (sizeof (bsp_t), 1);

	bsp->header = mem;

	version = LittleLong (bsp->header->version);
	if (!memcmp (&bsp->header->version, BSP2VERSION, 4)) {
		bsp2 = true;
	} else if (version != BSPVERSION)
		Sys_Error ("version %i, neither %i nor %s", version, BSPVERSION,
				   BSP2VERSION);

	if (bsp2) {
		set_bsp2_read (mem, mem_size, bsp);
		swap_bsp (bsp, 0, cb, cbdata);
	} else {
		bsp29_t     bsp29;

		if (sizeof (bsp29) != sizeof (*bsp))
			Sys_Error ("taniwha's being lazy about bsp29 support");
		memcpy (&bsp29, bsp, sizeof (bsp29));
		set_bsp29_read (mem, mem_size, &bsp29);
		memcpy (bsp, &bsp29, sizeof (bsp29));

#undef SET_LUMP
#define SET_LUMP(n) \
do { \
	if (bsp->num##n) {\
		bsp->n = (void *) malloc (bsp->num##n * sizeof (bsp->n[0])); \
		bsp->own_##n = 1; \
	} \
} while (0)

		SET_LUMP (nodes);
		SET_LUMP (clipnodes);
		SET_LUMP (edges);
		SET_LUMP (faces);
		SET_LUMP (leafs);
		SET_LUMP (marksurfaces);
		swap_from_bsp29 (bsp, &bsp29, cb, cbdata);
	}
	return bsp;
}

VISIBLE bsp_t *
LoadBSPFile (QFile *file, size_t size)
{
	void       *buf;
	bsp_t      *bsp;

	buf = malloc (size);
	Qread (file, buf, size);
	bsp = LoadBSPMem (buf, size, 0, 0);
	bsp->own_header = 1;
	return bsp;
}

#define ROUND(x) (((x) + 3) & ~3)

static bsp_t *
set_bsp2_write (const bsp_t *bsp, size_t *_size)
{
	size_t      size;
	byte       *data;
	bsp_t      *tbsp;

	size = sizeof (*tbsp);
	size += ROUND (sizeof (dheader_t));
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
	size += ROUND (bsp->nummarksurfaces * sizeof (uint32_t));
	size += ROUND (bsp->numsurfedges * sizeof (uint32_t));

	tbsp = calloc (size, 1);
	tbsp->header = (dheader_t *) &tbsp[1];

#undef SET_LUMP
#define SET_LUMP(l,n) \
do { \
	tbsp->num##n = bsp->num##n; \
	if (tbsp->num##n) {\
		tbsp->n = (void *) data; \
		tbsp->header->lumps[l].fileofs = data - (byte *) tbsp->header; \
		tbsp->header->lumps[l].filelen = tbsp->num##n * sizeof (tbsp->n[0]); \
		memcpy (data, bsp->n, tbsp->header->lumps[l].filelen); \
		data += ROUND (tbsp->header->lumps[l].filelen); \
	} else {\
		tbsp->n = 0; \
		tbsp->header->lumps[l].fileofs = 0; \
		tbsp->header->lumps[l].filelen = 0; \
	} \
} while (0)

	tbsp->header->version = BSPVERSION;

	data = (byte *) &tbsp->header[1];
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
	tbsp->n##size = bsp->n##size; \
	if (tbsp->n##size) { \
		tbsp->n = (void *) data; \
		tbsp->header->lumps[l].fileofs = data - (byte *) tbsp->header; \
		tbsp->header->lumps[l].filelen = tbsp->n##size; \
		memcpy (data, bsp->n, tbsp->n##size); \
		data += ROUND (tbsp->header->lumps[l].filelen); \
	} else {\
		tbsp->n = 0; \
		tbsp->header->lumps[l].fileofs = 0; \
		tbsp->header->lumps[l].filelen = 0; \
	} \
} while (0)

	SET_LUMP (LUMP_LIGHTING, lightdata);
	SET_LUMP (LUMP_VISIBILITY, visdata);
	SET_LUMP (LUMP_ENTITIES, entdata);
	SET_LUMP (LUMP_TEXTURES, texdata);

	*_size = size - sizeof (*tbsp);
	return tbsp;
}

static bsp29_t *
set_bsp29_write (const bsp_t *bsp, size_t *_size)
{
	size_t      size;
	byte       *data;
	bsp29_t    *tbsp;

	size = sizeof (*tbsp);
	size += ROUND (sizeof (dheader_t));
	size += ROUND (bsp->nummodels * sizeof (dmodel_t));
	size += ROUND (bsp->visdatasize);
	size += ROUND (bsp->lightdatasize);
	size += ROUND (bsp->texdatasize);
	size += ROUND (bsp->entdatasize);
	size += ROUND (bsp->numleafs * sizeof (dleaf29_t));
	size += ROUND (bsp->numplanes * sizeof (dplane_t));
	size += ROUND (bsp->numvertexes * sizeof (dvertex_t));
	size += ROUND (bsp->numnodes * sizeof (dnode29_t));
	size += ROUND (bsp->numtexinfo * sizeof (texinfo_t));
	size += ROUND (bsp->numfaces * sizeof (dface29_t));
	size += ROUND (bsp->numclipnodes * sizeof (dclipnode29_t));
	size += ROUND (bsp->numedges * sizeof (dedge29_t));
	size += ROUND (bsp->nummarksurfaces * sizeof (uint16_t));
	size += ROUND (bsp->numsurfedges * sizeof (uint32_t));

	tbsp = calloc (size, 1);
	tbsp->header = (dheader_t *) &tbsp[1];

#undef SET_LUMP
#define SET_LUMP(l,n) \
do { \
	tbsp->num##n = bsp->num##n; \
	if (tbsp->num##n) {\
		tbsp->n = (void *) data; \
		tbsp->header->lumps[l].fileofs = data - (byte *) tbsp->header; \
		tbsp->header->lumps[l].filelen = tbsp->num##n * sizeof (tbsp->n[0]); \
		memcpy (data, bsp->n, tbsp->header->lumps[l].filelen); \
		data += ROUND (tbsp->header->lumps[l].filelen); \
	} else {\
		tbsp->n = 0; \
		tbsp->header->lumps[l].fileofs = 0; \
		tbsp->header->lumps[l].filelen = 0; \
	} \
} while (0)

	tbsp->header->version = BSPVERSION;

	data = (byte *) &tbsp->header[1];
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
	tbsp->n##size = bsp->n##size; \
	if (tbsp->n##size) { \
		tbsp->n = (void *) data; \
		tbsp->header->lumps[l].fileofs = data - (byte *) tbsp->header; \
		tbsp->header->lumps[l].filelen = tbsp->n##size; \
		memcpy (data, bsp->n, tbsp->n##size); \
		data += ROUND (tbsp->header->lumps[l].filelen); \
	} else {\
		tbsp->n = 0; \
		tbsp->header->lumps[l].fileofs = 0; \
		tbsp->header->lumps[l].filelen = 0; \
	} \
} while (0)

	SET_LUMP (LUMP_LIGHTING, lightdata);
	SET_LUMP (LUMP_VISIBILITY, visdata);
	SET_LUMP (LUMP_ENTITIES, entdata);
	SET_LUMP (LUMP_TEXTURES, texdata);

	*_size = size - sizeof (*tbsp);
	return tbsp;
}

/*
	WriteBSPFile
*/
VISIBLE void
WriteBSPFile (const bsp_t *bsp, QFile *file)
{
	qboolean    bsp2 = (   bsp->models[0].mins[0] < -32768.0f
						|| bsp->models[0].mins[1] < -32768.0f
						|| bsp->models[0].mins[2] < -32768.0f
						|| bsp->models[0].mins[0] >= 32768.0f
						|| bsp->models[0].mins[1] >= 32768.0f
						|| bsp->models[0].mins[2] >= 32768.0f
						|| bsp->nummarksurfaces >= 32768
						|| bsp->numvertexes >= 32768
						|| bsp->numnodes >= 32768
						|| bsp->numleafs >= 32768
						|| bsp->numplanes >= 32768
						|| bsp->numtexinfo >= 32768
						|| bsp->numclipnodes >= 32768);

	if (bsp2) {
		bsp_t      *tbsp;
		size_t      size;

		tbsp = set_bsp2_write (bsp, &size);
		swap_bsp (tbsp, 1, 0, 0);
		Qwrite (file, tbsp->header, size);
		free (tbsp);
	} else {
		bsp29_t    *tbsp;
		size_t      size;

		tbsp = set_bsp29_write (bsp, &size);
		swap_to_bsp29 (tbsp, bsp);
		Qwrite (file, tbsp->header, size);
		free (tbsp);
	}
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
BSP_AddPlane (bsp_t *bsp, const dplane_t *plane)
{
	REALLOC (planes);
	bsp->planes[bsp->numplanes++] = *plane;
}

VISIBLE void
BSP_AddLeaf (bsp_t *bsp, const dleaf_t *leaf)
{
	REALLOC (leafs);
	bsp->leafs[bsp->numleafs++] = *leaf;
}

VISIBLE void
BSP_AddVertex (bsp_t *bsp, const dvertex_t *vertex)
{
	REALLOC (vertexes);
	bsp->vertexes[bsp->numvertexes++] = *vertex;
}

VISIBLE void
BSP_AddNode (bsp_t *bsp, const dnode_t *node)
{
	REALLOC (nodes);
	bsp->nodes[bsp->numnodes++] = *node;
}

VISIBLE void
BSP_AddTexinfo (bsp_t *bsp, const texinfo_t *texinfo)
{
	REALLOC (texinfo);
	bsp->texinfo[bsp->numtexinfo++] = *texinfo;
}

VISIBLE void
BSP_AddFace (bsp_t *bsp, const dface_t *face)
{
	REALLOC (faces);
	bsp->faces[bsp->numfaces++] = *face;
}

VISIBLE void
BSP_AddClipnode (bsp_t *bsp, const dclipnode_t *clipnode)
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
BSP_AddEdge (bsp_t *bsp, const dedge_t *edge)
{
	REALLOC (edges);
	bsp->edges[bsp->numedges++] = *edge;
}

VISIBLE void
BSP_AddModel (bsp_t *bsp, const dmodel_t *model)
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
BSP_AddLighting (bsp_t *bsp, const byte *lightdata, size_t lightdatasize)
{
	OWN (lightdata);
	bsp->lightdatasize = lightdatasize;
	bsp->lightdata = malloc (lightdatasize);
	memcpy (bsp->lightdata, lightdata, lightdatasize);
}

VISIBLE void
BSP_AddVisibility (bsp_t *bsp, const byte *visdata, size_t visdatasize)
{
	OWN (visdata);
	bsp->visdatasize = visdatasize;
	bsp->visdata = malloc (visdatasize);
	memcpy (bsp->visdata, visdata, visdatasize);
}

VISIBLE void
BSP_AddEntities (bsp_t *bsp, const char *entdata, size_t entdatasize)
{
	OWN (entdata);
	bsp->entdatasize = entdatasize;
	bsp->entdata = malloc (entdatasize);
	memcpy (bsp->entdata, entdata, entdatasize);
}

VISIBLE void
BSP_AddTextures (bsp_t *bsp, const byte *texdata, size_t texdatasize)
{
	OWN (texdata);
	bsp->texdatasize = texdatasize;
	bsp->texdata = malloc (texdatasize);
	memcpy (bsp->texdata, texdata, texdatasize);
}
