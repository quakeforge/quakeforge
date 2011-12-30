/*
	glsl_sprite.c

	Sprite drawing support for GLSL

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/30

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_local.h"

static const char quakesprite_vert[] =
#include "quakespr.vc"
;

static const char quakesprite_frag[] =
#include "quakespr.fc"
;

//static float proj_matrix[16];

static struct {
	int         program;
	shaderparam_t spritea;
	shaderparam_t spriteb;
	shaderparam_t palette;
	shaderparam_t matrix;
	shaderparam_t vertexa;
	shaderparam_t vertexb;
	shaderparam_t uvab;
	shaderparam_t colora;
	shaderparam_t colorb;
	shaderparam_t blend;
} quake_sprite = {
	0,
	{"spritea", 1},
	{"spriteb", 1},
	{"palette", 1},
	{"mvp_mat", 1},
	{"vertexa", 0},
	{"vertexb", 0},
	{"uvab", 0},
	{"vcolora", 0},
	{"vcolorb", 0},
	{"vblend", 0},
};
/*
static void
make_quad (qpic_t *pic, int x, int y, int w, int h,
		   int srcx, int srcy, int srcw, int srch, float verts[6][4])
{
	float       sl, sh, tl, th;

	sl = (float) srcx / (float) pic->width;
	sh = sl + (float) srcw / (float) pic->width;
	tl = (float) srcy / (float) pic->height;
	th = tl + (float) srch / (float) pic->height;

	verts[0][0] = x;
	verts[0][1] = y;
	verts[0][2] = sl;
	verts[0][3] = tl;

	verts[1][0] = x + w;
	verts[1][1] = y;
	verts[1][2] = sh;
	verts[1][3] = tl;

	verts[2][0] = x + w;
	verts[2][1] = y + h;
	verts[2][2] = sh;
	verts[2][3] = th;

	verts[3][0] = x;
	verts[3][1] = y;
	verts[3][2] = sl;
	verts[3][3] = tl;

	verts[4][0] = x + w;
	verts[4][1] = y + h;
	verts[4][2] = sh;
	verts[4][3] = th;

	verts[5][0] = x;
	verts[5][1] = y + h;
	verts[5][2] = sl;
	verts[5][3] = th;
}
*/
VISIBLE void
R_InitSprites (void)
{
	int         frag, vert;

	vert = GL_CompileShader ("quakespr.vert", quakesprite_vert,
							 GL_VERTEX_SHADER);
	frag = GL_CompileShader ("quakespr.frag", quakesprite_frag,
							 GL_FRAGMENT_SHADER);
	quake_sprite.program = GL_LinkProgram ("quakespr", vert, frag);
	GL_ResolveShaderParam (quake_sprite.program, &quake_sprite.spritea);
	GL_ResolveShaderParam (quake_sprite.program, &quake_sprite.spriteb);
	GL_ResolveShaderParam (quake_sprite.program, &quake_sprite.palette);
	GL_ResolveShaderParam (quake_sprite.program, &quake_sprite.matrix);
	GL_ResolveShaderParam (quake_sprite.program, &quake_sprite.vertexa);
	GL_ResolveShaderParam (quake_sprite.program, &quake_sprite.vertexb);
	GL_ResolveShaderParam (quake_sprite.program, &quake_sprite.colora);
	GL_ResolveShaderParam (quake_sprite.program, &quake_sprite.colorb);
	GL_ResolveShaderParam (quake_sprite.program, &quake_sprite.uvab);
	GL_ResolveShaderParam (quake_sprite.program, &quake_sprite.blend);
}

void
R_DrawSprite (void)
{
}

// All sprites are drawn in a batch, so avoid thrashing the gl state
void
R_SpriteBegin (void)
{
	qfglUseProgram (quake_sprite.program);
	qfglEnableVertexAttribArray (quake_sprite.vertexa.location);
	qfglEnableVertexAttribArray (quake_sprite.vertexb.location);
	qfglEnableVertexAttribArray (quake_sprite.uvab.location);
	qfglDisableVertexAttribArray (quake_sprite.colora.location);
	qfglDisableVertexAttribArray (quake_sprite.colorb.location);
	qfglDisableVertexAttribArray (quake_sprite.blend.location);

	qfglUniform1i (quake_sprite.spritea.location, 0);
	qfglActiveTexture (GL_TEXTURE0 + 0);
	qfglEnable (GL_TEXTURE_2D);

	qfglUniform1i (quake_sprite.spriteb.location, 1);
	qfglActiveTexture (GL_TEXTURE0 + 1);
	qfglEnable (GL_TEXTURE_2D);

	qfglUniform1i (quake_sprite.palette.location, 2);
	qfglActiveTexture (GL_TEXTURE0 + 2);
	qfglEnable (GL_TEXTURE_2D);
	qfglBindTexture (GL_TEXTURE_2D, glsl_palette);
}

void
R_SpriteEnd (void)
{
	qfglDisableVertexAttribArray (quake_sprite.vertexa.location);
	qfglDisableVertexAttribArray (quake_sprite.vertexb.location);
	qfglDisableVertexAttribArray (quake_sprite.uvab.location);

	qfglActiveTexture (GL_TEXTURE0 + 0);
	qfglEnable (GL_TEXTURE_2D);
	qfglActiveTexture (GL_TEXTURE0 + 1);
	qfglEnable (GL_TEXTURE_2D);
	qfglActiveTexture (GL_TEXTURE0 + 2);
	qfglEnable (GL_TEXTURE_2D);
}
