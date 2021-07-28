/*
	glsl_model_brush.c

	Brush model mesh processing for GLSL

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
// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "qfalloca.h"

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_bsp.h"
#include "QF/GLSL/qf_textures.h"

#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"

static glsltex_t glsl_notexture = { };

static void
glsl_brush_clear (model_t *m, void *data)
{
	int         i;
	mod_brush_t *brush = &m->brush;

	m->needload = true;
	for (i = 0; i < brush->numtextures; i++) {
		// NOTE: some maps (eg e1m2) have empty texture slots
		glsltex_t  *tex = 0;
		if (brush->textures[i]) {
			tex = brush->textures[i]->render;
		}
		if (tex && tex->gl_texturenum) {
			GLSL_ReleaseTexture (tex->gl_texturenum);
			GLSL_ReleaseTexture (tex->sky_tex[0]);
			GLSL_ReleaseTexture (tex->sky_tex[1]);
			tex->gl_texturenum = 0;
		}
	}
	for (i = 0; i < brush->numsurfaces; i++) {
		if (brush->surfaces[i].polys) {
			free (brush->surfaces[i].polys);
			brush->surfaces[i].polys = 0;
		}
	}
}

static int
load_skytex (texture_t *tx, byte *data)
{
	int         tex;

	tex = GLSL_LoadQuakeTexture (tx->name, tx->height, tx->height, data);
	// GLSL_LoadQuakeTexture () leaves the texture bound
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	return tex;
}

void
glsl_Mod_ProcessTexture (model_t *mod, texture_t *tx)
{
	if (!tx) {
		r_notexture_mip->render = &glsl_notexture;
		return;
	}
	glsltex_t  *tex = tx->render;
	if (!strncmp (tx->name, "sky", 3)) {
		// sky textures need to be loaded as two separate textures to allow
		// wrapping on both sky layers.
		byte       *data;
		byte       *tx_data;
		int         tx_w, tx_h;
		int         i, j;

		tx_w = tx->width;
		tx_h = tx->height;
		tx_data = (byte *)tx + tx->offsets[0];

		if (tx_w == tx_h) {
			// a square sky texture probably means it's black, but just in
			// case some other magic is being done, duplicate the square to
			// both sky layers.
			tex->sky_tex[0] = load_skytex (tx, tx_data);
			tex->sky_tex[1] = tex->sky_tex[0];
		} else if (tx_w == 2 * tx_h) {
			data = alloca (tx_h * tx_h);
			for (i = 0; i < 2; i++) {
				for (j = 0; j < tx_h; j++)
					memcpy (&data[j * tx_h], &tx_data[j * tx_w + i * tx_h],
							tx_h);
				tex->sky_tex[i] = load_skytex (tx, data);
			}
			tex->gl_texturenum = 0;
		} else {
			Sys_Error ("Mod_ProcessTexture: invalid sky texture: %dx%d\n",
					   tx_w, tx_h);
		}
	} else {
		tex->gl_texturenum = GLSL_LoadQuakeMipTex (tx);
	}
}

void
glsl_Mod_LoadLighting (model_t *mod, bsp_t *bsp)
{
	// a bit hacky, but it's as good a place as any
	mod->clear = glsl_brush_clear;
	mod_lightmap_bytes = 1;
	if (!bsp->lightdatasize) {
		mod->brush.lightdata = NULL;
		return;
	}
	mod->brush.lightdata = Hunk_AllocName (0, bsp->lightdatasize, mod->name);
	memcpy (mod->brush.lightdata, bsp->lightdata, bsp->lightdatasize);
}
