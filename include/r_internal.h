#ifndef __r_internal_h
#define __r_internal_h

#include "QF/vid.h"
#include "QF/plugin/vid_render.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "r_shared.h"

extern viddef_t 		vid;				// global video state

extern vid_render_data_t vid_render_data;
extern vid_render_funcs_t gl_vid_render_funcs;
extern vid_render_funcs_t glsl_vid_render_funcs;
extern vid_render_funcs_t sw_vid_render_funcs;
extern vid_render_funcs_t vulkan_vid_render_funcs;
extern vid_render_funcs_t *vid_render_funcs;

#define vr_data vid_render_data
#define vr_funcs vid_render_funcs

extern	refdef_t	r_refdef;
#define SW_COMP(comp, id) ((void *)((byte *)r_refdef.registry->comp_pools[r_refdef.scene->base + comp].data + (id) * r_refdef.registry->components.a[r_refdef.scene->base + comp].size))
extern int r_viewsize;

void R_LineGraph (int x, int y, int *h_vals, int count, int height);


void gl_R_Init (struct plitem_s *config);
void glsl_R_Init (struct plitem_s *config);
void glsl_R_Shutdown (void);
void sw_R_Init (struct plitem_s *config);
void R_RenderFrame (SCR_Func *scr_funcs);
void R_Init_Cvars (void);
void R_InitEfrags (void);
void R_ClearState (void);
void R_InitSky (struct texture_s *mt);	// called at level load
void R_Textures_Init (void);
void R_RenderView (void);			// must set r_refdef first
void R_ViewChanged (void);			// must set r_refdef first
								// called whenever r_refdef or vid change

extern struct psystem_s r_psystem;
extern struct psystem_s r_tsystem;
struct psystem_s *gl_ParticleSystem (void);
struct psystem_s *glsl_ParticleSystem (void);
struct psystem_s *glsl_TrailSystem (void);
struct psystem_s *sw_ParticleSystem (void);

struct scene_s;
void R_NewScene (struct scene_s *scene);

typedef struct visstate_s {
	const struct mleaf_s *viewleaf;
	int         *node_visframes;
	int         *leaf_visframes;
	int         *face_visframes;
	int          visframecount;
	const struct mod_brush_s *brush;
} visstate_t;

extern visstate_t r_visstate;//FIXME

// LordHavoc: relative bmodel lighting
void R_PushDlights (const vec3_t entorigin, const visstate_t *visstate);
void R_DrawWaterSurfaces (void);

void *D_SurfaceCacheAddress (void) __attribute__((pure));
int D_SurfaceCacheForRes (int width, int height);
void D_FlushCaches (void *data);
void D_DeleteSurfaceCache (void);
void D_InitCaches (void *buffer, int size);
void R_SetVrect (const vrect_t *pvrect, vrect_t *pvrectin, int lineadj);

void R_LoadSkys (const char *);

void R_ClearEfrags (void);

int R_FindNearLights (vec4f_t pos, int count, dlight_t **lights);

int R_InitGraphTextures (int base);

struct entity_s;
struct animation_s;
struct transform_s;
void R_DrawAliasModel (struct entity_s *e);

struct set_s;
void R_MarkLeavesPVS (visstate_t *state, const struct set_s *pvs);
void R_MarkLeaves (visstate_t *state, const struct mleaf_s *viewleaf);

void GL_SetPalette (void *data, const byte *palette);
void GLSL_SetPalette (void *data, const byte *palette);

int R_BillboardFrame (struct transform_s transform, int orientation,
					  vec4f_t cameravec,
					  vec4f_t *bbup, vec4f_t *bbright, vec4f_t *bbfwd);

// These correspond to the standard box sides for OpenGL cube maps but with
// TOP and BOTTOM swapped due to lelt/right handed systems (quake/gl are right,
// cube maps are left)
#define BOX_FRONT  4
#define BOX_RIGHT  0
#define BOX_BEHIND 5
#define BOX_LEFT   1
#define BOX_TOP    3
#define BOX_BOTTOM 2
void R_RenderFisheye (framebuffer_t *cube);

#endif//__r_internal_h
