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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>

#include "QF/bspfile.h"
#include "QF/qendian.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

static void
swap_bsp (bsp_t *bsp, int todisk)
{
	int				 c, i, j;
	dmiptexlump_t	*mtl;
	dmodel_t		*d;

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
		for (j=0 ; j<3 ; j++)
			bsp->vertexes[i].point[j] = LittleFloat (bsp->vertexes[i].point[j]);
	}
		
	// planes
	for (i=0 ; i<bsp->numplanes ; i++) {
		for (j=0 ; j<3 ; j++)
			bsp->planes[i].normal[j] = LittleFloat (bsp->planes[i].normal[j]);
		bsp->planes[i].dist = LittleFloat (bsp->planes[i].dist);
		bsp->planes[i].type = LittleLong (bsp->planes[i].type);
	}
	
	// texinfos
	for (i=0 ; i<bsp->numtexinfo ; i++) {
		for (j=0 ; j<8 ; j++)
			bsp->texinfo[i].vecs[0][j] = LittleFloat (bsp->texinfo[i].vecs[0][j]);
		bsp->texinfo[i].miptex = LittleLong (bsp->texinfo[i].miptex);
		bsp->texinfo[i].flags = LittleLong (bsp->texinfo[i].flags);
	}
	
	// faces
	for (i=0 ; i<bsp->numfaces ; i++) {
		bsp->faces[i].texinfo = LittleShort (bsp->faces[i].texinfo);
		bsp->faces[i].planenum = LittleShort (bsp->faces[i].planenum);
		bsp->faces[i].side = LittleShort (bsp->faces[i].side);
		bsp->faces[i].lightofs = LittleLong (bsp->faces[i].lightofs);
		bsp->faces[i].firstedge = LittleLong (bsp->faces[i].firstedge);
		bsp->faces[i].numedges = LittleShort (bsp->faces[i].numedges);
	}

	// nodes
	for (i=0 ; i<bsp->numnodes ; i++) {
		bsp->nodes[i].planenum = LittleLong (bsp->nodes[i].planenum);
		for (j=0 ; j<3 ; j++) {
			bsp->nodes[i].mins[j] = LittleShort (bsp->nodes[i].mins[j]);
			bsp->nodes[i].maxs[j] = LittleShort (bsp->nodes[i].maxs[j]);
		}
		bsp->nodes[i].children[0] = LittleShort (bsp->nodes[i].children[0]);
		bsp->nodes[i].children[1] = LittleShort (bsp->nodes[i].children[1]);
		bsp->nodes[i].firstface = LittleShort (bsp->nodes[i].firstface);
		bsp->nodes[i].numfaces = LittleShort (bsp->nodes[i].numfaces);
	}

	// leafs
	for (i=0 ; i<bsp->numleafs ; i++) {
		bsp->leafs[i].contents = LittleLong (bsp->leafs[i].contents);
		for (j=0 ; j<3 ; j++) {
			bsp->leafs[i].mins[j] = LittleShort (bsp->leafs[i].mins[j]);
			bsp->leafs[i].maxs[j] = LittleShort (bsp->leafs[i].maxs[j]);
		}

		bsp->leafs[i].firstmarksurface = LittleShort (bsp->leafs[i].firstmarksurface);
		bsp->leafs[i].nummarksurfaces = LittleShort (bsp->leafs[i].nummarksurfaces);
		bsp->leafs[i].visofs = LittleLong (bsp->leafs[i].visofs);
	}

	// clipnodes
	for (i=0 ; i<bsp->numclipnodes ; i++) {
		bsp->clipnodes[i].planenum = LittleLong (bsp->clipnodes[i].planenum);
		bsp->clipnodes[i].children[0] = LittleShort (bsp->clipnodes[i].children[0]);
		bsp->clipnodes[i].children[1] = LittleShort (bsp->clipnodes[i].children[1]);
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
	for (i=0 ; i<bsp->nummarksurfaces ; i++)
		bsp->marksurfaces[i] = LittleShort (bsp->marksurfaces[i]);

	// surfedges
	for (i=0 ; i<bsp->numsurfedges ; i++)
		bsp->surfedges[i] = LittleLong (bsp->surfedges[i]);

	// edges
	for (i=0 ; i<bsp->numedges ; i++) {
		bsp->edges[i].v[0] = LittleShort (bsp->edges[i].v[0]);
		bsp->edges[i].v[1] = LittleShort (bsp->edges[i].v[1]);
	}
}

