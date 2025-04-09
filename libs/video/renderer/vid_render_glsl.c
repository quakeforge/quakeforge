/*
	vid_render_glsl.c

	GLSL version of the renderer

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "QF/image.h"

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "QF/GLSL/funcs.h"
#include "QF/GLSL/defines.h"
#include "QF/GLSL/qf_bsp.h"
#include "QF/GLSL/qf_draw.h"
#include "QF/GLSL/qf_fisheye.h"
#include "QF/GLSL/qf_main.h"
#include "QF/GLSL/qf_particles.h"
#include "QF/GLSL/qf_vid.h"
#include "QF/GLSL/qf_warp.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_gl.h"

gl_ctx_t *glsl_ctx;

static void
glsl_vid_render_choose_visual (void *data)
{
	glsl_ctx->choose_visual (glsl_ctx);
}

static void
glsl_vid_render_create_context (void *data)
{
	glsl_ctx->create_context (glsl_ctx, 1);
}

static vid_model_funcs_t model_funcs = {
	.texture_render_size = sizeof (glsltex_t),

	.Mod_LoadLighting   = glsl_Mod_LoadLighting,
	//.Mod_SubdivideSurface = 0,
	.Mod_ProcessTexture = glsl_Mod_ProcessTexture,

	.Mod_LoadIQM         = Mod_LoadIQM,
	.Mod_LoadAliasModel  = Mod_LoadAliasModel,
	.Mod_LoadSpriteModel = Mod_LoadSpriteModel,

	.Mod_MakeAliasModelDisplayLists = glsl_Mod_MakeAliasModelDisplayLists,
	.Mod_LoadAllSkins               = glsl_Mod_LoadAllSkins,
	.Mod_FinalizeAliasModel         = glsl_Mod_FinalizeAliasModel,
	.Mod_LoadExternalSkins          = glsl_Mod_LoadExternalSkins,
	.Mod_IQMFinish                  = glsl_Mod_IQMFinish,
	.alias_cache                    = 0,
	.Mod_SpriteLoadFrames           = glsl_Mod_SpriteLoadFrames,

	.skin_set                = Skin_Set,
	.skin_setupskin          = glsl_Skin_SetupSkin,
	.skin_destroy            = glsl_Skin_Destroy,
};

static void
glsl_vid_render_init (void)
{
	if (!vr_data.vid->vid_internal->gl_context) {
		Sys_Error ("Sorry, OpenGL (GLSL) not supported by this program.");
	}
	vid_internal_t *vi = vr_data.vid->vid_internal;
	glsl_ctx = vi->gl_context (vi);
	glsl_ctx->init_gl = GLSL_Init_Common;
	glsl_ctx->load_gl (glsl_ctx);

	vi->set_palette = GLSL_SetPalette;
	vi->choose_visual = glsl_vid_render_choose_visual;
	vi->create_context = glsl_vid_render_create_context;
	vr_funcs = &glsl_vid_render_funcs;
	m_funcs = &model_funcs;
}

static void
glsl_vid_render_shutdown (void)
{
	glsl_R_Shutdown ();
	GLSL_Shutdown_Common ();
}

static unsigned int GLErr_InvalidEnum;
static unsigned int GLErr_InvalidValue;
static unsigned int GLErr_InvalidOperation;
static unsigned int GLErr_OutOfMemory;
static unsigned int GLErr_Unknown;

static unsigned int
R_TestErrors (unsigned int numerous)
{
	switch (qfeglGetError ()) {
	case GL_NO_ERROR:
		return numerous;
		break;
	case GL_INVALID_ENUM:
		GLErr_InvalidEnum++;
		R_TestErrors (numerous++);
		break;
	case GL_INVALID_VALUE:
		GLErr_InvalidValue++;
		R_TestErrors (numerous++);
		break;
	case GL_INVALID_OPERATION:
		GLErr_InvalidOperation++;
		R_TestErrors (numerous++);
		break;
	case GL_OUT_OF_MEMORY:
		GLErr_OutOfMemory++;
		R_TestErrors (numerous++);
		break;
	default:
		GLErr_Unknown++;
		R_TestErrors (numerous++);
		break;
	}

	return numerous;
}

static void
R_DisplayErrors (void)
{
	if (GLErr_InvalidEnum)
		printf ("%d OpenGL errors: Invalid Enum!\n", GLErr_InvalidEnum);
	if (GLErr_InvalidValue)
		printf ("%d OpenGL errors: Invalid Value!\n", GLErr_InvalidValue);
	if (GLErr_InvalidOperation)
		printf ("%d OpenGL errors: Invalid Operation!\n", GLErr_InvalidOperation);
	if (GLErr_OutOfMemory)
		printf ("%d OpenGL errors: Out Of Memory!\n", GLErr_OutOfMemory);
	if (GLErr_Unknown)
		printf ("%d Unknown OpenGL errors!\n", GLErr_Unknown);
}

static void
R_ClearErrors (void)
{
	GLErr_InvalidEnum = 0;
	GLErr_InvalidValue = 0;
	GLErr_InvalidOperation = 0;
	GLErr_OutOfMemory = 0;
	GLErr_Unknown = 0;
}

static void
glsl_begin_frame (void)
{
	if (0) {
		if (R_TestErrors (0))
			R_DisplayErrors ();
		R_ClearErrors ();
	}

	if (glsl_ctx->begun) {
		glsl_ctx->begun = 0;
		glsl_ctx->end_rendering ();
	}

	//FIXME forces the status bar to redraw. needed because it does not fully
	//update in sw modes but must in glsl mode
	vr_data.scr_copyeverything = 1;

	qfeglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glsl_ctx->begun = 1;

	GLSL_Set2D ();
	GLSL_DrawReset ();

	if (r_refdef.vrect.x > 0) {
		int         rx = r_refdef.vrect.x + r_refdef.vrect.width;
		int         vh = vid.height - vr_data.lineadj;
		// left
		glsl_Draw_TileClear (0, 0, r_refdef.vrect.x, vh);
		// right
		glsl_Draw_TileClear (rx, 0, vid.width - rx, vh);
	}
	if (r_refdef.vrect.y > 0) {
		int         lx = r_refdef.vrect.x;
		int         ty = r_refdef.vrect.y;
		int         rx = r_refdef.vrect.x + r_refdef.vrect.width;
		int         by = r_refdef.vrect.y + r_refdef.vrect.height;
		int         vh = vid.height - vr_data.lineadj;
		// top
		glsl_Draw_TileClear (lx, 0, rx, ty);
		// bottom
		glsl_Draw_TileClear (lx, by, r_refdef.vrect.width, vh - by);
	}
}

static void
glsl_render_view (void)
{
	qfeglClear (GL_DEPTH_BUFFER_BIT);
	glsl_R_RenderView ();
	glsl_R_RenderEntities (r_ent_queue);
}

static void
glsl_draw_transparent (void)
{
	glsl_R_DrawWaterSurfaces ();
}

static void glsl_bind_framebuffer (framebuffer_t *fb);

static void
glsl_post_process (framebuffer_t *src)
{
	if (scr_fisheye) {
		glsl_FisheyeScreen (src);
	} else if (r_dowarp) {
		glsl_WarpScreen (src);
	}
}

static void
glsl_set_2d (int scaled)
{
	if (scaled) {
		GLSL_Set2DScaled ();
	} else {
		GLSL_Set2D ();
	}
}

static void
glsl_end_frame (void)
{
	GLSL_FlushText ();
	GLSL_End2D ();
	qfeglFlush ();
}

static framebuffer_t *
glsl_create_cube_map (int side)
{
	GLuint      tex[2];
	qfeglGenTextures (2, tex);

	qfeglBindTexture (GL_TEXTURE_CUBE_MAP, tex[0]);
	for (int i = 0; i < 6; i++) {
		qfeglTexImage2D (GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA,
						 side, side, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	}
	qfeglTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
						GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
						GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfeglTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qfeglBindTexture (GL_TEXTURE_2D, tex[1]);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, side, side, 0,
					 GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	size_t      size = sizeof (framebuffer_t) * 6;
	size += sizeof (gl_framebuffer_t) * 6;

	framebuffer_t *cube = malloc (size);
	__auto_type buffer_base = (gl_framebuffer_t *) &cube[6];
	for (int i = 0; i < 6; i++) {
		cube[i].width = side;
		cube[i].height = side;
		__auto_type buffer = buffer_base + i;
		cube[i].buffer = buffer;

		buffer->color = tex[0];
		buffer->depth = tex[1];
		qfeglGenFramebuffers (1, &buffer->handle);

		qfeglBindFramebuffer (GL_FRAMEBUFFER, buffer->handle);
		qfeglFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
								   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
								   buffer->color, 0);
		qfeglFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
								   GL_TEXTURE_2D, buffer->depth, 0);
	}
	qfeglBindFramebuffer (GL_FRAMEBUFFER, 0);
	return cube;
}

static framebuffer_t *
glsl_create_frame_buffer (int width, int height)
{
	size_t      size = sizeof (framebuffer_t) + sizeof (gl_framebuffer_t);

	framebuffer_t *fb = malloc (size);
	fb->width = width;
	fb->height = height;
	__auto_type buffer = (gl_framebuffer_t *) &fb[1];
	fb->buffer = buffer;
	qfeglGenFramebuffers (1, &buffer->handle);

	GLuint      tex[2];
	qfeglGenTextures (2, tex);

	buffer->color = tex[0];
	buffer->depth = tex[1];

	qfeglBindTexture (GL_TEXTURE_2D, buffer->color);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
					 GL_RGBA, GL_UNSIGNED_BYTE, 0);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qfeglBindTexture (GL_TEXTURE_2D, buffer->depth);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0,
					 GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qfeglBindFramebuffer (GL_FRAMEBUFFER, buffer->handle);
	qfeglFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
							   GL_TEXTURE_2D, buffer->color, 0);
	qfeglFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
							   GL_TEXTURE_2D, buffer->depth, 0);

	qfeglBindFramebuffer (GL_FRAMEBUFFER, 0);
	return fb;
}

static void
glsl_destroy_frame_buffer (framebuffer_t *framebuffer)
{
	__auto_type fb = (gl_framebuffer_t *) framebuffer->buffer;

	qfeglDeleteFramebuffers (1, &fb->handle);

	GLuint      tex[2] = { fb->color, fb->depth };
	qfeglDeleteTextures (2, tex);
	free (framebuffer);
}

static void
glsl_bind_framebuffer (framebuffer_t *framebuffer)
{
	unsigned    width = vr_data.vid->width;
	unsigned    height = vr_data.vid->height;
	if (!framebuffer) {
		qfeglBindFramebuffer (GL_FRAMEBUFFER, 0);
	} else {
		gl_framebuffer_t *buffer = framebuffer->buffer;
		qfeglBindFramebuffer (GL_FRAMEBUFFER, buffer->handle);

		width = framebuffer->width;
		height = framebuffer->height;
	}

	vrect_t r = { 0, 0, width, height };
	R_SetVrect (&r, &r_refdef.vrect, 0);
}

static void
glsl_set_viewport (const vrect_t *view)
{
	int         x = view->x;
	int         y = vid.height - (view->y + view->height);	//FIXME vid.height
	int         w = view->width;
	int         h = view->height;
	qfeglViewport (x, y, w, h);
}

static void
glsl_set_fov (float x, float y)
{
	float       neard, fard;
	mat4f_t     proj;

	neard = r_nearclip;
	fard = r_farclip;

	// NOTE columns!
	proj[0] = (vec4f_t) { 1/x, 0, 0, 0 };
	proj[1] = (vec4f_t) { 0, 1/y, 0, 0 };
	proj[2] = (vec4f_t) { 0, 0, (fard) / (fard - neard), 1 };
	proj[3] = (vec4f_t) { 0, 0, (fard * neard) / (neard - fard), 0 };

	// convert 0..1 depth buffer range to -1..1
	static mat4f_t depth_range = {
		{ 1, 0, 0, 0},
		{ 0, 1, 0, 0},
		{ 0, 0, 2, 0},
		{ 0, 0,-1, 1},
	};
	mmulf (glsl_ctx->projection, depth_range, proj);
}

static void
glsl_capture_screen (capfunc_t callback, void *data)
{
	int         count = vid.width * vid.height;
	tex_t      *tex = malloc (sizeof (tex_t) + count * 3);

	if (tex) {
		tex->data = (byte *) (tex + 1);
		tex->width = vid.width;
		tex->height = vid.height;
		tex->format = tex_rgb;
		tex->palette = 0;
		tex->flagbits = 0;
		tex->loaded = true;
		tex->flipped = true;
		qfeglReadPixels (0, 0, tex->width, tex->height, GL_RGB,
						 GL_UNSIGNED_BYTE, tex->data);
	}
	callback (tex, data);
}

vid_render_funcs_t glsl_vid_render_funcs = {
	.init = glsl_vid_render_init,

	.UpdateScreen = SCR_UpdateScreen_legacy,

	.Draw_CharBuffer        = glsl_Draw_CharBuffer,
	.Draw_SetScale          = glsl_Draw_SetScale,
	.Draw_Character         = glsl_Draw_Character,
	.Draw_String            = glsl_Draw_String,
	.Draw_nString           = glsl_Draw_nString,
	.Draw_AltString         = glsl_Draw_AltString,
	.Draw_ConsoleBackground = glsl_Draw_ConsoleBackground,
	.Draw_Crosshair         = glsl_Draw_Crosshair,
	.Draw_CrosshairAt       = glsl_Draw_CrosshairAt,
	.Draw_TileClear         = glsl_Draw_TileClear,
	.Draw_Fill              = glsl_Draw_Fill,
	.Draw_FillRGBA          = glsl_Draw_FillRGBA,
	.Draw_Line              = glsl_Draw_Line,
	.Draw_TextBox           = glsl_Draw_TextBox,
	.Draw_FadeScreen        = glsl_Draw_FadeScreen,
	.Draw_BlendScreen       = glsl_Draw_BlendScreen,
	.Draw_CachePic          = glsl_Draw_CachePic,
	.Draw_UncachePic        = glsl_Draw_UncachePic,
	.Draw_MakePic           = glsl_Draw_MakePic,
	.Draw_DestroyPic        = glsl_Draw_DestroyPic,
	.Draw_PicFromWad        = glsl_Draw_PicFromWad,
	.Draw_Pic               = glsl_Draw_Pic,
	.Draw_FitPic            = glsl_Draw_FitPic,
	.Draw_Picf              = glsl_Draw_Picf,
	.Draw_SubPic            = glsl_Draw_SubPic,
	.Draw_AddFont           = glsl_Draw_AddFont,
	.Draw_Glyph             = glsl_Draw_Glyph,
	.Draw_SetClip           = glsl_Draw_SetClip,
	.Draw_ResetClip         = glsl_Draw_ResetClip,

	.ParticleSystem   = glsl_ParticleSystem,
	.TrailSystem      = glsl_TrailSystem,
	.R_Init           = glsl_R_Init,
	.R_ClearState     = glsl_R_ClearState,
	.R_LoadSkys       = glsl_R_LoadSkys,
	.R_NewScene       = glsl_R_NewScene,
	.R_LineGraph      = glsl_LineGraph,
	.begin_frame      = glsl_begin_frame,
	.render_view      = glsl_render_view,
	.draw_particles   = glsl_R_DrawParticles,
	.draw_trails      = glsl_R_DrawTrails,
	.draw_transparent = glsl_draw_transparent,
	.post_process     = glsl_post_process,
	.set_2d           = glsl_set_2d,
	.end_frame        = glsl_end_frame,

	.create_cube_map      = glsl_create_cube_map,
	.create_frame_buffer  = glsl_create_frame_buffer,
	.destroy_frame_buffer = glsl_destroy_frame_buffer,
	.bind_framebuffer     = glsl_bind_framebuffer,
	.set_viewport         = glsl_set_viewport,
	.set_fov              = glsl_set_fov,

	.capture_screen = glsl_capture_screen,

	.model_funcs = &model_funcs
};

static general_funcs_t plugin_info_general_funcs = {
	.shutdown = glsl_vid_render_shutdown,
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.vid_render = &glsl_vid_render_funcs,
};

static plugin_data_t plugin_info_data = {
	.general = &plugin_info_general_data,
	.vid_render = &vid_render_data,
};

static plugin_t plugin_info = {
	qfp_vid_render,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"GLSL Renderer",
	"Copyright (C) 1996-1997  Id Software, Inc.\n"
	"Copyright (C) 1999-2012  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(vid_render, glsl)
{
	return &plugin_info;
}
