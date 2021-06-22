/*
	QF/plugin/vid_render.h

	Video Renderer plugin data types

	Copyright (C) 2001 Jeff Teunissen <deek@quakeforge.net>

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
#ifndef __QF_plugin_vid_render_h
#define __QF_plugin_vid_render_h

#include <QF/draw.h>
#include <QF/plugin.h>
#include <QF/qtypes.h>
#include <QF/render.h>
#include <QF/screen.h>

struct plitem_s;
struct cvar_s;
struct skin_s;

struct mod_alias_ctx_s;

/*
	All video plugins must export these functions
*/

typedef struct vid_particle_funcs_s {
	void (*R_RocketTrail) (const struct entity_s *ent);
	void (*R_GrenadeTrail) (const struct entity_s *ent);
	void (*R_BloodTrail) (const struct entity_s *ent);
	void (*R_SlightBloodTrail) (const struct entity_s *ent);
	void (*R_WizTrail) (const struct entity_s *ent);
	void (*R_FlameTrail) (const struct entity_s *ent);
	void (*R_VoorTrail) (const struct entity_s *ent);
	void (*R_GlowTrail) (const struct entity_s *ent, int glow_color);

	void (*R_RunParticleEffect) (const vec3_t org, const vec3_t dir,
									int color, int count);
	void (*R_BloodPuffEffect) (const vec3_t org, int count);
	void (*R_GunshotEffect) (const vec3_t org, int count);
	void (*R_LightningBloodEffect) (const vec3_t org);
	void (*R_SpikeEffect) (const vec3_t org);
	void (*R_KnightSpikeEffect) (const vec3_t org);
	void (*R_SuperSpikeEffect) (const vec3_t org);
	void (*R_WizSpikeEffect) (const vec3_t org);

	void (*R_BlobExplosion) (const vec3_t org);
	void (*R_ParticleExplosion) (const vec3_t org);
	void (*R_ParticleExplosion2) (const vec3_t org, int colorStart,
									 int colorLength);
	void (*R_LavaSplash) (const vec3_t org);
	void (*R_TeleportSplash) (const vec3_t org);
	void (*R_DarkFieldParticles) (const struct entity_s *ent);
	void (*R_EntityParticles) (const struct entity_s *ent);

	void (*R_Particle_New) (ptype_t type, int texnum, const vec3_t org,
							float scale, const vec3_t vel, float die,
							int color, float alpha, float ramp);
	void (*R_Particle_NewRandom) (ptype_t type, int texnum, const vec3_t org,
								  int org_fuzz, float scale, int vel_fuzz,
								  float die, int color, float alpha,
								  float ramp);
} vid_particle_funcs_t;

typedef struct vid_model_funcs_s {
	size_t      texture_render_size;// size of renderer specific texture data
	void (*Mod_LoadLighting) (model_t *mod, bsp_t *bsp);
	void (*Mod_SubdivideSurface) (model_t *mod, msurface_t *fa);
	void (*Mod_ProcessTexture) (model_t *mod, texture_t *tx);
	void (*Mod_LoadIQM) (model_t *mod, void *buffer);
	void (*Mod_LoadAliasModel) (model_t *mod, void *buffer,
								cache_allocator_t allocator);
	void (*Mod_LoadSpriteModel) (model_t *mod, void *buffer);
	void (*Mod_MakeAliasModelDisplayLists) (struct mod_alias_ctx_s *alias_ctx,
											void *_m, int _s, int extra);
	void *(*Mod_LoadSkin) (struct mod_alias_ctx_s *alias_ctx, byte *skin,
						   int skinsize, int snum, int gnum,
						   qboolean group, maliasskindesc_t *skindesc);
	void (*Mod_FinalizeAliasModel) (struct mod_alias_ctx_s *alias_ctx);
	void (*Mod_LoadExternalSkins) (struct mod_alias_ctx_s *alias_ctx);
	void (*Mod_IQMFinish) (model_t *mod);
	int alias_cache;
	void (*Mod_SpriteLoadTexture) (model_t *mod, mspriteframe_t *pspriteframe,
								   int framenum);

	struct skin_s *(*Skin_SetColormap) (struct skin_s *skin, int cmap);
	struct skin_s *(*Skin_SetSkin) (struct skin_s *skin, int cmap,
									const char *skinname);
	void (*Skin_SetupSkin) (struct skin_s *skin, int cmap);
	void (*Skin_SetTranslation) (int cmap, int top, int bottom);
	void (*Skin_ProcessTranslation) (int cmap, const byte *translation);
	void (*Skin_InitTranslations) (void);
} vid_model_funcs_t;

