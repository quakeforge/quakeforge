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

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/bspfile.h"
#include "QF/spritegn.h"
#include "QF/modelgen.h"
#include "QF/zone.h"

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

typedef struct efrag_s {
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;


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
	vec_t      *transform;
	float      *color;
} instsurf_t;

typedef struct texture_s {
	char		*name;
	unsigned    width, height;
	void       *render;		// renderer specific data
	int			anim_total;				// total tenths in sequence ( 0 = no)
	int			anim_min, anim_max;		// time for this frame min <=time< max
	struct texture_s *anim_next;		// in the animation sequence
	struct texture_s *alternate_anims;	// bmodels in frmae 1 use these
	unsigned    offsets[MIPLEVELS];		// four mip maps stored
} texture_t;


#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80
#define SURF_DONTWARP		0x100
#define SURF_DRAWNOALPHA	0x200
#define SURF_DRAWFULLBRIGHT	0x400
#define SURF_LIGHTBOTHSIDES 0x800

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
	int			visframe;		// should be drawn when node is crossed

	plane_t		*plane;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges

	struct surfcache_s	*cachespots[MIPLEVELS];

	short		texturemins[2];
	unsigned short extents[2];

	int			light_s, light_t;	// gl lightmap coordinates

	glpoly_t   *polys;				// multiple if warped
	instsurf_t *instsurf;	///< null if not part of world model/sub-model
	instsurf_t *tinst;		///< for instance models

	mtexinfo_t *texinfo;
	int         ec_index;
	byte       *base;

// lighting info
	struct subpic_s *lightpic;	///< light map texture ref (glsl)
	int			dlightframe;
	uint32_t    dlightbits[(MAX_DLIGHTS + 31) >> 5];

	int			lightmaptexturenum;
	byte		styles[MAXLIGHTMAPS];
	int			cached_light[MAXLIGHTMAPS];	// values currently used in lightmap
	qboolean	cached_dlight;				// true if dynamic light in cache
	byte		*samples;		// [numstyles*surfsize]
} msurface_t;

typedef struct mnode_s {
// common with leaf
	int			contents;		// 0, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// node specific
	plane_t		*plane;
	struct mnode_s	*children[2];

	unsigned int		firstsurface;
	unsigned int		numsurfaces;
} mnode_t;

typedef struct mleaf_s {
// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	// for bounding box culling
	float		mins[3];
	float		maxs[3];

	struct mnode_s	*parent;

// leaf specific
	byte		*compressed_vis;
	efrag_t		*efrags;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
	int			key;			// BSP sequence number for leaf's contents
	byte		ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct mclipnode_s {
	int         planenum;
	int         children[2];
} mclipnode_t;

typedef struct hull_s {
	mclipnode_t	*clipnodes;
	plane_t		*planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
	struct nodeleaf_s *nodeleafs;
	int         depth;				///< maximum depth of the tree
} hull_t;

// SPRITE MODELS ==============================================================

// FIXME: shorten these?
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
	mspriteframe_t		*frameptr;
} mspriteframedesc_t;

typedef struct {
	int					type;
	int					maxwidth;
	int					maxheight;
	int					numframes;
	float				beamlength;		// remove?
	void				*cachespot;		// remove?
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
	int            skin;
	union {
		struct {
			int     texnum;
			int     fb_texnum;
		};
		void       *tex;
	};
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

typedef struct stvertset_s DARRAY_TYPE (stvert_t) stvertset_t;
typedef struct mtriangleset_s DARRAY_TYPE (mtriangle_t) mtriangleset_t;
typedef struct trivertxset_s DARRAY_TYPE (trivertx_t *) trivertxset_t;
extern	aliashdr_t	*pheader;
extern	stvertset_t	 stverts;
extern	mtriangleset_t triangles;
extern	trivertxset_t poseverts;
extern  int			 aliasbboxmins[3];
extern  int			 aliasbboxmaxs[3];

// Whole model ================================================================

typedef enum {mod_brush, mod_sprite, mod_alias, mod_iqm} modtype_t;

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
	char		 name[MAX_QPATH];
	const struct vpath_s *vpath;// virtual path where this model was found
	qboolean	 needload;		// bmodels and sprites don't cache normally
	aliashdr_t  *aliashdr;		// if not null, alias model is not cached
	qboolean	 hasfullbrights;

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
	qboolean	 clipbox;
	vec3_t		 clipmins, clipmaxs;

// brush model
	int			 firstmodelsurface, nummodelsurfaces;

	int			 numsubmodels;
	dmodel_t	*submodels;

	int			 numplanes;
	plane_t		*planes;

	int			 numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			 numvertexes;
	mvertex_t	*vertexes;

	int			 numedges;
	medge_t		*edges;

	int			 numnodes;
	mnode_t		*nodes;
	int          depth;				///< maximum depth of the tree

	int			 numtexinfo;
	mtexinfo_t	*texinfo;

	int			 numsurfaces;
	msurface_t	*surfaces;

	int			 numsurfedges;
	int			*surfedges;

	int			 numclipnodes;
	mclipnode_t	*clipnodes;

	int			 nummarksurfaces;
	msurface_t	**marksurfaces;

	hull_t		 hulls[MAX_MAP_HULLS];
	hull_t		*hull_list[MAX_MAP_HULLS];

	int			 numtextures;
	texture_t	**textures;
	texture_t	*skytexture;

	byte		*visdata;
	byte		*lightdata;
	char		*entities;

	unsigned int checksum;
	unsigned int checksum2;

// additional model data
	cache_user_t cache;
	void      (*clear) (struct model_s *m, void *data);
	void       *data;
} model_t;

// ============================================================================

extern float RadiusFromBounds (const vec3_t mins, const vec3_t maxs) __attribute__((pure));
void	Mod_Init (void);
void	Mod_Init_Cvars (void);
void	Mod_ClearAll (void);
model_t *Mod_ForName (const char *name, qboolean crash);
void	*Mod_Extradata (model_t *mod);	// handles caching
void	Mod_TouchModel (const char *name);

mleaf_t *Mod_PointInLeaf (const vec3_t p, model_t *model) __attribute__((pure));
byte	*Mod_LeafPVS (mleaf_t *leaf, model_t *model);
model_t	*Mod_FindName (const char *name);
int     Mod_CalcFullbright (byte *in, byte *out, int pixels);
int     Mod_Fullbright (byte * skin, int width, int height, const char *name);

void    *Mod_LoadAliasFrame (void *pin, int *posenum, maliasframedesc_t *frame,
							 int extra);
void    *Mod_LoadAliasGroup (void *pin, int *posenum, maliasframedesc_t *frame,
							 int extra);

void Mod_FindClipDepth (hull_t *hull);
void	 Mod_LoadBrushModel (model_t *mod, void *buffer);
void	 Mod_FloodFillSkin (byte * skin, int skinwidth, int skinheight);

void	 Mod_Print (void);

extern struct cvar_s *gl_mesh_cache;
extern struct cvar_s *gl_subdivide_size;
extern struct cvar_s *gl_alias_render_tri;
extern struct cvar_s *gl_textures_external;
extern model_t *loadmodel;
extern char *loadname;
extern byte *mod_base;
extern byte mod_novis[MAX_MAP_LEAFS / 8];
extern int mod_lightmap_bytes;

#endif//__QF_model_h
