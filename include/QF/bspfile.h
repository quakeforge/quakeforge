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

	$Id$
*/
#ifndef __bspfile_h_
#define __bspfile_h_

#include "QF/qtypes.h"
#include "QF/quakeio.h"

// upper design bounds

#define	MAX_MAP_HULLS		4

#define	MAX_MAP_MODELS		256
#define	MAX_MAP_BRUSHES		4096
#define	MAX_MAP_ENTITIES	1024
#define	MAX_MAP_ENTSTRING	65536

#define	MAX_MAP_PLANES		32767
//#define	MAX_MAP_PLANES		8192
#define	MAX_MAP_NODES		32767		// because negative shorts are contents
#define	MAX_MAP_CLIPNODES	32767		//
#define	MAX_MAP_LEAFS		32767		//
//#define	MAX_MAP_LEAFS		8192
#define	MAX_MAP_VERTS		65535
#define	MAX_MAP_FACES		65535
#define	MAX_MAP_MARKSURFACES 65535
#define	MAX_MAP_TEXINFO		4096
#define	MAX_MAP_EDGES		256000
#define	MAX_MAP_SURFEDGES	512000
#define	MAX_MAP_TEXTURES	512
#define	MAX_MAP_MIPTEX		0x200000
#define	MAX_MAP_LIGHTING	0x100000
#define	MAX_MAP_VISIBILITY	0x100000

#define	MAX_MAP_PORTALS		65536

// key / value pair sizes

#define	MAX_KEY		32
#define	MAX_VALUE	1024


//=============================================================================

// little-endian PACK
#define IDPAKHEADER		(('K'<<24)+('C'<<16)+('A'<<8)+'P')

#define BSPVERSION		29
#define Q2BSPVERSION	38
#define	TOOLVERSION		2

typedef struct lump_s {
	int		fileofs;
	int 	filelen;
} lump_t;

#define	LUMP_ENTITIES	0
#define	LUMP_PLANES		1
#define	LUMP_TEXTURES	2
#define	LUMP_VERTEXES	3
#define	LUMP_VISIBILITY	4
#define	LUMP_NODES		5
#define	LUMP_TEXINFO	6
#define	LUMP_FACES		7
#define	LUMP_LIGHTING	8
#define	LUMP_CLIPNODES	9
#define	LUMP_LEAFS		10
#define	LUMP_MARKSURFACES 11
#define	LUMP_EDGES		12
#define	LUMP_SURFEDGES	13
#define	LUMP_MODELS		14
#define	HEADER_LUMPS	15

typedef struct dmodel_s {
	float		mins[3], maxs[3];
	float		origin[3];
	int			headnode[MAX_MAP_HULLS];
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} dmodel_t;

