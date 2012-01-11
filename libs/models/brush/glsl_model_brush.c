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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_textures.h"

#include "compat.h"

void
Mod_ProcessTexture (texture_t *tx)
{
	if (!strncmp (tx->name, "sky", 3)) {
		// sky textures need to be loaded as two separate textures to allow
		// wrapping on both sky layers.
		byte       *data;
		byte       *tx_data;
		int         i, j;

		if (tx->width != 256 || tx->height != 128)
			Sys_Error ("Mod_ProcessTexture: invalid sky texture: %dx%d\n",
					   tx->width, tx->height);
		data = alloca (128 * 128);
		tx_data = (byte *)tx + tx->offsets[0];
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 128; j++)
				memcpy (&data[j * 128], &tx_data[j * 256 + i * 128], 128);
			tx->sky_tex[i] = GL_LoadQuakeTexture (tx->name, 128, 128, data);
			// GL_LoadQuakeTexture () leaves the texture bound
			qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
		tx->gl_texturenum = 0;
	} else {
		tx->gl_texturenum = GL_LoadQuakeMipTex (tx);
	}
}

void
Mod_LoadExternalTextures (model_t *mod)
{
}

void
Mod_LoadLighting (bsp_t *bsp)
{
	mod_lightmap_bytes = 1;
	if (!bsp->lightdatasize) {
		loadmodel->lightdata = NULL;
		return;
	}
	loadmodel->lightdata = Hunk_AllocName (bsp->lightdatasize, loadname);
	memcpy (loadmodel->lightdata, bsp->lightdata, bsp->lightdatasize);
}

void
Mod_SubdivideSurface (msurface_t *fa)
{
}
