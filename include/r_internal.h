#ifndef __r_internal_h
#define __r_internal_h

#include "QF/vid.h"
#include "QF/plugin/vid_render.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "r_screen.h"
#include "r_shared.h"

extern viddef_t 		vid;				// global video state

extern vid_render_data_t vid_render_data;
extern vid_render_funcs_t gl_vid_render_funcs;
extern vid_render_funcs_t glsl_vid_render_funcs;
extern vid_render_funcs_t sw_vid_render_funcs;
extern vid_render_funcs_t sw32_vid_render_funcs;
extern vid_render_funcs_t vulkan_vid_render_funcs;
extern vid_render_funcs_t *vid_render_funcs;

#define vr_data vid_render_data
#define vr_funcs vid_render_funcs

extern	refdef_t	r_refdef;
extern int r_viewsize;

void R_LineGraph (int x, int y, int *h_vals, int count, int height);

void Fog_Update (float density, float red, float green, float blue,
				 float time);
struct plitem_s;
void Fog_ParseWorldspawn (struct plitem_s *worldspawn);

void Fog_GetColor (quat_t fogcolor);
float Fog_GetDensity (void) __attribute__((pure));
void Fog_SetupFrame (void);
void Fog_EnableGFog (void);
void Fog_DisableGFog (void);
void Fog_StartAdditive (void);
void Fog_StopAdditive (void);
void Fog_Init (void);

void gl_R_Init (void);
void glsl_R_Init (void);
void sw_R_Init (void);
void sw32_R_Init (void);
void R_RenderFrame (SCR_Func *scr_funcs);
void R_Init_Cvars (void);
void R_InitEfrags (void);
void R_ClearState (void);
void R_InitSky (struct texture_s *mt);	// called at level load
void R_Textures_Init (void);
void R_RenderView (void);			// must set r_refdef first
void R_ViewChanged (void);			// must set r_refdef first
								// called whenever r_refdef or vid change

void R_AddEfrags (mod_brush_t *, entity_t *ent);
void R_RemoveEfrags (entity_t *ent);

void R_NewMap (model_t *worldmodel, model_t **models, int num_models);

// LordHavoc: relative bmodel lighting
void R_PushDlights (const vec3_t entorigin);
void R_DrawWaterSurfaces (void);

void *D_SurfaceCacheAddress (void) __attribute__((pure));
int D_SurfaceCacheForRes (void *data, int width, int height);
void D_FlushCaches (void *data);
void D_DeleteSurfaceCache (void);
void D_InitCaches (void *data, void *buffer, int size);
void R_SetVrect (const vrect_t *pvrect, vrect_t *pvrectin, int lineadj);

void R_LoadSkys (const char *);

void R_ClearEfrags (void);
void R_ClearEnts (void);
void R_EnqueueEntity (struct entity_s *ent);
entity_t *R_AllocEntity (void);
void R_FreeAllEntities (void);

void R_FindNearLights (const vec3_t pos, int count, dlight_t **lights);
dlight_t *R_AllocDlight (int key);
void R_DecayLights (double frametime);
void R_ClearDlights (void);

int R_InitGraphTextures (int base);

void R_DrawAliasModel (entity_t *e);

void R_MarkLeaves (void);

void GL_SetPalette (void *data, const byte *palette);
void GLSL_SetPalette (void *data, const byte *palette);

int R_BillboardFrame (entity_t *ent, int orientation, const vec3_t cameravec,
					  vec3_t bbup, vec3_t bbright, vec3_t bbpn);

#endif//__r_internal_h
