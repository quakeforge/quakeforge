#ifndef __mod_internal_h
#define __mod_internal_h

#include "QF/darray.h"
#include "QF/iqm.h"
#include "QF/model.h"
#include "QF/qfmodel.h"
#include "QF/skin.h"
#include "QF/plugin/vid_render.h"

typedef struct mod_alias_skin_s {
	int         skin_num;
	int         group_num;	// -1 if not in an animated group
	byte       *texels;
	keyframe_t *skindesc;
} mod_alias_skin_t;

typedef struct stvertset_s DARRAY_TYPE (stvert_t) stvertset_t;
typedef struct dtriangleset_s DARRAY_TYPE (dtriangle_t) dtriangleset_t;
typedef struct trivertxset_s DARRAY_TYPE (trivertx_t *) trivertxset_t;
typedef struct askinset_s DARRAY_TYPE (mod_alias_skin_t) askinset_t;

typedef struct mod_alias_ctx_s {
	qf_model_t *model;
	qf_mesh_t  *mesh;
	model_t    *mod;
	mdl_t      *mdl;
	stvertset_t stverts;
	dtriangleset_t triangles;
	trivertxset_t poseverts;
	askinset_t  skins;
	int         skinwidth;
	int         skinheight;
	uint32_t    names_base;
	uint32_t    names_size;
	char       *names;
	uint32_t    numskins;
	daliasframe_t **dframes;	///< array of pointers to dframes (in)
	int         aliasbboxmins[3];
	int         aliasbboxmaxs[3];
} mod_alias_ctx_t;

typedef struct mod_iqm_ctx_s {
	qf_model_t *qf_model;
	qf_mesh_t  *qf_meshes;
	model_t    *mod;

	iqmheader  *hdr;
	const char *text;
	iqmmesh    *meshes;
	iqmvertexarray *vtxarr;
	iqmtriangle *triangles;
	iqmtriangle *adjacency;
	iqmjoint   *joints;
	iqmpose    *poses;
	iqmanim    *anims;
	uint16_t   *frames;
	iqmbounds  *bounds;
	const char *comment;
	float       average_area;
} mod_iqm_ctx_t;

typedef struct mod_sprite_ctx_s {
	model_t    *mod;
	dsprite_t  *dsprite;
	msprite_t  *sprite;
	int         numframes;
	dspriteframe_t **dframes;	///< array of pointers to dframes (in)
	keyframe_t **frames;
} mod_sprite_ctx_t;

uint32_t iqm_attr_size (const iqmvertexarray *a) __attribute__((const));
qfm_attrdesc_t iqm_mesh_attribute (iqmvertexarray a, uint32_t offset) __attribute__((const));
uint32_t mesh_type_size (qfm_type_t type) __attribute__((const));
qfm_type_t mesh_index_type (uint32_t num_verts) __attribute__((const));
uint32_t pack_indices (uint32_t *indices, uint32_t num_inds,
					   qfm_type_t index_type);


int Mod_CalcFullbright (byte *out, const byte *in, size_t pixels);
int Mod_ClearFullbright (byte *out, const byte *in, size_t pixels);
void Mod_FloodFillSkin (byte *skin, int skinwidth, int skinheight);
//FIXME gl specific. rewrite to use above
int Mod_Fullbright (byte * skin, int width, int height, const char *name);

void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_FindClipDepth (hull_t *hull);

model_t	*Mod_FindName (const char *name);
float RadiusFromBounds (const vec3_t mins, const vec3_t maxs) __attribute__((pure));

struct vulkan_ctx_s;

extern vid_model_funcs_t *m_funcs;

void gl_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx, void *_m,
										int _s, int extra);
void gl_Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx);
void gl_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx);
void gl_Mod_LoadExternalSkins (mod_alias_ctx_t *alias_ctx);
void gl_Mod_IQMFinish (mod_iqm_ctx_t *iqm_ctx);

void glsl_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx,
										  void *_m, int _s, int extra);
