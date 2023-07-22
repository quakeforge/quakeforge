/*
	model.h

	(description)

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

#ifndef __QF_model_h
#define __QF_model_h

#include "QF/qtypes.h"
#include "QF/bspfile.h"
#include "QF/spritegn.h"
#include "QF/modelgen.h"
#include "QF/zone.h"

#include "QF/simd/types.h"

extern struct vid_model_funcs_s *mod_funcs;

/*
	d*_t structures are on-disk representations
	m*_t structures are in-memory
*/

// entity effects =============================================================

#define	EF_BRIGHTFIELD	1
#define	EF_MUZZLEFLASH 	2
#define	EF_BRIGHTLIGHT 	4
#define	EF_DIMLIGHT 	8
#define	EF_FLAG1	 	16
#define	EF_FLAG2	 	32
#define EF_BLUE			64
#define EF_RED			128

// BRUSH MODELS ===============================================================

// in memory representation ===================================================

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct {
	vec3_t		position;
} mvertex_t;

/** Instanced surface data.

	There will be one of these for each surface in the world model. This
	covers the sub-models in the world model. These instanced surfaces will
	be allocated in one block at map load time and then never freed until
	the next map load.

	However, for instanced brush models (ammo boxes, health boxes, etc), an
	instanced surface will be allocated for each surface for each model
	once per frame. These instanced surfaces will be mass-freed each frame.
*/
typedef struct instsurf_s {
	struct instsurf_s *_next;		///< next in free/alloc list
	struct instsurf_s *tex_chain;	///< next in texture chain
	struct instsurf_s *lm_chain;	///< next in lightmap chain
	struct msurface_s *surface;		///< surface to render
	vec4f_t    *transform;
	float      *color;
} instsurf_t;

#define ANIM_CYCLE 2
typedef struct texture_s {
	char		name[16];
	unsigned    width, height;
	void       *render;		// renderer specific data
	int			anim_total;				// total tenths in sequence ( 0 = no)
	int			anim_min, anim_max;		// time for this frame min <=time< max
	struct texture_s *anim_next;		// in the animation sequence
	struct texture_s *alternate_anims;	// bmodels in frmae 1 use these
	unsigned    offsets[MIPLEVELS];		// four mip maps stored
} texture_t;


