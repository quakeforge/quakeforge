/*
	vid_render_sw.c

	SW version of the renderer

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

#include <string.h>

#include "QF/cvar.h"
#include "QF/image.h"

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "QF/ui/view.h"

#include "d_local.h"
#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

sw_ctx_t *sw_ctx;

static float r_aliasuvscale = 1.0;

static void
sw_vid_render_choose_visual (void *data)
{
    sw_ctx->choose_visual (sw_ctx);
}

static void
sw_vid_render_create_context (void *data)
{
    sw_ctx->create_context (sw_ctx);
}

static void
sw_vid_render_set_palette (void *data, const byte *palette)
{
    sw_ctx->set_palette (sw_ctx, palette);
}

static void
sw_vid_render_set_colormap (void *data, const byte *colormap)
{
	R_SetColormap (colormap);
}

static vid_model_funcs_t model_funcs = {
	0,
	sw_Mod_LoadLighting,
	0,//Mod_SubdivideSurface,
	0,//Mod_ProcessTexture,

	Mod_LoadIQM,
	Mod_LoadAliasModel,
	Mod_LoadSpriteModel,

	sw_Mod_MakeAliasModelDisplayLists,
	sw_Mod_LoadSkin,
	0,
	0,
	sw_Mod_IQMFinish,
	1,
	sw_Mod_SpriteLoadFrames,

	Skin_SetColormap,
	Skin_SetSkin,
	sw_Skin_SetupSkin,
	Skin_SetTranslation,
	sw_Skin_ProcessTranslation,
	sw_Skin_InitTranslations,
};

static void
sw_vid_render_init (void)
{
	if (!vr_data.vid->vid_internal->sw_context) {
		Sys_Error ("Sorry, software rendering not supported by this program.");
	}
	sw_ctx = vr_data.vid->vid_internal->sw_context ();

	vr_data.vid->vid_internal->data = sw_ctx;
	vr_data.vid->vid_internal->set_palette = sw_vid_render_set_palette;
	vr_data.vid->vid_internal->set_colormap = sw_vid_render_set_colormap;
	vr_data.vid->vid_internal->choose_visual = sw_vid_render_choose_visual;
	vr_data.vid->vid_internal->create_context = sw_vid_render_create_context;

	vr_funcs = &sw_vid_render_funcs;
	m_funcs = &model_funcs;
}

static void
sw_vid_render_shutdown (void)
{
}

static void sw_bind_framebuffer (framebuffer_t *framebuffer);

static void
sw_begin_frame (void)
{
	if (r_numsurfs) {
		int         surfcount = surface_p - surfaces;
		int         max_surfs = surf_max - surfaces;
		if (surfcount > r_maxsurfsseen)
			r_maxsurfsseen = surfcount;

		Sys_Printf ("Used %d of %d surfs; %d max\n",
					surfcount, max_surfs, r_maxsurfsseen);
	}

	if (r_numedges) {
		int         edgecount = edge_p - r_edges;

		if (edgecount > r_maxedgesseen)
			r_maxedgesseen = edgecount;

		Sys_Printf ("Used %d of %d edges; %d max\n", edgecount,
					r_numallocatededges, r_maxedgesseen);
	}

	sw_bind_framebuffer (0);

	// do 3D refresh drawing, and then update the screen
	if (vr_data.scr_fullupdate++ < vid.numpages) {
		vr_data.scr_copyeverything = 1;
		Draw_TileClear (0, 0, vid.width, vid.height);
	}
}

static void
sw_render_view (void)
{
	R_RenderView ();
}

static void
sw_draw_transparent (void)
{
}

static void
sw_post_process (framebuffer_t *src)
{
	if (scr_fisheye) {
		R_RenderFisheye (src);
	} else if (r_dowarp) {
		D_WarpScreen (src);
	}
}

static void
sw_set_2d (int scaled)
{
}

static void
sw_end_frame (void)
{
	if (r_reportsurfout && r_outofsurfaces)
		Sys_Printf ("Short %d surfaces\n", r_outofsurfaces);

	if (r_reportedgeout && r_outofedges)
		Sys_Printf ("Short roughly %d edges\n", r_outofedges * 2 / 3);

	// update one of three areas
	vrect_t     vrect;
	if (vr_data.scr_copyeverything) {
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;
		vrect.next = 0;
	} else if (scr_copytop) {
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height - vr_data.lineadj;
		vrect.next = 0;
	} else {
		vrect.x = vr_data.scr_view->xpos;
		vrect.y = vr_data.scr_view->ypos;
		vrect.width = vr_data.scr_view->xlen;
		vrect.height = vr_data.scr_view->ylen;
		vrect.next = 0;
	}
	sw_ctx->update (sw_ctx, &vrect);
}

static framebuffer_t *
sw_create_cube_map (int side)
{
	size_t      pixels = side * side;		// per face
	size_t      size = sizeof (framebuffer_t) * 6;
	size += sizeof (sw_framebuffer_t) * 6;
	size += pixels * 6;						// color buffer
	// depth buffer, scan table and zspantable are shared between cube faces
	// FIXME need *6 depth and zspan for multi-threaded
	size += pixels * sizeof (short);		// depth buffer

	framebuffer_t *cube = malloc (size);
	__auto_type buffer_base = (sw_framebuffer_t *) &cube[6];
	byte       *color_base = (byte *) &buffer_base[6];
	short      *depth_base = (short *) (color_base + 6 * pixels);
	for (int i = 0; i < 6; i++) {
		cube[i].width = side;
		cube[i].height = side;
		__auto_type buffer = buffer_base + i;
		cube[i].buffer = buffer;
		buffer->color = color_base + i * pixels;
		buffer->depth = depth_base;
		buffer->rowbytes = side;
	}
	return cube;
}

static framebuffer_t *
sw_create_frame_buffer (int width, int height)
{
	size_t      pixels = width * height;
	size_t      size = sizeof (framebuffer_t);
	size += sizeof (sw_framebuffer_t);
	size += pixels;							// color buffer
	size += pixels * sizeof (short);		// depth buffer

	framebuffer_t *fb = malloc (size);
	fb->width = width;
	fb->height = height;
	__auto_type buffer = (sw_framebuffer_t *) &fb[1];
	fb->buffer = buffer;
	buffer->color = (byte *) &buffer[1];
	buffer->depth = (short *) (buffer->color + pixels);
	buffer->rowbytes = width;
	return fb;
}

static void sw_set_viewport (const vrect_t *view);

static void
sw_bind_framebuffer (framebuffer_t *framebuffer)
{
	int         changed = 0;

	if (!framebuffer) {
		framebuffer = sw_ctx->framebuffer;
	}
	sw_framebuffer_t *fb = framebuffer->buffer;

	if (!fb->depth) {
		fb->depth = malloc (framebuffer->width * framebuffer->height
							* sizeof (short));
	}

	if (d_zbuffer != fb->depth
		|| d_zwidth != framebuffer->width || d_height != framebuffer->height) {
		d_zwidth = framebuffer->width;
		d_zrowbytes = d_zwidth * sizeof (short);
		for (unsigned i = 0; i < framebuffer->height; i++) {
			zspantable[i] = fb->depth + i * d_zwidth;
		}
		changed = 1;
	}
	if (d_rowbytes != fb->rowbytes || d_height != framebuffer->height) {
		d_rowbytes = fb->rowbytes;
		d_height = framebuffer->height;
		for (unsigned i = 0; i < framebuffer->height; i++) {
			d_scantable[i] = i * d_rowbytes;
		}
		changed = 1;
	}
	d_viewbuffer = fb->color;
	d_zbuffer = fb->depth;

	if (changed) {
		vrect_t r = { 0, 0, framebuffer->width, framebuffer->height };
		sw_set_viewport (&r);
	}
}

static void
sw_set_viewport (const vrect_t *view)
{
#define SHIFT20(x) (((x) << 20) + (1 << 19) - 1)
	r_refdef.vrectright             = view->x + view->width;
	r_refdef.vrectbottom            = view->y + view->height;
	r_refdef.vrectx_adj_shift20     = SHIFT20 (view->x);
	r_refdef.vrectright_adj_shift20 = SHIFT20 (r_refdef.vrectright);

	r_refdef.fvrectx      = (float) view->x;
	r_refdef.fvrecty      = (float) view->y;
	r_refdef.fvrectright  = (float) r_refdef.vrectright;
	r_refdef.fvrectbottom = (float) r_refdef.vrectbottom;

	r_refdef.fvrectx_adj      = (float) view->x - 0.5;
	r_refdef.fvrecty_adj      = (float) view->y - 0.5;
	r_refdef.fvrectright_adj  = (float) r_refdef.vrectright - 0.5;
	r_refdef.fvrectbottom_adj = (float) r_refdef.vrectbottom - 0.5;

	int         aleft = view->x * r_aliasuvscale;
	int         atop = view->y * r_aliasuvscale;
	int         awidth = view->width * r_aliasuvscale;
	int         aheight = view->height * r_aliasuvscale;
	r_refdef.aliasvrectleft = aleft;
	r_refdef.aliasvrecttop = atop;
	r_refdef.aliasvrectright = aleft + awidth;
	r_refdef.aliasvrectbottom = atop + aheight;

	// values for perspective projection
	// if math were exact, the values would range from 0.5 to to range+0.5
	// hopefully they wll be in the 0.000001 to range+.999999 and truncate
	// the polygon rasterization will never render in the first row or column
	// but will definately render in the [range] row and column, so adjust the
	// buffer origin to get an exact edge to edge fill
	xcenter = view->width * XCENTERING + view->x - 0.5;
	ycenter = view->height * YCENTERING + view->y - 0.5;
	aliasxcenter = xcenter * r_aliasuvscale;
	aliasycenter = ycenter * r_aliasuvscale;

	r_refdef.vrect.x = view->x;
	r_refdef.vrect.y = view->y;
	r_refdef.vrect.width = view->width;
	r_refdef.vrect.height = view->height;

	D_ViewChanged ();
}

static void
sw_set_fov (float x, float y)
{
	int         i;
	float       res_scale;

	r_viewchanged = true;

	// 320*200 1.0 pixelAspect = 1.6 aspect
	// 320*240 1.0 pixelAspect = 1.3333 aspect
	// proper 320*200 pixelAspect = 0.8333333
	pixelAspect = 1;//FIXME vid.aspect;

	float       hFOV = 2 * x;
	float       vFOV = 2 * y * pixelAspect;

	// general perspective scaling
	xscale = r_refdef.vrect.width / hFOV;
	yscale = xscale * pixelAspect;
	xscaleinv = 1.0 / xscale;
	yscaleinv = 1.0 / yscale;
	// perspective scaling for alias models
	aliasxscale = xscale * r_aliasuvscale;
	aliasyscale = yscale * r_aliasuvscale;
	// perspective scaling for paricle position
	xscaleshrink = (r_refdef.vrect.width - 6) / hFOV;
	yscaleshrink = xscaleshrink * pixelAspect;

	// left side clip
	screenedge[0].normal[0] = -1.0 / (XCENTERING * hFOV);
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[0].type = PLANE_ANYZ;

	// right side clip
	screenedge[1].normal[0] = 1.0 / ((1.0 - XCENTERING) * hFOV);
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[1].type = PLANE_ANYZ;

	// top side clip
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = -1.0 / (YCENTERING * vFOV);
	screenedge[2].normal[2] = 1;
	screenedge[2].type = PLANE_ANYZ;

	// bottom side clip
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = 1.0 / ((1.0 - YCENTERING) * vFOV);
	screenedge[3].normal[2] = 1;
	screenedge[3].type = PLANE_ANYZ;

	for (i = 0; i < 4; i++)
		VectorNormalize (screenedge[i].normal);

	res_scale = sqrt ((double) (r_refdef.vrect.width * r_refdef.vrect.height) /
					  (320.0 * 152.0)) * (2.0 / hFOV);
	r_aliastransition = r_aliastransbase * res_scale;
	r_resfudge = r_aliastransadj * res_scale;
}

static void
sw_capture_screen (capfunc_t callback, void *data)
{
	int         count, x, y;
	tex_t      *tex;
	const byte *src;
	byte       *dst;
	framebuffer_t *fb = sw_ctx->framebuffer;

	count = fb->width * fb->height;
	tex = malloc (sizeof (tex_t) + count * 3);
	if (tex) {
		tex->data = (byte *) (tex + 1);
		tex->width = fb->width;
		tex->height = fb->height;
		tex->format = tex_rgb;
		tex->palette = 0;
		tex->flagbits = 0;
		tex->loaded = 1;
		src = ((sw_framebuffer_t *) fb->buffer)->color;
		int rowbytes = ((sw_framebuffer_t *) fb->buffer)->rowbytes;
		for (y = 0; y < tex->height; y++) {
			dst = tex->data + y * tex->width * 3;
			for (x = 0; x < tex->width; x++) {
				byte        c = src[x];
				*dst++ = vid.basepal[c * 3 + 0];
				*dst++ = vid.basepal[c * 3 + 1];
				*dst++ = vid.basepal[c * 3 + 2];
			}
			src += rowbytes;
		}
	}
	callback (tex, data);
}

vid_render_funcs_t sw_vid_render_funcs = {
	sw_vid_render_init,
	Draw_Character,
	Draw_String,
	Draw_nString,
	Draw_AltString,
	Draw_ConsoleBackground,
	Draw_Crosshair,
	Draw_CrosshairAt,
	Draw_TileClear,
	Draw_Fill,
	Draw_TextBox,
	Draw_FadeScreen,
	Draw_BlendScreen,
	Draw_CachePic,
	Draw_UncachePic,
	Draw_MakePic,
	Draw_DestroyPic,
	Draw_PicFromWad,
	Draw_Pic,
	Draw_Picf,
	Draw_SubPic,

	sw_ParticleSystem,
	sw_R_Init,
	R_ClearState,
	R_LoadSkys,
	R_NewMap,
	R_LineGraph,
	sw_begin_frame,
	sw_render_view,
	R_DrawEntitiesOnList,
	R_DrawParticles,
	sw_draw_transparent,
	sw_post_process,
	sw_set_2d,
	sw_end_frame,

	sw_create_cube_map,
	sw_create_frame_buffer,
	sw_bind_framebuffer,
	sw_set_viewport,
	sw_set_fov,

	sw_capture_screen,

	&model_funcs
};

static general_funcs_t plugin_info_general_funcs = {
	.shutdown = sw_vid_render_shutdown,
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.vid_render = &sw_vid_render_funcs,
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
	"SW Renderer",
	"Copyright (C) 1996-1997  Id Software, Inc.\n"
	"Copyright (C) 1999-2012  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(vid_render, sw)
{
	return &plugin_info;
}
