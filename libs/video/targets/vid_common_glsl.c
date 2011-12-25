/*
	vid_common_glsl.c

	Common OpenGLSL video driver functions

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2011      Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GL/defines.h"
#include "QF/GL/extensions.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "d_iface.h"
#include "r_cvar.h"
#include "sbar.h"

VISIBLE int					glsl_palette;
VISIBLE int					gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
VISIBLE int					gl_filter_max = GL_LINEAR;
VISIBLE float       		gldepthmin, gldepthmax;
VISIBLE qboolean			is8bit = false;

static void
GL_Common_Init_Cvars (void)
{
}

void
VID_SetPalette (unsigned char *palette)
{
	byte       *pal, *ip, *op;
	unsigned int r, g, b, v;
	unsigned short i;
	unsigned int *table;

	// 8 8 8 encoding
	Sys_MaskPrintf (SYS_VID, "Converting 8to24\n");

	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 255; i++) { // used to be i<256, see d_8to24table below
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

#ifdef WORDS_BIGENDIAN
		v = (255 << 0) + (r << 24) + (g << 16) + (b << 8);
#else
		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
#endif
		*table++ = v;
	}
	d_8to24table[255] = 0;	// 255 is transparent

	Sys_MaskPrintf (SYS_VID, "Converting palette to RGBA texture\n");
	pal = malloc (256 * 4);
	for (i = 0, ip = palette, op = pal; i < 255; i++) {
		*op++ = *ip++;
		*op++ = 255;	// alpha = 1
	}
	QuatZero (op);		// color 255 = transparent (alpha = 0)

	if (!glsl_palette) {
		GLuint      tex;
		qfglGenTextures (1, &tex);
		glsl_palette = tex;
	}
	qfglBindTexture (GL_TEXTURE_2D, glsl_palette);
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, pal);
	free (pal);
}

void
GL_Init_Common (void)
{
	GLF_FindFunctions ();

	GL_Common_Init_Cvars ();

	qfglClearColor (0, 0, 0, 0);

	qfglEnable (GL_TEXTURE_2D);
	qfglCullFace (GL_FRONT);
	qfglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qfglShadeModel (GL_FLAT);

	qfglEnable (GL_BLEND);
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void
VID_Init8bitPalette (void)
{
}

void
VID_LockBuffer (void)
{
}

void
VID_UnlockBuffer (void)
{
}

void
D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
}
