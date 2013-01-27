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

static GLuint cmap_tex[MAX_TRANSLATIONS];
static GLuint skin_tex[MAX_TRANSLATIONS];

void
glsl_Skin_ProcessTranslation (int cmap, const byte *translation)
{
	byte        top[4 * VID_GRADES * 16];
	byte        bottom[4 * VID_GRADES * 16];
	const byte *src;
	byte       *dst;
	int         i, j;

	src = translation + TOP_RANGE;
	for (i = 0, dst = top; i < VID_GRADES; i++, src += 256 - 16) {
		for (j = 0; j < 16; j++) {
			byte        c = *src++;
			const byte *in = vid.palette + c * 3;
			*dst++ = *in++;
			*dst++ = *in++;
			*dst++ = *in++;
			*dst++ = 255;	// alpha = 1
		}
	}
	src = translation + BOTTOM_RANGE;
	for (i = 0, dst = bottom; i < VID_GRADES; i++, src += 256 - 16) {
		for (j = 0; j < 16; j++) {
			byte        c = *src++;
			const byte *in = vid.palette + c * 3;
			*dst++ = *in++;
			*dst++ = *in++;
			*dst++ = *in++;
			*dst++ = 255;	// alpha = 1
		}
	}
	qfeglBindTexture (GL_TEXTURE_2D, cmap_tex[cmap - 1]);
	qfeglTexSubImage2D (GL_TEXTURE_2D, 0, TOP_RANGE, 0, 16, VID_GRADES,
					   GL_RGBA, GL_UNSIGNED_BYTE, top);
	qfeglTexSubImage2D (GL_TEXTURE_2D, 0, BOTTOM_RANGE, 0, 16, VID_GRADES,
					   GL_RGBA, GL_UNSIGNED_BYTE, bottom);
}

void
glsl_Skin_SetupSkin (skin_t *skin, int cmap)
{
	skin->texnum = 0;
	if (cmap) {
		if (skin->texels) {
			tex_t      *tex = skin->texels;

			skin->texnum = skin_tex[cmap - 1];
			qfeglBindTexture (GL_TEXTURE_2D, skin->texnum);
			qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE,
							tex->width, tex->height,
							0, GL_LUMINANCE, GL_UNSIGNED_BYTE, tex->data);
			qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
							   GL_CLAMP_TO_EDGE);
			qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
							   GL_CLAMP_TO_EDGE);
			qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
							   GL_NEAREST);
			qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
							   GL_NEAREST);
		}
		skin->auxtex = cmap_tex[cmap - 1];
	} else {
		skin->auxtex = 0;
	}
}

void
glsl_Skin_InitTranslations (void)
{
	byte        map[4 * VID_GRADES * 256];
	byte       *src, *dst;
	int         i;

	for (i = 0, dst = map, src = vid.colormap8; i < 256 * VID_GRADES; i++) {
		byte        c = *src++;
		const byte *in = vid.palette + c * 3;
		*dst++ = *in++;
		*dst++ = *in++;
		*dst++ = *in++;
		*dst++ = 255;	// alpha = 1
	}
	qfeglGenTextures (MAX_TRANSLATIONS, cmap_tex);
	qfeglGenTextures (MAX_TRANSLATIONS, skin_tex);
	for (i = 0; i < MAX_TRANSLATIONS; i++) {
		qfeglBindTexture (GL_TEXTURE_2D, cmap_tex[i]);
		qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 256, VID_GRADES, 0,
						GL_RGBA, GL_UNSIGNED_BYTE, map);
		qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
}
