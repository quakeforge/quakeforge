/*
	glsl_main.c

	GLSL rendering

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cvar.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_alias.h"
#include "QF/GLSL/qf_bsp.h"
#include "QF/GLSL/qf_lightmap.h"
#include "QF/GLSL/qf_textures.h"

#include "mod_internal.h"
#include "r_internal.h"

mat4_t glsl_projection;
mat4_t glsl_view;

void
glsl_R_ViewChanged (float aspect)
{
	double      xmin, xmax, ymin, ymax;
	float       fovx, fovy, neard, fard;
	vec_t      *proj = glsl_projection;

	fovx = r_refdef.fov_x;
	fovy = r_refdef.fov_y;
	neard = r_nearclip->value;
	fard = r_farclip->value;

	ymax = neard * tan (fovy * M_PI / 360);		// fov_2 / 2
	ymin = -ymax;
	xmax = neard * tan (fovx * M_PI / 360);		// fov_2 / 2
	xmin = -xmax;

	proj[0] = (2 * neard) / (xmax - xmin);
	proj[4] = 0;
	proj[8] = (xmax + xmin) / (xmax - xmin);
	proj[12] = 0;

	proj[1] = 0;
	proj[5] = (2 * neard) / (ymax - ymin);
	proj[9] = (ymax + ymin) / (ymax - ymin);
	proj[13] = 0;

	proj[2] = 0;
	proj[6] = 0;
	proj[10] = (fard + neard) / (neard - fard);
	proj[14] = (2 * fard * neard) / (neard - fard);

	proj[3] = 0;
	proj[7] = 0;
	proj[11] = -1;
	proj[15] = 0;
}

void
glsl_R_SetupFrame (void)
{
	R_AnimateLight ();
	R_ClearEnts ();
	r_framecount++;

	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	r_viewleaf = Mod_PointInLeaf (r_origin, r_worldentity.model);
}

static void
R_SetupView (void)
{
	float       x, y, w, h;
	mat4_t      mat;
	static mat4_t z_up = {
		 0, 0, -1, 0,
		-1, 0,  0, 0,
		 0, 1,  0, 0,
		 0, 0,  0, 1,
	};

	x = r_refdef.vrect.x;
	y = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height));
	w = r_refdef.vrect.width;
	h = r_refdef.vrect.height;
	qfeglViewport (x, y, w, h);

	Mat4Zero (mat);
	VectorCopy (vpn, mat + 0);
	VectorNegate (vright, mat + 4);			// we want vleft
	VectorCopy (vup, mat + 8);
	mat[15] = 1;
	Mat4Transpose (mat, mat);//AngleVectors gives the transpose of what we want
	Mat4Mult (z_up, mat, glsl_view);

	Mat4Identity (mat);
	VectorNegate (r_refdef.vieworg, mat + 12);
	Mat4Mult (glsl_view, mat, glsl_view);

	qfeglEnable (GL_CULL_FACE);
	qfeglEnable (GL_DEPTH_TEST);
}

static void
R_RenderEntities (void)
{
	entity_t   *ent;

	if (!r_drawentities->int_val)
		return;

	glsl_R_AliasBegin ();
	for (ent = r_ent_queue; ent; ent = ent->next) {
		if (ent->model->type != mod_alias)
			continue;
		currententity = ent;
		glsl_R_DrawAlias ();
	}
	glsl_R_AliasEnd ();

	glsl_R_SpriteBegin ();
	for (ent = r_ent_queue; ent; ent = ent->next) {
		if (ent->model->type != mod_sprite)
			continue;
		currententity = ent;
		glsl_R_DrawSprite ();
	}
	glsl_R_SpriteEnd ();
}

static void
R_DrawViewModel (void)
{
	currententity = vr_data.view_model;
	if (vr_data.inhibit_viewmodel
		|| !r_drawviewmodel->int_val
		|| !r_drawentities->int_val
		|| !currententity->model)
		return;

	// hack the depth range to prevent view model from poking into walls
	qfeglDepthRangef (0, 0.3);
	glsl_R_AliasBegin ();
	glsl_R_DrawAlias ();
	glsl_R_AliasEnd ();
	qfeglDepthRangef (0, 1);
}

void
glsl_R_RenderView (void)
{
	double      t[10] = {};
	int         speeds = r_speeds->int_val;

	if (speeds)
		t[0] = Sys_DoubleTime ();
	glsl_R_SetupFrame ();
	R_SetupView ();
	if (speeds)
		t[1] = Sys_DoubleTime ();
	R_MarkLeaves ();
	if (speeds)
		t[2] = Sys_DoubleTime ();
	R_PushDlights (vec3_origin);
	if (speeds)
		t[3] = Sys_DoubleTime ();
	glsl_R_DrawWorld ();
	if (speeds)
		t[4] = Sys_DoubleTime ();
	glsl_R_DrawSky ();
	if (speeds)
		t[5] = Sys_DoubleTime ();
	R_RenderEntities ();
	if (speeds)
		t[6] = Sys_DoubleTime ();
	glsl_R_DrawWaterSurfaces ();
	if (speeds)
		t[7] = Sys_DoubleTime ();
	glsl_R_DrawParticles ();
	if (speeds)
		t[8] = Sys_DoubleTime ();
	R_DrawViewModel ();
	if (speeds)
		t[9] = Sys_DoubleTime ();
	if (speeds) {
		Sys_Printf ("frame: %g, setup: %g, mark: %g, pushdl: %g, world: %g,"
					" sky: %g, ents: %g, water: %g, part: %g, view: %g\n",
					(t[9] - t[0]) * 1000, (t[1] - t[0]) * 1000,
					(t[2] - t[1]) * 1000, (t[3] - t[2]) * 1000,
					(t[4] - t[3]) * 1000, (t[5] - t[4]) * 1000,
					(t[6] - t[5]) * 1000, (t[7] - t[6]) * 1000,
					(t[8] - t[7]) * 1000, (t[9] - t[8]) * 1000);
	}
}

void
glsl_R_Init (void)
{
	R_Init_Cvars ();
	glsl_R_Particles_Init_Cvars ();
	Draw_Init ();
	SCR_Init ();
	glsl_R_InitBsp ();
	glsl_R_InitAlias ();
	glsl_R_InitSprites ();
	glsl_R_InitParticles ();
	glsl_Fog_Init ();
	Skin_Init ();
}

void
glsl_R_NewMap (model_t *worldmodel, struct model_s **models, int num_models)
{
	int         i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.model = worldmodel;

	// Force a vis update
	r_viewleaf = NULL;
	R_MarkLeaves ();

	R_FreeAllEntities ();
	glsl_R_ClearParticles ();
	glsl_R_RegisterTextures (models, num_models);
	glsl_R_BuildLightmaps (models, num_models);
	glsl_R_BuildDisplayLists (models, num_models);
}

void
glsl_R_LineGraph (int x, int y, int *h_vals, int count)
{
}

void
glsl_R_ClearState (void)
{
	R_ClearEfrags ();
	R_ClearDlights ();
	glsl_R_ClearParticles ();
}
