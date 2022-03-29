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
struct mod_sprite_ctx_s;
struct entqueue_s;
struct framebuffer_s;
struct vrect_s;

/*
	All video plugins must export these functions
*/

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
	void (*Mod_SpriteLoadFrames) (struct mod_sprite_ctx_s *sprite_ctx);

	struct skin_s *(*Skin_SetColormap) (struct skin_s *skin, int cmap);
	struct skin_s *(*Skin_SetSkin) (struct skin_s *skin, int cmap,
									const char *skinname);
	void (*Skin_SetupSkin) (struct skin_s *skin, int cmap);
	void (*Skin_SetTranslation) (int cmap, int top, int bottom);
	void (*Skin_ProcessTranslation) (int cmap, const byte *translation);
	void (*Skin_InitTranslations) (void);
} vid_model_funcs_t;

typedef struct vid_render_funcs_s {
	void      (*init) (void);
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

	struct tex_s *(*SCR_CaptureBGR) (void);

	struct psystem_s *(*ParticleSystem) (void);
	void (*R_Init) (void);
	void (*R_ClearState) (void);
	void (*R_LoadSkys) (const char *);
	void (*R_NewMap) (model_t *worldmodel, model_t **models, int num_models);
	void (*R_LineGraph) (int x, int y, int *h_vals, int count, int height);
	void (*R_ViewChanged) (void);

	void (*begin_frame) (void);
	void (*render_view) (void);
	void (*draw_entities) (struct entqueue_s *queue);
	void (*draw_particles) (struct psystem_s *psystem);
	void (*draw_transparent) (void);
	void (*post_process) (struct framebuffer_s *src);
	void (*set_2d) (int scaled);
	void (*end_frame) (void);

	struct framebuffer_s *(*create_cube_map) (int side);
	struct framebuffer_s *(*create_frame_buffer) (int width, int height);
	void (*bind_framebuffer) (struct framebuffer_s *framebuffer);
	void (*set_viewport) (const struct vrect_s *view);

	vid_model_funcs_t *model_funcs;
} vid_render_funcs_t;

typedef struct vid_render_data_s {
	viddef_t   *vid;
	refdef_t   *refdef;
	struct view_s *scr_view;
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
	double      frametime;
	double      realtime;
	lightstyle_t *lightstyle;
} vid_render_data_t;

#endif // __QF_plugin_vid_render_h