typedef struct dheader_s {
	int			version;	
	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct dmiptexlump_s {
	int			nummiptex;
	int			dataofs[4];		// [nummiptex]
} dmiptexlump_t;

#define	MIPLEVELS	4
typedef struct miptex_s {
	char		name[16];
	unsigned int	width, height;
	unsigned int	offsets[MIPLEVELS];		// four mip maps stored
} miptex_t;


typedef struct dvertex_s {
	float	point[3];
} dvertex_t;


// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5

typedef struct dplane_s {
	float	normal[3];
	float	dist;
	int		type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;

#define	CONTENTS_EMPTY		-1
#define	CONTENTS_SOLID		-2
#define	CONTENTS_WATER		-3
#define	CONTENTS_SLIME		-4
#define	CONTENTS_LAVA		-5
#define	CONTENTS_SKY		-6
#define	CONTENTS_ORIGIN		-7              // removed at csg time
#define	CONTENTS_CLIP		-8              // changed to contents_solid

#define	CONTENTS_CURRENT_0		-9
#define	CONTENTS_CURRENT_90		-10
#define	CONTENTS_CURRENT_180	-11
#define	CONTENTS_CURRENT_270	-12
#define	CONTENTS_CURRENT_UP		-13
#define	CONTENTS_CURRENT_DOWN	-14


// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct dnode_s {
	int			planenum;
	short		children[2];	// negative numbers are -(leafs+1), not nodes
	short		mins[3];		// for sphere culling
	short		maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;	// counting both sides
} dnode_t;

typedef struct dclipnode_s {
	int			planenum;
	short		children[2];	// negative numbers are contents
} dclipnode_t;


typedef struct texinfo_s {
	float		vecs[2][4];		// [s/t][xyz offset]
	int			miptex;
	int			flags;
} texinfo_t;
#define	TEX_SPECIAL		1		// sky or slime, no lightmap or 256 subdivision

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct dedge_s {
	unsigned short	v[2];		// vertex numbers
} dedge_t;

#define	MAXLIGHTMAPS	4
typedef struct dface_s {
	short		planenum;
	short		side;

	int			firstedge;		// we must support > 64k edges
	short		numedges;	
	short		texinfo;

// lighting info
	byte		styles[MAXLIGHTMAPS];
	int			lightofs;		// start of [numstyles*surfsize] samples
} dface_t;


#define	AMBIENT_WATER	0
#define	AMBIENT_SKY		1
#define	AMBIENT_SLIME	2
#define	AMBIENT_LAVA	3

#define	NUM_AMBIENTS			4		// automatic ambient sounds

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info
typedef struct dleaf_s {
	int			contents;
	int			visofs;				// -1 = no visibility info

	short		mins[3];			// for frustum culling
	short		maxs[3];

	unsigned short		firstmarksurface;
	unsigned short		nummarksurfaces;

	byte		ambient_level[NUM_AMBIENTS];
} dleaf_t;

//============================================================================

typedef struct bsp_s {
	int			nummodels;
	dmodel_t	*models;

	int			visdatasize;
	byte		*visdata;

	int			lightdatasize;
	byte		*lightdata;

	int			texdatasize;
	byte		*texdata; // (dmiptexlump_t)

	int			entdatasize;
	char		*entdata;

	int			numleafs;
	dleaf_t		*leafs;

	int			numplanes;
	dplane_t	*planes;

	int			numvertexes;
	dvertex_t	*vertexes;

	int			numnodes;
	dnode_t		*nodes;

	int			numtexinfo;
	texinfo_t	*texinfo;

	int			numfaces;
	dface_t		*faces;

	int			numclipnodes;
	dclipnode_t	*clipnodes;

	int			numedges;
	dedge_t		*edges;

	int			nummarksurfaces;
	unsigned short	*marksurfaces;

	int			numsurfedges;
	int			*surfedges;
} bsp_t;

bsp_t *LoadBSPMem (void *mem, int size);
bsp_t *LoadBSPFile (QFile *file, int size);
void WriteBSPFile (bsp_t *bsp, QFile *file);
bsp_t *BSP_New (void);
void BSP_AddPlane (bsp_t *bsp, dplane_t *plane);
void BSP_AddLeaf (bsp_t *bsp, dleaf_t *leaf);
void BSP_AddVertex (bsp_t *bsp, dvertex_t *vertex);
void BSP_AddNode (bsp_t *bsp, dnode_t *node);
void BSP_AddTexinfo (bsp_t *bsp, texinfo_t *texinfo);
void BSP_AddFace (bsp_t *bsp, dface_t *face);
void BSP_AddClipnode (bsp_t *bsp, dclipnode_t *clipnode);
void BSP_AddMarkSurface (bsp_t *bsp, unsigned short marksurface);
void BSP_AddSurfEdge (bsp_t *bsp, int surfedge);
void BSP_AddEdge (bsp_t *bsp, dedge_t *edge);
void BSP_AddModel (bsp_t *bsp, dmodel_t *model);
void BSP_AddLighting (bsp_t *bsp, byte *lightdata, int lightdatasize);
void BSP_AddVisibility (bsp_t *bsp, byte *visdata, int visdatasize);
void BSP_AddEntities (bsp_t *bsp, char *entdata, int entdatasize);
void BSP_AddTextures (bsp_t *bsp, byte *texdata, int texdatasize);

#endif	// __bspfile_h_
