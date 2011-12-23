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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"

#include "QF/GL/qf_textures.h"

#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_screen.h"

VISIBLE vec3_t r_origin, vpn, vright, vup;
VISIBLE refdef_t r_refdef;
qboolean r_cache_thrash;
int r_init;
int r_framecount;
int d_lightstylevalue[256];
int r_visframecount;
entity_t r_worldentity;

void
r_easter_eggs_f (cvar_t *var)
{
}

void
r_particles_style_f (cvar_t *var)
{
}

void
gl_overbright_f (cvar_t *var)
{
}

VISIBLE void
R_ViewChanged (float aspect)
{
}

VISIBLE void
R_RenderView (void)
{
}

VISIBLE void
R_Init (void)
{
}

VISIBLE void
R_Particles_Init_Cvars (void)
{
}

VISIBLE void
R_ClearParticles (void)
{
}

VISIBLE void
SCR_UpdateScreen (double realtime, SCR_Func *scr_funcs)
{
}

VISIBLE void
R_NewMap (model_t *worldmodel, struct model_s **models, int num_models)
{
}

VISIBLE void
R_LoadSkys (const char *sky)
{
}

VISIBLE void
Fog_Update (float density, float red, float green, float blue, float time)
{
}

VISIBLE void
Fog_ParseWorldspawn (struct plitem_s *worldspawn)
{
}

VISIBLE void
Skin_Set_Translate (int top, int bottom, void *_dest)
{
}

VISIBLE void
Skin_Do_Translation_Model (struct model_s *model, int skinnum, int slot,
						   skin_t *skin)
{
}

VISIBLE void
Skin_Process (skin_t *skin, struct tex_s * tex)
{
}

VISIBLE void
Skin_Init_Translation (void)
{
}

VISIBLE void
R_LineGraph (int x, int y, int *h_vals, int count)
{
}

VISIBLE void
R_InitParticles (void)
{
}

VISIBLE void
SCR_ScreenShot_f (void)
{
}

VISIBLE int
GL_LoadTexture (const char *identifier, int width, int height, byte *data,
				qboolean mipmap, qboolean alpha, int bytesperpixel)
{
	return 0;
}

VISIBLE void
R_ClearState (void)
{
	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearParticles ();
}
