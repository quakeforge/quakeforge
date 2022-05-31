/*
	vid_render_gl.c

	GL version of the renderer

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

#include "QF/GL/funcs.h"
#include "QF/GL/qf_draw.h"
#include "QF/GL/qf_fisheye.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_particles.h"
#include "QF/GL/qf_vid.h"

#include "mod_internal.h"
#include "r_cvar.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_gl.h"

gl_ctx_t *gl_ctx;

/* Unknown renamed to GLErr_Unknown to solve conflict with winioctl.h */
static unsigned int GLErr_InvalidEnum;
static unsigned int GLErr_InvalidValue;
static unsigned int GLErr_InvalidOperation;
static unsigned int GLErr_OutOfMemory;
static unsigned int GLErr_StackOverflow;
static unsigned int GLErr_StackUnderflow;
static unsigned int GLErr_Unknown;

static unsigned int
R_TestErrors (unsigned int numerous)
{
	switch (qfglGetError ()) {
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
	case GL_STACK_OVERFLOW:
		GLErr_StackOverflow++;
		R_TestErrors (numerous++);
		break;
	case GL_STACK_UNDERFLOW:
		GLErr_StackUnderflow++;
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
	if (GLErr_StackOverflow)
		printf ("%d OpenGL errors: Stack Overflow!\n", GLErr_StackOverflow);
	if (GLErr_StackUnderflow)
		printf ("%d OpenGL errors: Stack Underflow\n!", GLErr_StackUnderflow);
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
	GLErr_StackOverflow = 0;
	GLErr_StackUnderflow = 0;
	GLErr_Unknown = 0;
}

void
gl_errors (const char *msg)
{
	if (R_TestErrors (0)) {
		printf ("gl_errors: %s\n", msg);
		R_DisplayErrors ();
	}
	R_ClearErrors ();
}

static void
gl_vid_render_choose_visual (void *data)
{
	gl_ctx->choose_visual (gl_ctx);
}

static void
gl_vid_render_create_context (void *data)
{
	gl_ctx->create_context (gl_ctx, 0);
}

static vid_model_funcs_t model_funcs = {
	sizeof (gltex_t),
	gl_Mod_LoadLighting,
	gl_Mod_SubdivideSurface,
	gl_Mod_ProcessTexture,

	Mod_LoadIQM,
	Mod_LoadAliasModel,
	Mod_LoadSpriteModel,

	gl_Mod_MakeAliasModelDisplayLists,
	gl_Mod_LoadAllSkins,
	gl_Mod_FinalizeAliasModel,
	gl_Mod_LoadExternalSkins,
	gl_Mod_IQMFinish,
	1,
	gl_Mod_SpriteLoadFrames,

	Skin_Free,
	Skin_SetColormap,
	Skin_SetSkin,
	gl_Skin_SetupSkin,
	Skin_SetTranslation,
	gl_Skin_ProcessTranslation,
	gl_Skin_InitTranslations,
};

static void
gl_vid_render_init (void)
{
	if (!vr_data.vid->vid_internal->sw_context) {
		Sys_Error ("Sorry, OpenGL not supported by this program.");
	}
	gl_ctx = vr_data.vid->vid_internal->gl_context ();
	gl_ctx->init_gl = GL_Init_Common;
	gl_ctx->load_gl ();

	vr_data.vid->vid_internal->data = gl_ctx;
	vr_data.vid->vid_internal->set_palette = GL_SetPalette;
	vr_data.vid->vid_internal->choose_visual = gl_vid_render_choose_visual;
	vr_data.vid->vid_internal->create_context = gl_vid_render_create_context;

	vr_funcs = &gl_vid_render_funcs;
	m_funcs = &model_funcs;
}

static void
gl_vid_render_shutdown (void)
{
}

static void
gl_begin_frame (void)
{
	if (gl_ctx->begun) {
		gl_ctx->end_rendering ();
		gl_ctx->begun = 0;
	}

	//FIXME forces the status bar to redraw. needed because it does not fully
	//update in sw modes but must in gl mode
	vr_data.scr_copyeverything = 1;

	if (gl_clear) {
		qfglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	} else {
		qfglClear (GL_DEPTH_BUFFER_BIT);
	}

	gl_ctx->begun = 1;

	if (r_speeds) {
		gl_ctx->start_time = Sys_DoubleTime ();
		gl_ctx->brush_polys = 0;
		gl_ctx->alias_polys = 0;
	}

	GL_Set2D ();
	GL_DrawReset ();

	// draw any areas not covered by the refresh
	if (r_refdef.vrect.x > 0) {
		// left
		gl_Draw_TileClear (0, 0, r_refdef.vrect.x,
						   vid.height - vr_data.lineadj);
		// right
		gl_Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0,
						   vid.width - r_refdef.vrect.x + r_refdef.vrect.width,
						   vid.height - vr_data.lineadj);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		gl_Draw_TileClear (r_refdef.vrect.x, 0,
						   r_refdef.vrect.x + r_refdef.vrect.width,
						   r_refdef.vrect.y);
		// bottom
		gl_Draw_TileClear (r_refdef.vrect.x,
						   r_refdef.vrect.y + r_refdef.vrect.height,
						   r_refdef.vrect.width,
						   vid.height - vr_data.lineadj -
						   (r_refdef.vrect.height + r_refdef.vrect.y));
	}

	gl_Fog_SetupFrame ();
}