bsp_t *
LoadBSPFile (QFile *file, int size)
{
	dheader_t  *header;
	bsp_t      *bsp;

	header = malloc (size);
	Qread (file, header, size);

	if (LittleLong (header->version) != BSPVERSION)
		Sys_Error ("version %i, not %i", LittleLong (header->version),
				   BSPVERSION);

#undef SET_LUMP
#define SET_LUMP(l,n) \
do { \
	bsp->num##n = LittleLong (header->lumps[l].filelen); \
	bsp->n = malloc (bsp->num##n); \
	memcpy (bsp->n, (byte *) header + LittleLong (header->lumps[l].fileofs), \
			bsp->num##n); \
	bsp->num##n /= sizeof (bsp->n[0]); \
} while (0)

	bsp = malloc (sizeof (bsp_t));
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
	bsp->n##size = LittleLong (header->lumps[l].filelen); \
	bsp->n = malloc (bsp->n##size); \
	memcpy (bsp->n, (byte *) header + LittleLong (header->lumps[l].fileofs), \
			bsp->n##size); \
	bsp->n##size /= sizeof (bsp->n[0]); \
} while (0)

	SET_LUMP (LUMP_LIGHTING, lightdata);
	SET_LUMP (LUMP_VISIBILITY, visdata);
	SET_LUMP (LUMP_ENTITIES, entdata);
	SET_LUMP (LUMP_TEXTURES, texdata);

	free (header);

	swap_bsp (bsp, 0);
	return bsp;
}

/*
	WriteBSPFile

	Swaps the bsp file in place, so it should not be referenced again
*/
void
WriteBSPFile (bsp_t *bsp, QFile *file)
{		
	int         size;
	dheader_t  *header;
	byte       *data;

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
	size += ROUND (bsp->nummarksurfaces * sizeof (unsigned short));
	size += ROUND (bsp->numsurfedges * sizeof (int));

	header = malloc (size);
	memset (header, 0, size);

	swap_bsp (bsp, 1);

#undef SET_LUMP
#define SET_LUMP(l,n) \
do { \
	bsp->num##n *= sizeof (bsp->n[0]); \
	header->lumps[l].fileofs = LittleLong (data - (byte *) header); \
	header->lumps[l].filelen = LittleLong (bsp->num##n); \
	memcpy (data, bsp->n, bsp->num##n); \
	data += ROUND (bsp->num##n); \
} while (0)

	header->version = LittleLong (BSPVERSION);

	data = (byte *) &header[1];
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
	bsp->n##size *= sizeof (bsp->n[0]); \
	header->lumps[l].fileofs = LittleLong (data - (byte *) header); \
	header->lumps[l].filelen = LittleLong (bsp->n##size); \
	memcpy (data, bsp->n, bsp->n##size); \
	data += ROUND (bsp->n##size); \
} while (0)

	SET_LUMP (LUMP_LIGHTING, lightdata);
	SET_LUMP (LUMP_VISIBILITY, visdata);
	SET_LUMP (LUMP_ENTITIES, entdata);
	SET_LUMP (LUMP_TEXTURES, texdata);

	Qwrite (file, header, size);
	free (header);
}

bsp_t *
BSP_New (void)
{
	return calloc (1, sizeof (bsp_t));
}

void BSP_AddPlane (bsp_t *bsp, dplane_t *plane)
{
	bsp->planes = realloc (bsp->planes,
						   (bsp->numplanes + 1) * sizeof (dplane_t));
	bsp->planes[bsp->numplanes++] = *plane;
}

void BSP_AddLeaf (bsp_t *bsp, dleaf_t *leaf)
{
	bsp->leafs = realloc (bsp->leafs,
						  (bsp->numleafs + 1) * sizeof (dleaf_t));
	bsp->leafs[bsp->numleafs++] = *leaf;
}

void BSP_AddVertex (bsp_t *bsp, dvertex_t *vertex)
{
	bsp->vertexes = realloc (bsp->vertexes,
							 (bsp->numvertexes + 1) * sizeof (dvertex_t));
	bsp->vertexes[bsp->numvertexes++] = *vertex;
}

void BSP_AddNode (bsp_t *bsp, dnode_t *node)
{
	bsp->nodes = realloc (bsp->nodes,
						  (bsp->numnodes + 1) * sizeof (dnode_t));
	bsp->nodes[bsp->numnodes++] = *node;
}

void
BSP_AddTexinfo (bsp_t *bsp, texinfo_t *texinfo)
{
	bsp->texinfo = realloc (bsp->texinfo,
							(bsp->numtexinfo + 1) * sizeof (texinfo_t));
	bsp->texinfo[bsp->numtexinfo++] = *texinfo;
}

void BSP_AddFace (bsp_t *bsp, dface_t *face)
{
	bsp->faces = realloc (bsp->faces,
						  (bsp->numfaces + 1) * sizeof (dface_t));
	bsp->faces[bsp->numfaces++] = *face;
}

void BSP_AddClipnode (bsp_t *bsp, dclipnode_t *clipnode)
{
	bsp->clipnodes = realloc (bsp->clipnodes,
							  (bsp->numclipnodes + 1) * sizeof (dclipnode_t));
	bsp->clipnodes[bsp->numclipnodes++] = *clipnode;
}

void BSP_AddMarkSurface (bsp_t *bsp, unsigned short marksurface)
{
	bsp->marksurfaces = realloc (bsp->marksurfaces,
								 (bsp->nummarksurfaces + 1)
								 * sizeof (unsigned short));
	bsp->marksurfaces[bsp->nummarksurfaces++] = marksurface;
}

void BSP_AddSurfEdge (bsp_t *bsp, int surfedge)
{
	bsp->surfedges = realloc (bsp->surfedges,
							  (bsp->numsurfedges + 1) * sizeof (int));
	bsp->surfedges[bsp->numsurfedges++] = surfedge;
}

void BSP_AddEdge (bsp_t *bsp, dedge_t *edge)
{
	bsp->edges = realloc (bsp->edges,
						  (bsp->numedges + 1) * sizeof (dedge_t));
	bsp->edges[bsp->numedges++] = *edge;
}

void BSP_AddModel (bsp_t *bsp, dmodel_t *model)
{
	bsp->models = realloc (bsp->models,
						   (bsp->nummodels + 1) * sizeof (dmodel_t));
	bsp->models[bsp->nummodels++] = *model;
}

void BSP_AddLighting (bsp_t *bsp, byte *lightdata, int lightdatasize)
{
	bsp->lightdatasize = lightdatasize;
	bsp->lightdata = malloc (lightdatasize);
	memcpy (bsp->lightdata, lightdata, lightdatasize);
}

void BSP_AddVisibility (bsp_t *bsp, byte *visdata, int visdatasize)
{
	bsp->visdatasize = visdatasize;
	bsp->visdata = malloc (visdatasize);
	memcpy (bsp->visdata, visdata, visdatasize);
}

void BSP_AddEntities (bsp_t *bsp, char *entdata, int entdatasize)
{
	bsp->entdatasize = entdatasize;
	bsp->entdata = malloc (entdatasize);
	memcpy (bsp->entdata, entdata, entdatasize);
}

void BSP_AddTextures (bsp_t *bsp, byte *texdata, int texdatasize)
{
	bsp->texdatasize = texdatasize;
	bsp->texdata = malloc (texdatasize);
	memcpy (bsp->texdata, texdata, texdatasize);
}