void glsl_Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx);
void glsl_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx);
void glsl_Mod_LoadExternalSkins (mod_alias_ctx_t *alias_ctx);
void glsl_Mod_IQMFinish (mod_iqm_ctx_t *iqm_ctx);

void sw_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx, void *_m,
										int _s, int extra);
void sw_Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx);
void sw_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx);
void sw_Mod_IQMFinish (mod_iqm_ctx_t *iqm_ctx);

void gl_Mod_LoadLighting (model_t *mod, bsp_t *bsp);
void gl_Mod_SubdivideSurface (model_t *mod, msurface_t *fa);
struct texture_s;
void gl_Mod_ProcessTexture (model_t *mod, struct texture_s *tx);

void glsl_Mod_LoadLighting (model_t *mod, bsp_t *bsp);
void glsl_Mod_ProcessTexture (model_t *mod, struct texture_s *tx);

void sw_Mod_LoadLighting (model_t *mod, bsp_t *bsp);

void Vulkan_Mod_LoadLighting (model_t *mod, bsp_t *bsp,
							  struct vulkan_ctx_s *ctx);
void Vulkan_Mod_SubdivideSurface (model_t *mod, msurface_t *fa,
								  struct vulkan_ctx_s *ctx);
void Vulkan_Mod_ProcessTexture (model_t *mod, struct texture_s *tx,
								struct vulkan_ctx_s *ctx);

void Mod_LoadSpriteFrame (mspriteframe_t *frame, const dspriteframe_t *dframe);
void gl_Mod_SpriteLoadFrames (mod_sprite_ctx_t *sprite_ctx);
void glsl_Mod_SpriteLoadFrames (mod_sprite_ctx_t *sprite_ctx);
void sw_Mod_SpriteLoadFrames (mod_sprite_ctx_t *sprite_ctx);

void Mod_LoadIQM (model_t *mod, void *buffer);
qfm_blend_t *Mod_IQMBuildBlendPalette (mod_iqm_ctx_t *iqm, uint32_t *size);

void Mod_LoadAliasModel (model_t *mod, void *buffer,
                         cache_allocator_t allocator);
void Mod_LoadSpriteModel (model_t *mod, void *buffer);

int Skin_CalcTopColors (byte *out, const byte *in, size_t pixels, int stride);
int Skin_CalcTopMask (byte *out, const byte *in, size_t pixels, int stride);
int Skin_CalcBottomColors(byte *out, const byte *in, size_t pixels, int stride);
int Skin_CalcBottomMask (byte *out, const byte *in, size_t pixels, int stride);
int Skin_ClearTopColors (byte *out, const byte *in, size_t pixels);
int Skin_ClearBottomColors (byte *out, const byte *in, size_t pixels);
void Skin_SetColormap (byte *dest, int top, int bottom);
void Skin_SetPalette (byte *dest, int top, int bottom);

typedef struct tex_s tex_t;
typedef struct colormap_s colormap_t;

tex_t *Skin_DupTex (const tex_t *tex);

typedef struct skin_s {
	tex_t      *tex;
	uint32_t    id;
} skin_t;

typedef struct glskin_s {
	uint32_t    id;
	uint32_t    fb;
} glskin_t;

void Skin_Init (void);
uint32_t Skin_Set (const char *skinname);
skin_t *Skin_Get (uint32_t skin) __attribute__((pure));

void sw_Skin_SetupSkin (skin_t *skin);
void sw_Skin_Destroy (skin_t *skin);
const byte *sw_Skin_Colormap (const colormap_t *colormap);

void glsl_Skin_SetupSkin (skin_t *skin);
void glsl_Skin_Destroy (skin_t *skin);
uint32_t glsl_Skin_Colormap (const colormap_t *colormap);

void gl_Skin_SetupSkin (skin_t *skin);
void gl_Skin_Destroy (skin_t *skin);
glskin_t gl_Skin_Get (const tex_t *tex, const colormap_t *colormap,
					  const byte *texel_base);

#endif// __mod_internal_h