static void
gl_render_view (void)
{
	// do 3D refresh drawing, and then update the screen
	qfglClear (GL_DEPTH_BUFFER_BIT);
	gl_R_RenderView ();
	gl_R_RenderEntities (r_ent_queue);
}

static void
gl_draw_transparent (void)
{
	gl_R_DrawWaterSurfaces ();
}

static void
gl_post_process (framebuffer_t *src)
{
	if (scr_fisheye) {
		gl_FisheyeScreen (src);
	} else if (r_dowarp) {
		gl_WarpScreen (src);
	}
}

static void
gl_set_2d (int scaled)
{
	if (scaled) {
		GL_Set2DScaled ();
	} else {
		GL_Set2D ();
	}
}

static void
gl_end_frame (void)
{
	if (r_speeds) {
//		qfglFinish ();
		double      start_time = gl_ctx->start_time;
		double      end_time = Sys_DoubleTime ();
		Sys_MaskPrintf (SYS_dev, "%3i ms  %4i wpoly %4i epoly %4i parts\n",
						(int) ((end_time - start_time) * 1000),
						gl_ctx->brush_polys, gl_ctx->alias_polys,
						r_psystem.numparticles);
	}

	GL_FlushText ();
	qfglFlush ();

	if (gl_finish) {
		gl_ctx->end_rendering ();
		gl_ctx->begun = 0;
	}
}

static framebuffer_t *
gl_create_cube_map (int side)
{
	GLuint      tex[2];
	qfglGenTextures (2, tex);

	qfglBindTexture (GL_TEXTURE_CUBE_MAP_ARB, tex[0]);
	for (int i = 0; i < 6; i++) {
		qfglTexImage2D (GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + i, 0, GL_RGBA,
						side, side, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	}
	qfglTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S,
					   GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T,
					   GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER,
					   GL_LINEAR);
	qfglTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER,
					   GL_LINEAR);

	qfglBindTexture (GL_TEXTURE_2D, tex[1]);
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, side, side, 0,
					GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
		qfglGenFramebuffers (1, &buffer->handle);

		qfglBindFramebuffer (GL_FRAMEBUFFER, buffer->handle);
		qfglFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
								  GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + i,
								  buffer->color, 0);
		qfglFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
								  GL_TEXTURE_2D, buffer->depth, 0);
	}
	qfglBindFramebuffer (GL_FRAMEBUFFER, 0);
	return cube;
}

