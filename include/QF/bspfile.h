/*
	bspfile.h

	BSP (Binary Space Partitioning) file definitions

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
#ifndef __QF_bspfile_h
#define __QF_bspfile_h

#include "QF/qtypes.h"
#include "QF/quakeio.h"

// upper design bounds

#define MAX_MAP_HULLS			4		// format limit (array)

#define MAX_MAP_PLANES			32767	// format limit (s16) FIXME u16 ok?
#define MAX_MAP_NODES			65520	// because negative shorts are contents
#define MAX_MAP_CLIPNODES		65520	// but contents "max" is -15, so
#define MAX_MAP_LEAFS			65520	// -32768 to -17 are available
#define MAX_MAP_VERTS			65535	// format limit (u16)
#define MAX_MAP_FACES			65535	// format limit (u16)
#define MAX_MAP_MARKSURFACES	65535	// format limit (u16)

//=============================================================================

#define BSPVERSION		29
#define BSP2VERSION		"BSP2"		// use memcmp with 4 bytes
#define Q2BSPVERSION	38
#define TOOLVERSION		2

typedef struct lump_s {
	int32_t fileofs;
	int32_t filelen;
} lump_t;

#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_TEXTURES		2
#define LUMP_VERTEXES		3
#define LUMP_VISIBILITY		4
#define LUMP_NODES			5
#define LUMP_TEXINFO		6
#define LUMP_FACES			7
#define LUMP_LIGHTING		8
#define LUMP_CLIPNODES		9
#define LUMP_LEAFS			10
#define LUMP_MARKSURFACES	11
#define LUMP_EDGES			12
#define LUMP_SURFEDGES		13
#define LUMP_MODELS			14
#define HEADER_LUMPS		15

typedef struct dmodel_s {
	float       mins[3], maxs[3];
	float       origin[3];
	int32_t     headnode[MAX_MAP_HULLS];
	int32_t     visleafs;				// not including the solid leaf 0
	int32_t     firstface, numfaces;
} dmodel_t;

typedef struct dheader_s {
	int32_t     version;
	lump_t      lumps[HEADER_LUMPS];
} dheader_t;

typedef struct dmiptexlump_s {
	int32_t     nummiptex;
	int32_t     dataofs[4];				// [nummiptex]
} dmiptexlump_t;

#define MIPTEXNAME  16
#define MIPLEVELS   4
typedef struct miptex_s {
	char        name[MIPTEXNAME];
	uint32_t    width, height;
	uint32_t    offsets[MIPLEVELS];		// four mip maps stored
} miptex_t;


typedef struct dvertex_s {
	float   point[3];
} dvertex_t;


// 0-2 are axial planes
#define PLANE_X		0
#define PLANE_Y		1
#define PLANE_Z		2
// 3-5 are non-axial planes snapped to the nearest
#define PLANE_ANYX	3
#define PLANE_ANYY	4
#define PLANE_ANYZ	5

typedef struct dplane_s {
	float   normal[3];
	float   dist;
	int32_t type;						// PLANE_X - PLANE_ANYZ
} dplane_t;

#define CONTENTS_EMPTY			-1
#define CONTENTS_SOLID			-2
#define CONTENTS_WATER			-3
#define CONTENTS_SLIME			-4
#define CONTENTS_LAVA			-5
#define CONTENTS_SKY			-6
#define CONTENTS_ORIGIN			-7		// removed at csg time
#define CONTENTS_CLIP			-8		// changed to contents_solid

#define CONTENTS_CURRENT_0		-9
#define CONTENTS_CURRENT_90		-10
#define CONTENTS_CURRENT_180	-11
#define CONTENTS_CURRENT_270	-12
#define CONTENTS_CURRENT_UP		-13
#define CONTENTS_CURRENT_DOWN	-14

//BSP2 version (bsp 29 version is in bspfile.c)
typedef struct dnode_s {
	int32_t     planenum;
	int32_t     children[2];	// negative numbers are -(leafs+1), not nodes
	float       mins[3];		// for sphere culling
	float       maxs[3];
	uint32_t    firstface;
	uint32_t    numfaces;		// counting both sides
} dnode_t;

//BSP2 version (bsp 29 version is in bspfile.c)
typedef struct dclipnode_s {
	int32_t     planenum;
	int32_t     children[2];	// negative numbers are contents
} dclipnode_t;


typedef struct texinfo_s {
	float       vecs[2][4];		// [s/t][xyz offset]
	int32_t     miptex;
	int32_t     flags;
} texinfo_t;
#define TEX_SPECIAL 1			// sky or slime, no lightmap or 256 subdivision
#define TEX_MISSING 2			// this texinfo does not have a texture

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
//BSP2 version (bsp 29 version is in bspfile.c)
typedef struct dedge_s {
	uint32_t    v[2];			// vertex numbers
} dedge_t;

#define MAXLIGHTMAPS 4
//BSP2 version (bsp 29 version is in bspfile.c)
typedef struct dface_s {
	int32_t     planenum;
	int32_t     side;

	int32_t     firstedge;		// we must support > 64k edges
	int32_t     numedges;
	int32_t     texinfo;

// lighting info
	byte        styles[MAXLIGHTMAPS];
	int32_t     lightofs;		// start of [numstyles*surfsize] samples
} dface_t;


#define AMBIENT_WATER   0
#define AMBIENT_SKY     1
#define AMBIENT_SLIME   2
#define AMBIENT_LAVA    3

#define NUM_AMBIENTS    4			// automatic ambient sounds

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info
//BSP2 version (bsp 29 version is in bspfile.c)
typedef struct dleaf_s {
	int32_t     contents;
	int32_t     visofs;				// -1 = no visibility info

	float       mins[3];			// for frustum culling
	float       maxs[3];

	uint32_t    firstmarksurface;
	uint32_t    nummarksurfaces;

	byte        ambient_level[NUM_AMBIENTS];
} dleaf_t;

//============================================================================

typedef struct bsp_s {
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
	dleaf_t    *leafs;

	int         own_planes;
	int         numplanes;
	dplane_t   *planes;

	int         own_vertexes;
	int         numvertexes;
	dvertex_t  *vertexes;

	int         own_nodes;
	int         numnodes;
	dnode_t    *nodes;

	int         own_texinfo;
	int         numtexinfo;
	texinfo_t  *texinfo;

	int         own_faces;
	int         numfaces;
	dface_t    *faces;

	int         own_clipnodes;
	int         numclipnodes;
	dclipnode_t *clipnodes;

	int         own_edges;
	int         numedges;
	dedge_t    *edges;

	int         own_marksurfaces;
	int         nummarksurfaces;
	uint32_t   *marksurfaces;

	int         own_surfedges;
	int         numsurfedges;
	int32_t    *surfedges;
} bsp_t;

/**	Create a bsp structure from a memory buffer.
	The returned structure will be setup to point into the supplied buffer.
	All structures within the bsp will be byte-swapped. For this reason, if
	a callback is provided, the callback be called after byteswapping the
	header, but before byteswapping any data in the lumps.

	\param mem		The buffer holding the bsp data.
	\param size		The size of the buffer. This is used for sanity checking.
	\param cb		Pointer to the callback function.
	\param cbdata	Pointer to extra data for the callback.
	\return			Initialized bsp structure.

	\note The caller maintains ownership of the memory buffer. BSP_Free will
	free only the bsp structure itself. However, if the caller wishes to
	relinquish ownership of the buffer, set bsp_t::own_header to true.
*/
bsp_t *LoadBSPMem (void *mem, size_t size, void (*cb) (const bsp_t *, void *),
				   void *cbdata);
