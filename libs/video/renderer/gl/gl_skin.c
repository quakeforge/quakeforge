/*
	gl_skin.c

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_textures.h"

#include "compat.h"
#include "r_cvar.h"

static byte translate[256];
static unsigned int translate32[256];

static int player_width = 296;
static int player_height = 194;

VISIBLE void
Skin_Set_Translate (int top, int bottom, void *_dest)
{
	int         i;

	top = bound (0, top, 13) << 4;
	bottom = bound (0, bottom, 13) << 4;

	if (VID_Is8bit ()) {	// 8bit texture upload
		for (i = 0; i < 16; i++) {
			// the artists made some backwards ranges. sigh.
			if (top < 128)
				translate[TOP_RANGE + i] = top + i;
			else
				translate[TOP_RANGE + i] = top + 15 - i;

			if (bottom < 128)
				translate[BOTTOM_RANGE + i] = bottom + i;
			else
				translate[BOTTOM_RANGE + i] = bottom + 15 - i;
		}
	} else {
		if (top < 128)
			memcpy (translate32 + TOP_RANGE, d_8to24table + top,
					16 * sizeof (unsigned int));
		else
			for (i = 0; i < 16; i++)
				translate32[TOP_RANGE + i] = d_8to24table[top + 15 - i];

		if (bottom < 128)
			memcpy (translate32 + BOTTOM_RANGE, d_8to24table + bottom,
					16 * sizeof (unsigned int));
		else
			for (i = 0; i < 16; i++)
				translate32[BOTTOM_RANGE + i] = d_8to24table[bottom + 15 - i];
	}
}

static inline void
build_skin_8 (byte * original, int tinwidth, int tinheight,
			  unsigned int scaled_width, unsigned int scaled_height,
			  int inwidth, qboolean alpha)
{
	//  Improvements should be mirrored in GL_ResampleTexture in gl_textures.c
	byte        *inrow;
	byte         pixels[512 * 256], *out;
	unsigned int i, j;
	unsigned int frac, fracstep;

	out = pixels;
	memset (pixels, 0, sizeof (pixels));
	fracstep = tinwidth * 0x10000 / scaled_width;
	for (i = 0; i < scaled_height; i++, out += scaled_width) {
		inrow = original + inwidth * (i * tinheight / scaled_height);
		frac = fracstep >> 1;
		for (j = 0; j < scaled_width; j++) {
			out[j] = translate[inrow[frac >> 16]];
			frac += fracstep;
		}
	}

	GL_Upload8_EXT ((byte *) pixels, scaled_width, scaled_height, false,
					alpha);
}

static inline void
build_skin_32 (byte * original, int tinwidth, int tinheight,
			   unsigned int scaled_width, unsigned int scaled_height,
			   int inwidth, qboolean alpha)
{
	//  Improvements should be mirrored in GL_ResampleTexture in gl_textures.c
	byte        *inrow;
	unsigned int i, j;
	int          samples = alpha ? gl_alpha_format : gl_solid_format;
	unsigned int frac, fracstep;
	unsigned int pixels[512 * 256], *out;

	out = pixels;
	memset (pixels, 0, sizeof (pixels));
	fracstep = tinwidth * 0x10000 / scaled_width;
	for (i = 0; i < scaled_height; i++, out += scaled_width) {
		inrow = original + inwidth * (i * tinheight / scaled_height);
		frac = fracstep >> 1;
		for (j = 0; j < scaled_width; j++) {
			out[j] = translate32[inrow[frac >> 16]];
			frac += fracstep;
		}
	}

	qfglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (Anisotropy)
        qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
						   aniso);
}

static void
build_skin (int texnum, byte *ptexels, int width, int height,
			int owidth, int oheight, qboolean alpha)
{
	unsigned int scaled_width, scaled_height;

	// because this happens during gameplay, do it fast
	// instead of sending it through GL_Upload8()
	qfglBindTexture (GL_TEXTURE_2D, texnum);

	// FIXME deek: This 512x256 limit sucks!
	scaled_width = min (gl_max_size->int_val, 512);
	scaled_height = min (gl_max_size->int_val, 256);

	// allow users to crunch sizes down even more if they want
	scaled_width >>= gl_playermip->int_val;
	scaled_height >>= gl_playermip->int_val;

	if (VID_Is8bit ()) {				// 8bit texture upload
		build_skin_8 (ptexels, owidth, oheight, scaled_width, scaled_height,
					  width, alpha);
	} else {
		build_skin_32 (ptexels, owidth, oheight, scaled_width, scaled_height,
					   width, alpha);
	}
}

VISIBLE void
Skin_Do_Translation (skin_t *player_skin, int slot, skin_t *skin)
{
	byte       *original;
	int         inwidth, inheight;
	int         texnum = skin->texture;
	tex_t      *skin_texels;

	if ((skin_texels = (tex_t *) Skin_Cache (player_skin)) != NULL) {
		// skin data width
		inwidth = 320;
		inheight = 200;
		original = skin_texels->data;
	} else {
		original = player_8bit_texels;
		inwidth = player_width;
		inheight = player_height;
	}
	build_skin (texnum, original, inwidth, inheight,
				player_width, player_height, false);
}

VISIBLE void
Skin_Do_Translation_Model (model_t *model, int skinnum, int slot, skin_t *skin)
{
	byte       *original;
	int         inwidth, inheight;
	int         texnum = skin->texture;
	aliashdr_t *paliashdr;
	maliasskindesc_t *pskindesc;
	
	if (!model)							// player doesn't have a model yet
		return;
	if (model->type != mod_alias)		// only translate skins on alias models
		return;

	paliashdr = Cache_Get (&model->cache);
	if (skinnum < 0
		|| skinnum >= paliashdr->mdl.numskins) {
		Con_Printf ("(%d): Invalid player skin #%d\n", slot,
					skinnum);
		skinnum = 0;
	}
	pskindesc = ((maliasskindesc_t *)
				 ((byte *) paliashdr + paliashdr->skindesc)) + skinnum;
	//FIXME: broken for skin groups
	original = (byte *) paliashdr + pskindesc->skin;

	inwidth = paliashdr->mdl.skinwidth;
	inheight = paliashdr->mdl.skinheight;

	build_skin (texnum, original, inwidth, inheight, inwidth, inheight, false);

	Cache_Release (&model->cache);
}

VISIBLE void
Skin_Player_Model (model_t *model)
{
	aliashdr_t *paliashdr;
	
	player_width = 296;
	player_height = 194;
	if (!model)							// player doesn't have a model yet
		return;
	if (model->type != mod_alias)		// only translate skins on alias models
		return;

	paliashdr = Cache_Get (&model->cache);
	player_width = paliashdr->mdl.skinwidth;
	player_height = paliashdr->mdl.skinheight;
	Cache_Release (&model->cache);
}

VISIBLE void
Skin_Init_Translation (void)
{
	int         i;

	for (i = 0; i < 256; i++) {
		translate[i] = i;
		translate32[i] = d_8to24table[i];
	}
}

VISIBLE void
Skin_Process (skin_t *skin, tex_t *tex)
{
	int pixels = tex->width * tex->height;
	byte *ptexels = Hunk_TempAlloc (pixels);

	skin->fb_texture = 0;
	if (Mod_CalcFullbright (tex->data, ptexels, pixels)) {
		skin->fb_texture = skin_fb_textures + (skin - skin_cache);
		build_skin (skin->fb_texture, ptexels, tex->width, tex->height,
					296, 194, true);
	}
}
