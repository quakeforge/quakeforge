/*
	glsl_skin.c

	GLSL Skin support

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/23

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include <stdlib.h>

#include "QF/image.h"
#include "QF/model.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"

#include "mod_internal.h"
#include "r_internal.h"


static GLuint colormaps[256];

uint32_t
glsl_Skin_Colormap (const colormap_t *colormap)
{
	byte top = colormap->top & 0x0f;
	byte bot = colormap->bottom & 0x0f;
	int  ind = top | (bot << 4);
	if (colormaps[ind]) {
		return colormaps[ind];
	}
	qfeglGenTextures (1, &colormaps[ind]);
	byte cmap[4 * VID_GRADES * 256];
	byte *dst = cmap;
	byte *src = cmap + 3 * VID_GRADES * 256;
	Skin_SetColormap (src, top, bot);
	for (int i = 0; i < VID_GRADES * 256; i++) {
		byte        c = *src++;
		const byte *in = vid.palette + c * 3;
		*dst++ = *in++;
		*dst++ = *in++;
		*dst++ = *in++;
		*dst++ = 255;
	}

	qfeglBindTexture (GL_TEXTURE_2D, colormaps[ind]);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 256, VID_GRADES, 0,
					 GL_RGBA, GL_UNSIGNED_BYTE, cmap);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	return colormaps[ind];
}

void
glsl_Skin_SetupSkin (skin_t *skin, int cmap)
{
	tex_t      *tex = skin->tex;
	skin->tex = nullptr;	// tex memory is only temporarily allocated

	qfeglGenTextures (1, &skin->id);
	qfeglBindTexture (GL_TEXTURE_2D, skin->id);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE, tex->width, tex->height,
					 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, tex->data);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void
glsl_Skin_Destroy (skin_t *skin)
{
	qfeglDeleteTextures (1, &skin->id);
}
