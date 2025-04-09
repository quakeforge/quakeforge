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

#include "QF/scene/entity.h"//FIXME

struct plitem_s;
struct cvar_s;
struct scene_s;
struct particle_s;

struct mod_alias_ctx_s;
struct mod_iqm_ctx_s;
struct mod_sprite_ctx_s;
typedef struct skin_s skin_t;
struct entqueue_s;
struct framebuffer_s;
struct vrect_s;
struct texture_s;

/*
	All video plugins must export these functions
*/

typedef struct vid_model_funcs_s {
	size_t      texture_render_size;// size of renderer specific texture data
	void (*Mod_LoadLighting) (model_t *mod, bsp_t *bsp);
	void (*Mod_SubdivideSurface) (model_t *mod, msurface_t *fa);
	void (*Mod_ProcessTexture) (model_t *mod, struct texture_s *tx);
	void (*Mod_LoadIQM) (model_t *mod, void *buffer);
	void (*Mod_LoadAliasModel) (model_t *mod, void *buffer,
								cache_allocator_t allocator);
	void (*Mod_LoadSpriteModel) (model_t *mod, void *buffer);
	void (*Mod_MakeAliasModelDisplayLists) (struct mod_alias_ctx_s *alias_ctx,
											void *_m, int _s, int extra);
	void (*Mod_LoadAllSkins) (struct mod_alias_ctx_s *alias_ctx);
	void (*Mod_FinalizeAliasModel) (struct mod_alias_ctx_s *alias_ctx);
	void (*Mod_LoadExternalSkins) (struct mod_alias_ctx_s *alias_ctx);
	void (*Mod_IQMFinish) (struct mod_iqm_ctx_s *iqm_ctx);
	int alias_cache;
	void (*Mod_SpriteLoadFrames) (struct mod_sprite_ctx_s *sprite_ctx);

	uint32_t (*skin_set) (const char *skinname);
	void (*skin_setupskin) (skin_t *skin);
	void (*skin_destroy) (skin_t *skin);
} vid_model_funcs_t;

struct tex_s;
struct font_s;
struct draw_charbuffer_s;
struct imui_ctx_s;

typedef void (*capfunc_t) (struct tex_s *screencap, void *data);

typedef struct vid_render_funcs_s {
	void      (*init) (void);
	void (*UpdateScreen) (SCR_Func *scr_funcs);
	void (*Draw_CharBuffer) (int x, int y, struct draw_charbuffer_s *buffer);
	void (*Draw_SetScale) (int scale);
	void (*Draw_Character) (int x, int y, unsigned ch);
	void (*Draw_String) (int x, int y, const char *str);
	void (*Draw_nString) (int x, int y, const char *str, int count);
	void (*Draw_AltString) (int x, int y, const char *str);
	void (*Draw_ConsoleBackground) (int lines, byte alpha);
	void (*Draw_Crosshair) (void);
	void (*Draw_CrosshairAt) (int ch, int x, int y);
	void (*Draw_TileClear) (int x, int y, int w, int h);
	void (*Draw_Fill) (int x, int y, int w, int h, int c);
	void (*Draw_FillRGBA) (int x, int y, int w, int h, const quat_t rgba);
	void (*Draw_Line) (int x0, int y0, int x1, int y1, int c);
	void (*Draw_TextBox) (int x, int y, int width, int lines, byte alpha);
	void (*Draw_FadeScreen) (void);
	void (*Draw_BlendScreen) (quat_t color);
	qpic_t *(*Draw_CachePic) (const char *path, bool alpha);
	void (*Draw_UncachePic) (const char *path);
	qpic_t *(*Draw_MakePic) (int width, int height, const byte *data);
	void (*Draw_DestroyPic) (qpic_t *pic);
	qpic_t *(*Draw_PicFromWad) (const char *name);
	void (*Draw_Pic) (int x, int y, qpic_t *pic);
	void (*Draw_FitPic) (int x, int y, int width, int height, qpic_t *pic);
	void (*Draw_Picf) (float x, float y, qpic_t *pic);
	void (*Draw_SubPic) (int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height);
	int (*Draw_AddFont) (struct font_s *font);
	void (*Draw_Glyph) (int x, int y, int fontid, int glyphid, int c);
	void (*Draw_SetClip) (int x, int y, int w, int h);
	void (*Draw_ResetClip) (void);
	void (*Draw_Flush) (void);

	struct psystem_s *(*ParticleSystem) (void);
	struct psystem_s *(*TrailSystem) (void);
	void (*R_Init) (struct plitem_s *config);
	void (*R_ClearState) (void);
	void (*R_LoadSkys) (const char *);
	void (*R_NewScene) (struct scene_s *scene);
	void (*R_LineGraph) (int x, int y, int *h_vals, int count, int height);

	void (*begin_frame) (void);
	void (*render_view) (void);
	void (*draw_particles) (struct psystem_s *psystem);
	void (*draw_trails) (struct psystem_s *psystem);
	void (*draw_transparent) (void);
	void (*post_process) (struct framebuffer_s *src);
	void (*set_2d) (int scaled);
	void (*end_frame) (void);

	struct framebuffer_s *(*create_cube_map) (int side);
	struct framebuffer_s *(*create_frame_buffer) (int width, int height);
	void (*destroy_frame_buffer) (struct framebuffer_s *framebuffer);
	void (*bind_framebuffer) (struct framebuffer_s *framebuffer);
	void (*set_viewport) (const struct vrect_s *view);
	// x and y are tan(f/2) for fov_x and fov_y
	void (*set_fov) (float x, float y);

	void (*capture_screen) (capfunc_t callback, void *data);

	void (*debug_ui) (struct imui_ctx_s *imui_ctx);

	vid_model_funcs_t *model_funcs;
} vid_render_funcs_t;

typedef struct vid_render_data_s {
	viddef_t   *vid;
	refdef_t   *refdef;
	int         scr_copytop;
	int         scr_copyeverything;
	int         scr_fullupdate;	// set to 0 to force full redraw
	void       (*viewsize_callback) (int view_size);
	int        *scr_viewsize;
	int        *graphheight;
	float       min_wateralpha;
	bool        force_fullscreen;
	bool        inhibit_viewmodel;
	bool        paused;
	int         lineadj;
	entity_t    view_model;	//FIXME still?!?
	double      frametime;
	double      realtime;
	lightstyle_t *lightstyle;
} vid_render_data_t;

#endif // __QF_plugin_vid_render_h