static framebuffer_t *
gl_create_frame_buffer (int width, int height)
{
	size_t      size = sizeof (framebuffer_t) + sizeof (gl_framebuffer_t);

	framebuffer_t *fb = malloc (size);
	fb->width = width;
	fb->height = height;
	__auto_type buffer = (gl_framebuffer_t *) &fb[1];
	fb->buffer = buffer;
	qfglGenFramebuffers (1, &buffer->handle);

	GLuint      tex[2];
	qfglGenTextures (2, tex);

	buffer->color = tex[0];
	buffer->depth = tex[1];

	qfglBindTexture (GL_TEXTURE_2D, buffer->color);
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, 0);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qfglBindTexture (GL_TEXTURE_2D, buffer->depth);
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0,
					GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qfglBindFramebuffer (GL_FRAMEBUFFER, buffer->handle);
	qfglFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
							  GL_TEXTURE_2D, buffer->color, 0);
	qfglFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
							  GL_TEXTURE_2D, buffer->depth, 0);

	qfglBindFramebuffer (GL_FRAMEBUFFER, 0);
	return fb;
}

static void
gl_bind_framebuffer (framebuffer_t *framebuffer)
{
	unsigned    width = vr_data.vid->width;
	unsigned    height = vr_data.vid->height;
	if (!framebuffer) {
		qfglBindFramebuffer (GL_FRAMEBUFFER, 0);
	} else {
		gl_framebuffer_t *buffer = framebuffer->buffer;
		qfglBindFramebuffer (GL_FRAMEBUFFER, buffer->handle);

		width = framebuffer->width;
		height = framebuffer->height;
	}

	vrect_t r = { 0, 0, width, height };
	R_SetVrect (&r, &r_refdef.vrect, 0);
}

static void
gl_set_viewport (const vrect_t *view)
{
	int         x = view->x;
	int         y = vid.height - (view->y + view->height);	//FIXME vid.height
	int         w = view->width;
	int         h = view->height;
	qfglViewport (x, y, w, h);
}

static void
gl_set_fov (float x, float y)
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
	mmulf (gl_ctx->projection, depth_range, proj);
}

static void
gl_capture_screen (capfunc_t callback, void *data)
{
	int         count;
	tex_t      *tex;

	count = vid.width * vid.height;
	tex = malloc (sizeof (tex_t) + count * 3);
	if (tex) {
		tex->data = (byte *) (tex + 1);
		tex->width = vid.width;
		tex->height = vid.height;
		tex->format = tex_rgb;
		tex->palette = 0;
		tex->flagbits = 0;
		tex->loaded = 1;
		tex->bgr = 1;
		tex->flipped = 1;
		qfglReadPixels (0, 0, tex->width, tex->height, GL_BGR_EXT,
						GL_UNSIGNED_BYTE, tex->data);
	}
	callback (tex, data);
}

vid_render_funcs_t gl_vid_render_funcs = {
	gl_vid_render_init,
	gl_Draw_Character,
	gl_Draw_String,
	gl_Draw_nString,
	gl_Draw_AltString,
	gl_Draw_ConsoleBackground,
	gl_Draw_Crosshair,
	gl_Draw_CrosshairAt,
	gl_Draw_TileClear,
	gl_Draw_Fill,
	gl_Draw_Line,
	gl_Draw_TextBox,
	gl_Draw_FadeScreen,
	gl_Draw_BlendScreen,
	gl_Draw_CachePic,
	gl_Draw_UncachePic,
	gl_Draw_MakePic,
	gl_Draw_DestroyPic,
	gl_Draw_PicFromWad,
	gl_Draw_Pic,
	gl_Draw_Picf,
	gl_Draw_SubPic,

	gl_ParticleSystem,
	gl_R_Init,
	gl_R_ClearState,
	gl_R_LoadSkys,
	gl_R_NewScene,
	gl_R_LineGraph,
	gl_begin_frame,
	gl_render_view,
	gl_R_DrawParticles,
	gl_draw_transparent,
	gl_post_process,
	gl_set_2d,
	gl_end_frame,

	gl_create_cube_map,
	gl_create_frame_buffer,
	gl_bind_framebuffer,
	gl_set_viewport,
	gl_set_fov,

	gl_capture_screen,

	&model_funcs
};

static general_funcs_t plugin_info_general_funcs = {
	.shutdown = gl_vid_render_shutdown,
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.vid_render = &gl_vid_render_funcs,
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
	"GL Renderer",
	"Copyright (C) 1996-1997  Id Software, Inc.\n"
	"Copyright (C) 1999-2012  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(vid_render, gl)
{
	return &plugin_info;
}