typedef struct vid_render_funcs_s {
	void (*Draw_Character) (int x, int y, unsigned ch);
	void (*Draw_String) (int x, int y, const char *str);
	void (*Draw_nString) (int x, int y, const char *str, int count);
	void (*Draw_AltString) (int x, int y, const char *str);
	void (*Draw_ConsoleBackground) (int lines, byte alpha);
	void (*Draw_Crosshair) (void);
	void (*Draw_CrosshairAt) (int ch, int x, int y);
	void (*Draw_TileClear) (int x, int y, int w, int h);
	void (*Draw_Fill) (int x, int y, int w, int h, int c);
	void (*Draw_TextBox) (int x, int y, int width, int lines, byte alpha);
	void (*Draw_FadeScreen) (void);
	void (*Draw_BlendScreen) (quat_t color);
	qpic_t *(*Draw_CachePic) (const char *path, qboolean alpha);
	void (*Draw_UncachePic) (const char *path);
	qpic_t *(*Draw_MakePic) (int width, int height, const byte *data);
	void (*Draw_DestroyPic) (qpic_t *pic);
	qpic_t *(*Draw_PicFromWad) (const char *name);
	void (*Draw_Pic) (int x, int y, qpic_t *pic);
	void (*Draw_Picf) (float x, float y, qpic_t *pic);
	void (*Draw_SubPic) (int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height);

	void (*SCR_SetFOV) (float fov);
	void (*SCR_DrawRam) (void);
	void (*SCR_DrawTurtle) (void);
	void (*SCR_DrawPause) (void);
	struct tex_s *(*SCR_CaptureBGR) (void);
	struct tex_s *(*SCR_ScreenShot) (unsigned width, unsigned height);
	void (*SCR_DrawStringToSnap) (const char *s, struct tex_s *tex,
								  int x, int y);

	void (*Fog_Update) (float density, float red, float green, float blue,
						float time);
	void (*Fog_ParseWorldspawn) (struct plitem_s *worldspawn);

	void (*R_Init) (void);
	void (*R_RenderFrame) (SCR_Func scr_3dfunc, SCR_Func *scr_funcs);
	void (*R_ClearState) (void);
	void (*R_LoadSkys) (const char *);
	void (*R_NewMap) (model_t *worldmodel, model_t **models, int num_models);
	void (*R_AddEfrags) (mod_brush_t *brush, entity_t *ent);
	void (*R_RemoveEfrags) (entity_t *ent);
	void (*R_EnqueueEntity) (struct entity_s *ent);	//FIXME should not be here
	void (*R_LineGraph) (int x, int y, int *h_vals, int count);
	dlight_t *(*R_AllocDlight) (int key);
	entity_t *(*R_AllocEntity) (void);
	void (*R_MaxDlightsCheck) (struct cvar_s *var);
	void (*R_RenderView) (void);
	void (*R_DecayLights) (double frametime);

	void (*R_ViewChanged) (void);
	void (*R_ClearParticles) (void);
	void (*R_InitParticles) (void);
	void (*SCR_ScreenShot_f) (void);
	void (*r_easter_eggs_f) (struct cvar_s *var);
	void (*r_particles_style_f) (struct cvar_s *var);

	vid_particle_funcs_t *particles;
	vid_model_funcs_t *model_funcs;
} vid_render_funcs_t;

typedef struct vid_render_data_s {
	viddef_t   *vid;
	refdef_t   *refdef;
	vrect_t    *scr_vrect;
	int         scr_copytop;
	int         scr_copyeverything;
	int         scr_fullupdate;	// set to 0 to force full redraw
	void       (*viewsize_callback) (struct cvar_s *);
	struct cvar_s *scr_viewsize;
	struct cvar_s *graphheight;
	float       min_wateralpha;
	qboolean    force_fullscreen;
	qboolean    inhibit_viewmodel;
	qboolean    paused;
	int         lineadj;
	struct entity_s *view_model;
	struct entity_s *player_entity;
	float       gravity;
	double      frametime;
	double      realtime;
	lightstyle_t *lightstyle;
	vec_t      *origin;
	vec_t      *vpn;
	vec_t      *vright;
	vec_t      *vup;
} vid_render_data_t;

#endif // __QF_plugin_vid_render_h