#define SURF_DRAWSKY            0x01
#define SURF_DRAWALPHA          0x02
#define SURF_DRAWTURB           0x04
#define SURF_PLANEBACK          0x08
#define SURF_DRAWTILED          0x10
#define SURF_DRAWBACKGROUND     0x20
#define SURF_LIGHTBOTHSIDES     0x40

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct {
	unsigned int	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct {
	float		vecs[2][4];
	float		mipadjust;
	texture_t	*texture;
	int			flags;
} mtexinfo_t;

#define	VERTEXSIZE	7

typedef struct glpoly_s {
	struct	glpoly_s	*next;
	int		numverts;
	int		flags;					// for SURF_UNDERWATER
	float	verts[4][VERTEXSIZE];	// variable sized (xyz s1t1 s2t2)
} glpoly_t;

#define MAX_DLIGHTS 128

typedef struct msurface_s {
	int			_visframe;	// should be drawn when node is crossed/
							// no longer used, see r_face_visframes

	int			flags;
	plane_t		*plane;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges

	union {
		// sw-only
		struct surfcache_s	*cachespots[MIPLEVELS];
		// gl/glsl/vulkan
		struct {
			glpoly_t   *polys;	// multiple if warped
			instsurf_t *instsurf;///< null if not part of world model/sub-model
			struct subpic_s *lightpic;///< light map texture ref (glsl)
			byte       *base;
		};
	};

	mtexinfo_t *texinfo;
	short		texturemins[2];
	unsigned short extents[2];

// lighting info
	byte		*samples;		// [numstyles*surfsize]
	int			dlightframe;
	uint32_t    dlightbits[(MAX_DLIGHTS + 31) >> 5];

	byte		styles[MAXLIGHTMAPS];
	int			cached_light[MAXLIGHTMAPS];	// values currently used in lightmap
	bool		cached_dlight;				// true if dynamic light in cache

	int         model_index;	///< < 0: instance, 0 main, > 0: sub
} msurface_t;

typedef struct mnode_s {
	/// Vector representation of the plane.
	/// \note the 4th component is the negative of the plane distance. This
	/// makes it so a 4d dot product with a point vector (4th component = 1)
	/// gives the distance of the point from the plane.
	vec4f_t     plane;
	byte        type;
	byte        signbits;
	byte        pad[2];
	int32_t     children[2];	///< Negative values = ~(leaf number)
	float       minmaxs[6];
	uint32_t    firstsurface;
	uint32_t    numsurfaces;
} mnode_t;

typedef struct mleaf_s {
// common with node
	int			contents;		// wil be a negative contents number
	int			cluster;		// cluster to which this leaf bleongs XXX

	// for bounding box culling
	float		mins[3];
	float		maxs[3];

// leaf specific
	byte		*compressed_vis;
	struct efrag_s *efrags;

	int         firstmarksurface;
	int			nummarksurfaces;
	int			key;			// BSP sequence number for leaf's contents
	byte		ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct hull_s {
	dclipnode_t	*clipnodes;
	plane_t		*planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
	struct nodeleaf_s *nodeleafs;
	int         depth;				///< maximum depth of the tree
} hull_t;

typedef struct mod_brush_s {
	unsigned    firstmodelsurface, nummodelsurfaces;

	unsigned    numsubmodels;
	dmodel_t   *submodels;

	unsigned    numplanes;
	plane_t    *planes;

	unsigned    modleafs;		///< number of leafs in model, including 0
	unsigned    visleafs;		///< number of visible leafs, not counting 0
	mleaf_t    *leafs;

	unsigned    numvertexes;
	mvertex_t  *vertexes;

	unsigned    numedges;
	medge_t    *edges;

	unsigned    numnodes;
	mnode_t    *nodes;
	int         depth;				///< maximum depth of the tree

	unsigned    numtexinfo;
	mtexinfo_t *texinfo;

	unsigned    numsurfaces;
	msurface_t *surfaces;

	unsigned    numsurfedges;
	int        *surfedges;

	unsigned    numclipnodes;
	dclipnode_t *clipnodes;

	unsigned    nummarksurfaces;
	msurface_t **marksurfaces;

	hull_t      hulls[MAX_MAP_HULLS];
	hull_t     *hull_list[MAX_MAP_HULLS];

	unsigned    numtextures;
	texture_t **textures;
	texture_t  *skytexture;

	byte       *visdata;
	byte       *lightdata;
	char       *entities;	//FIXME should not be here

	int32_t    *node_parents;
	int32_t    *leaf_parents;
	int        *leaf_flags;	// union of surf flags for surfs in leaf

	unsigned int checksum;
	unsigned int checksum2;
} mod_brush_t;

// SPRITE MODELS ==============================================================

typedef struct mspriteframe_s {
	int		width;
	int		height;
	float	up, down, left, right;
	byte	pixels[4];
	int		gl_texturenum;
} mspriteframe_t;

typedef struct {
	int				numframes;
	float			*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct {
	spriteframetype_t	type;
	union {
		mspriteframe_t *frame;
		mspritegroup_t *group;
	};
} mspriteframedesc_t;

typedef struct {
	int         type;
	float       beamlength;
	int         numframes;
	int         data;
	mspriteframedesc_t	frames[1];
} msprite_t;


// ALIAS MODELS ===============================================================
// Alias models are position independent, so the cache manager can move them.

// NOTE: the first three lines must match maliasgroupframedesc_t
typedef struct {
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
	aliasframetype_t	type;
	int					firstpose;
	int					numposes;
	float				interval;
	char				name[16];
} maliasframedesc_t;

typedef struct {
	aliasskintype_t type;
	int     skin;
	int     texnum;
	int     fb_texnum;
} maliasskindesc_t;

typedef struct {
	int					numframes;
	int					intervals;
	maliasframedesc_t	frames[1];
} maliasgroup_t;

typedef struct {
	int					numskins;
	int					intervals;
	maliasskindesc_t	skindescs[1];
} maliasskingroup_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mtriangle_s {
	int					facesfront;
	int					vertindex[3];
} mtriangle_t;


#define	MAX_SKINS	32
typedef struct {
	int			model;
	int			stverts;
	int			skindesc;
	int			triangles;

	mdl_t		mdl;
	int			tex_coord;
	int			numposes;
	int			poseverts;
	int			posedata;	// numposes * poseverts trivert_t
	int			commands;	// gl command list with embedded s/t

	unsigned short crc;

	maliasframedesc_t	frames[1];
} aliashdr_t;

// Whole model ================================================================

typedef enum {
	mod_brush,
	mod_sprite,
	mod_alias,
	mod_iqm,

	mod_light,

	mod_num_types
} modtype_t;

#define	EF_ROCKET		1			// leave a trail
#define	EF_GRENADE		2			// leave a trail
#define	EF_GIB			4			// leave a trail
#define	EF_ROTATE		8			// rotate (bonus items)
#define	EF_TRACER		16			// green split trail
#define	EF_ZOMGIB		32			// small blood trail
#define	EF_TRACER2		64			// orange split trail + rotate
#define	EF_TRACER3		128			// purple trail
#define EF_GLOWTRAIL	4096		// glowcolor particle trail

typedef struct model_s {
	//FIXME use pointers. needs care in bsp submodel loading
	char		 path[MAX_QPATH];
	char		 name[MAX_QPATH];
	const struct vpath_s *vpath;// virtual path where this model was found
	bool		 needload;		// bmodels and sprites don't cache normally
	aliashdr_t  *aliashdr;		// if not null, alias model is not cached
	bool		 hasfullbrights;

	modtype_t	 type;
	int			 numframes;
	synctype_t	 synctype;

	int			 flags;

// lighting info
	float		 min_light;
	byte		 shadow_alpha;	// 255 = 1.0
	byte		 fullbright;

// coarse cull, for fine culling, axis-aligned bbox isn't good enough
	float		 radius;
// FIXME: bbox cruft has to stay until sw rendering gets updated
	vec3_t		 mins, maxs;

// solid volume for clipping
	bool		 clipbox;
	vec3_t		 clipmins, clipmaxs;

// brush model
	//FIXME should be a pointer (submodels make things tricky)
	mod_brush_t brush;

// additional model data
	cache_user_t cache;
	void      (*clear) (struct model_s *m, void *data);
	void       *data;

	int         render_id;
} model_t;

// ============================================================================

void Mod_Init (void);
void Mod_Init_Cvars (void);
void Mod_ClearAll (void);
model_t *Mod_ForName (const char *name, bool crash);
void Mod_TouchModel (const char *name);
void Mod_UnloadModel (model_t *model);
// brush specific
mleaf_t *Mod_PointInLeaf (vec4f_t p, const mod_brush_t *brush) __attribute__((pure));
struct set_s *Mod_LeafPVS (const mleaf_t *leaf, const mod_brush_t *brush);

void Mod_LeafPVS_set (const mleaf_t *leaf, const mod_brush_t *brush,
					  byte defvis, struct set_s *pvs);
void Mod_LeafPVS_mix (const mleaf_t *leaf, const mod_brush_t *brush,
					  byte defvis, struct set_s *pvs);

void Mod_Print (void);

extern int gl_mesh_cache;
extern float gl_subdivide_size;
extern int gl_alias_render_tri;
extern int gl_textures_external;
extern int mod_sky_divide;
extern int mod_lightmap_bytes;

#endif//__QF_model_h
