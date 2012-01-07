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
#include "QF/GLSL/qf_textures.h"

#include "compat.h"

void
Mod_ProcessTexture (texture_t *tx)
{
	tx->gl_texturenum = GL_LoadQuakeMipTex (tx);
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