bsp_t *LoadBSPFile (QFile *file, size_t size);
void WriteBSPFile (const bsp_t *bsp, QFile *file);
bsp_t *BSP_New (void);
void BSP_Free (bsp_t *bsp);
void BSP_AddPlane (bsp_t *bsp, const dplane_t *plane);
void BSP_AddLeaf (bsp_t *bsp, const dleaf_t *leaf);
void BSP_AddVertex (bsp_t *bsp, const dvertex_t *vertex);
void BSP_AddNode (bsp_t *bsp, const dnode_t *node);
void BSP_AddTexinfo (bsp_t *bsp, const texinfo_t *texinfo);
void BSP_AddFace (bsp_t *bsp, const dface_t *face);
void BSP_AddClipnode (bsp_t *bsp, const dclipnode_t *clipnode);
void BSP_AddMarkSurface (bsp_t *bsp, int marksurface);
void BSP_AddSurfEdge (bsp_t *bsp, int surfedge);
void BSP_AddEdge (bsp_t *bsp, const dedge_t *edge);
void BSP_AddModel (bsp_t *bsp, const dmodel_t *model);
void BSP_AddLighting (bsp_t *bsp, const byte *lightdata, size_t lightdatasize);
void BSP_AddVisibility (bsp_t *bsp, const byte *visdata, size_t visdatasize);
void BSP_AddEntities (bsp_t *bsp, const char *entdata, size_t entdatasize);
void BSP_AddTextures (bsp_t *bsp, const byte *texdata, size_t texdatasize);

#endif//__QF_bspfile_h
