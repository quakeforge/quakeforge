/*
	gl_rmisc.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/vid.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_draw.h"
#include "QF/GL/qf_fisheye.h"
#include "QF/GL/qf_lightmap.h"
#include "QF/GL/qf_particles.h"
#include "QF/GL/qf_rlight.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_sky.h"
#include "QF/GL/qf_sprite.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "varrays.h"
#include "vid_gl.h"

static gltex_t gl_notexture = { };

static void
gl_R_LoadSky_f (void)
{
	if (Cmd_Argc () != 2) {
		Sys_Printf ("loadsky <name> : load a skybox\n");
		return;
	}

	gl_R_LoadSkys (Cmd_Argv (1));
}

/*
  R_TimeRefresh_f

  For program optimization
  LordHavoc: improved appearance and accuracy of timerefresh
*/
static void
gl_R_TimeRefresh_f (void)
{
/*FIXME update for simd
	double      start, stop, time;
	int         i;

	gl_ctx->end_rendering ();

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		gl_R_RenderView ();
		gl_ctx->end_rendering ();
	}

	stop = Sys_DoubleTime ();
	time = stop - start;
	Sys_Printf ("%g seconds (%g fps)\n", time, 128 / time);
*/
}

void
gl_R_Init (void)
{
	r_notexture_mip->render = &gl_notexture;

	R_Init_Cvars ();

	Cmd_AddCommand ("timerefresh", gl_R_TimeRefresh_f,
					"Tests the current refresh rate for the current location");
	Cmd_AddCommand ("loadsky", gl_R_LoadSky_f, "Load a skybox");

	gl_Draw_Init ();
	glrmain_init ();
	gl_lightmap_init ();
	SCR_Init ();
	gl_R_InitBubble ();

	GDT_Init ();

	gl_R_InitGraphTextures ();
	gl_Skin_Init_Textures ();

	r_init = 1;
	gl_R_InitParticles ();
	gl_R_InitSprites ();
	Skin_Init ();
	gl_InitFisheye ();
}

static void
register_textures (mod_brush_t *brush)
{
	texture_t  *tex;

	for (unsigned i = 0; i < brush->numtextures; i++) {
		tex = brush->textures[i];
		if (!tex)
			continue;
		gl_R_AddTexture (tex);
	}
}

void
gl_R_NewScene (scene_t *scene)
{
	texture_t  *tex;
	mod_brush_t *brush;

	for (int i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	r_refdef.worldmodel = scene->worldmodel;
	brush = &scene->worldmodel->brush;

	// Force a vis update
	R_MarkLeaves (0, 0, 0, 0);

	R_ClearParticles ();

	GL_BuildLightmaps (scene->models, scene->num_models);

	// identify sky texture
	gl_R_ClearTextures ();
	for (unsigned i = 0; i < brush->numtextures; i++) {
		tex = brush->textures[i];
		if (!tex)
			continue;
		if (!strncmp (tex->name, "sky", 3)) {
			gl_R_InitSky (tex);
		}
	}

	gl_R_InitSurfaceChains (brush);
	gl_R_AddTexture (r_notexture_mip);
	register_textures (brush);
	for (int i = 0; i < scene->num_models; i++) {
		if (!scene->models[i])
			continue;
		if (*scene->models[i]->path == '*')
			continue;
		if (scene->models[i] != r_refdef.worldmodel
			&& scene->models[i]->type == mod_brush)
			register_textures (&scene->models[i]->brush);
	}
}
