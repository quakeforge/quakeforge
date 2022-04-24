/*
	gl_textures.c

	Texture format setup.

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2001 Ragnvald Maartmann-Moe IV

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
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdio.h>

#include "QF/cmd.h"
#include "QF/crc.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"
#include "sbar.h"
#include "vid_internal.h"

typedef struct {
	GLuint      texnum;
	char        identifier[64];
	int         width, height;
	int         bytesperpixel;
//	int         texelformat;  // eventually replace bytesperpixel
	qboolean    mipmap;
	unsigned short crc;  // LordHavoc: CRC for texure validation
} gltexture_t;

static gltexture_t gltextures[MAX_GLTEXTURES];
static int  numgltextures = 0;

typedef struct {
	const char *name;
	int         minimize, maximize;
} glmode_t;

static glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
//	Anisotropic...
};

typedef struct {
	const char *name;
	int format;
} glformat_t;

#if 0
static glformat_t formats[] = {
/*
	1-4 are standardly supported formats by GL, not exactly the
	evil magic numbers they appear to be. Provided mostly as a
	way to avoid ugly code for supporting a shortcut, partly for
	consistency. --Despair
*/
	{"1", 1},
	{"2", 2},
	{"3", 3},
	{"4", 4},
	{"GL_ALPHA", GL_ALPHA},
	{"GL_ALPHA4", GL_ALPHA4},
	{"GL_ALPHA8", GL_ALPHA8},
	{"GL_ALPHA12", GL_ALPHA12},
	{"GL_ALPHA16", GL_ALPHA16},
/*
	{"GL_DEPTH_COMPONENT", GL_DEPTH_COMPONENT},
	{"GL_DEPTH_COMPONENT16", GL_DEPTH_COMPONENT16},
	{"GL_DEPTH_COMPONENT24", GL_DEPTH_COMPONENT24},
	{"GL_DEPTH_COMPONENT32", GL_DEPTH_COMPONENT32},
*/
	{"GL_LUMINANCE", GL_LUMINANCE},
	{"GL_LUMINANCE4", GL_LUMINANCE4},
	{"GL_LUMINANCE8", GL_LUMINANCE8},
	{"GL_LUMINANCE12", GL_LUMINANCE12},
	{"GL_LUMINANCE16", GL_LUMINANCE16},
	{"GL_LUMINANCE_ALPHA", GL_LUMINANCE_ALPHA},
	{"GL_LUMINANCE4_ALPHA4", GL_LUMINANCE4_ALPHA4},
	{"GL_LUMINANCE6_ALPHA2", GL_LUMINANCE6_ALPHA2},
	{"GL_LUMINANCE8_ALPHA8", GL_LUMINANCE8_ALPHA8},
	{"GL_LUMINANCE12_ALPHA4", GL_LUMINANCE12_ALPHA4},
	{"GL_LUMINANCE12_ALPHA12", GL_LUMINANCE12_ALPHA12},
	{"GL_LUMINANCE16_ALPHA16", GL_LUMINANCE16_ALPHA16},
	{"GL_INTENSITY", GL_INTENSITY},
	{"GL_INTENSITY4", GL_INTENSITY4},
	{"GL_INTENSITY8", GL_INTENSITY8},
	{"GL_INTENSITY12", GL_INTENSITY12},
	{"GL_INTENSITY16", GL_INTENSITY16},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
	{"GL_RGB", GL_RGB},
	{"GL_RGB4", GL_RGB4},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB10", GL_RGB10},
	{"GL_RGB12", GL_RGB12},
	{"GL_RGB16", GL_RGB16},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA2", GL_RGBA2},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB10_A2", GL_RGB10_A2},
	{"GL_RGBA12", GL_RGBA12},
	{"GL_RGBA16", GL_RGBA16},
/*	EXT_paletted_textures
	{"COLOR_INDEX1_EXT", COLOR_INDEX1_EXT}.
	{"COLOR_INDEX2_EXT", COLOR_INDEX2_EXT},
	{"COLOR_INDEX4_EXT", COLOR_INDEX4_EXT},
	{"COLOR_INDEX8_EXT", COLOR_INDEX8_EXT},
	{"COLOR_INDEX12_EXT", COLOR_INDEX12_EXT},
	{"COLOR_INDEX16_EXT", COLOR_INDEX16_EXT},
*/
/*	EXT_cmyka
	{"CMYK_EXT", CMYK_EXT},
	{"CMYKA_EXT", CMYKA_EXT},
*/
/*	EXT_422_pixels
	{"422_EXT", 422_EXT},
	{"422_REV_EXT", 422_REV_EXT},
	{"422_AVERAGE_EXT", 422_AVERAGE_EXT},
	{"422_REV_AVERAGE_EXT", 422_REV_AVERAGE_EXT},
*/
/*	EXT_abgr
	{"ABGR_EXT", ABGR_EXT},
*/
/*	EXT_bgra
	{"BGR_EXT", BGR_EXT},
	{"BGRA_EXT", BGRA_EXT},
*/
/* 1.4 texture_compression -- Append _ARB for older ARB version.
 * applicable only for CompressedTexImage and CompressedTexSubimage
 * which will complicate upload paths. *ponder*
	{"COMPRESSED_ALPHA", COMPRESSED_ALPHA},
	{"COMPRESSED_LUMINANCE", COMPRESSED_LUMINANCE},
	{"COMPRESSED_LUMINANCE_ALPHA", COMPRESSED_LUMINANCE_ALPHA},
	{"COMPRESSED_INTENSITY", COMPRESSED_INTENSITY},
	{"COMPRESSED_RGB", COMPRESSED_RGB_ARB},
	{"COMPRESSED_RGBA", COMPRESSED_RGBA},
*/
	{"NULL", 0}
};
#endif

int gl_alpha_format = 4, gl_lightmap_format = 4, gl_solid_format = 3;


void
GL_TextureMode_f (void)
{
	int          i;
	gltexture_t *glt;

	if (Cmd_Argc () == 1) {
		for (i = 0; i < 6; i++)
			if (gl_filter_min == modes[i].minimize) {
				Sys_Printf ("%s\n", modes[i].name);
				return;
			}
		Sys_Printf ("current filter is unknown?\n");
		return;
	}

	for (i = 0; i < 6; i++) {
		if (!strcasecmp (modes[i].name, Cmd_Argv (1)))
			break;
	}

	if (i == 6) {
		Sys_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
		if (glt->mipmap) {
			qfglBindTexture (GL_TEXTURE_2D, glt->texnum);
			qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
							   gl_filter_min);
			qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
							   gl_filter_max);
			if (gl_Anisotropy)
				qfglTexParameterf (GL_TEXTURE_2D,
								   GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_aniso);
		}
	}
}

#if 0
static int
GL_TextureDepth_f (int format)
{
	int i;

	for (i = 0; i < 42; i++) {
		if (format == formats[i].format) {
			Sys_Printf ("%s\n", formats[i].name);
			return GL_RGBA;
		}
		Sys_Printf ("Current texture format is unknown.\n");
		return GL_RGBA;
	}

	for (i = 0; formats[i].format != 0; i++) {
		if (!strcasecmp (formats[i].name, Cmd_Argv (1)))
			break;
	}

	if (formats[i].format == 0) {
		Sys_Printf ("bad texture format name\n");
		return GL_RGBA;
	}

	return formats[i].format;
}
#endif

static void
GL_ResampleTexture (unsigned int *in, int inwidth, int inheight,
					unsigned int *out, int outwidth, int outheight)
{
	// Improvements here should be mirrored in build_skin_32 in gl_skin.c
	int           i, j;
	unsigned int  frac, fracstep;
	unsigned int *inrow;

	if (!outwidth || !outheight)
		return;
	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++, out += outwidth) {
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j ++) {
			out[j] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}

static void
GL_Resample8BitTexture (const unsigned char *in, int inwidth, int inheight,
						unsigned char *out, int outwidth, int outheight)
{
	// Improvements here should be mirrored in build_skin_8 in gl_skin.c
	const unsigned char *inrow;
	int            i, j;
	unsigned int   frac, fracstep;

	if (!outwidth || !outheight)
		return;
	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++, out += outwidth) {
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j ++) {
			out[j] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}

/*
	GL_MipMap

	Operates in place, quartering the size of the texture.
*/
static void
GL_MipMap (byte *in, int width, int height)
{
	byte       *out;
	int         i, j;

	width <<= 2;
	height >>= 1;
	out = in;

	for (i = 0; i < height; i++, in += width) {
		for (j = 0; j < width; j += 8, out += 4, in += 8) {
			out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
		}
	}
}

/*
	GL_MipMap8Bit

	Mipping for 8 bit textures
*/
static void
GL_MipMap8Bit (byte *in, int width, int height)
{
	byte          *at1, *at2, *at3, *at4, *out;
	int            i, j;
	unsigned short r, g, b;

	height >>= 1;
	out = in;

	for (i = 0; i < height; i++, in += width)
		for (j = 0; j < width; j += 2, out += 1, in += 2) {
			at1 = (byte *) & d_8to24table[in[0]];
			at2 = (byte *) & d_8to24table[in[1]];
			at3 = (byte *) & d_8to24table[in[width + 0]];
			at4 = (byte *) & d_8to24table[in[width + 1]];

			r = (at1[0] + at2[0] + at3[0] + at4[0]);
			r >>= 5;
			g = (at1[1] + at2[1] + at3[1] + at4[1]);
			g >>= 5;
			b = (at1[2] + at2[2] + at3[2] + at4[2]);
			b >>= 5;

			out[0] = gl_15to8table[(r << 0) + (g << 5) + (b << 10)];
		}
}

static void
GL_Upload32 (unsigned int *data, int width, int height, qboolean mipmap,
			 qboolean alpha)
{
	int           scaled_width, scaled_height, intformat;
	unsigned int *scaled;

	if (!width || !height)
		return; // Null texture

	// Snap the height and width to a power of 2.
	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1);
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1);

	scaled_width >>= gl_picmip;
	scaled_height >>= gl_picmip;

	scaled_width = min (scaled_width, gl_max_size);
	scaled_height = min (scaled_height, gl_max_size);

	if (!(scaled = malloc (scaled_width * scaled_height * sizeof (GLuint))))
		Sys_Error ("GL_LoadTexture: too big");

	intformat = alpha ? gl_alpha_format : gl_solid_format;

	// If the real width/height and the 'scaled' width/height aren't
	// identical, then we rescale it.
	if (scaled_width == width && scaled_height == height) {
		memcpy (scaled, data, width * height * sizeof (GLuint));
	} else {
		GL_ResampleTexture (data, width, height, scaled, scaled_width,
							scaled_height);
	}

	qfglTexImage2D (GL_TEXTURE_2D, 0, intformat, scaled_width, scaled_height,
					0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);

	if (mipmap) {
		int         miplevel = 0;

		while (scaled_width > 1 || scaled_height > 1) {
			GL_MipMap ((byte *) scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			scaled_width = max (scaled_width, 1);
			scaled_height = max (scaled_height, 1);
			miplevel++;
			qfglTexImage2D (GL_TEXTURE_2D, miplevel, intformat, scaled_width,
							scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
							scaled);
		}
	}

	if (mipmap) {
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
						   gl_filter_min);
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
						   gl_filter_max);
	} else {
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
						   gl_filter_max);
		if (gl_picmip)
			qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
							   GL_NEAREST);
		else
			qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
							   gl_filter_max);
	}
	if (gl_Anisotropy)
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
						   gl_aniso);

	free (scaled);
}

/*
        GL_Upload8_EXT

        If we have shared or global palettes, upload an 8-bit texture.
        If we don't, this function does nothing.
*/
void
GL_Upload8_EXT (const byte *data, int width, int height, qboolean mipmap,
				qboolean alpha)
{
	byte       *scaled;
	int         scaled_width, scaled_height;

	// Snap the height and width to a power of 2.
	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1);
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1);

	scaled_width >>= gl_picmip;
	scaled_height >>= gl_picmip;

	scaled_width = min (scaled_width, gl_max_size);
	scaled_height = min (scaled_height, gl_max_size);

	if (!(scaled = malloc (scaled_width * scaled_height)))
		Sys_Error ("GL_LoadTexture: too big");

	// If the real width/height and the 'scaled' width/height aren't
	// identical then we rescale it.
	if (scaled_width == width && scaled_height == height) {
		memcpy (scaled, data, width * height);
	} else {
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width,
								scaled_height);
	}
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width,
					scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
					scaled);

	if (mipmap) {
		int         miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1) {
			GL_MipMap8Bit ((byte *) scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			scaled_width = max (scaled_width, 1);
			scaled_height = max (scaled_height, 1);
			miplevel++;
			qfglTexImage2D (GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT,
							scaled_width, scaled_height, 0, GL_COLOR_INDEX,
							GL_UNSIGNED_BYTE, scaled);
		}
	}

	if (mipmap) {
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
						   gl_filter_min);
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
						   gl_filter_max);
	} else {
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
						   gl_filter_max);
		if (gl_picmip)
			qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
							   GL_NEAREST);
		else
			qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
							   gl_filter_max);
	}
	if (gl_Anisotropy)
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
						   gl_aniso);

	free (scaled);
}

void
GL_Upload8 (const byte *data, int width, int height, qboolean mipmap,
			qboolean alpha)
{
	int           i, s, p;
	unsigned int *trans;

	s = width * height;
	trans = malloc (s * sizeof (unsigned int));
	SYS_CHECKMEM (trans);

	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha) {
		alpha = false;
		for (i = 0; i < s; i++) {
			p = data[i];
			if (p == 255)
				alpha = true;
			trans[i] = d_8to24table[p];
		}
	} else {
		unsigned int *out = trans;
		const byte   *in = data;
		for (i = 0; i < s; i++) {
			*out++ = d_8to24table[*in++];
		}
	}

	if (vid.is8bit && !alpha) {
		GL_Upload8_EXT (data, width, height, mipmap, alpha);
	} else {
		GL_Upload32 (trans, width, height, mipmap, alpha);
	}

	free (trans);
}

int
GL_LoadTexture (const char *identifier, int width, int height, const byte *data,
				qboolean mipmap, qboolean alpha, int bytesperpixel)
{
	int          crc, i;
	gltexture_t *glt;

	// LordHavoc: now just using a standard CRC for texture validation
	crc = CRC_Block (data, width * height * bytesperpixel);

	// see if the texture is already present
	if (identifier[0]) {
		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
			if (strequal (identifier, glt->identifier)) {
				if (crc != glt->crc
					|| width != glt->width
				    || height != glt->height
				    || bytesperpixel != glt->bytesperpixel)
					goto SetupTexture;
				else
					return gltextures[i].texnum;
			}
		}
	}

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("numgltextures == MAX_GLTEXTURES");

	glt = &gltextures[numgltextures];
	numgltextures++;

	strncpy (glt->identifier, identifier, sizeof (glt->identifier) - 1);
	glt->identifier[sizeof (glt->identifier) - 1] = '\0';

	qfglGenTextures (1, &glt->texnum);

SetupTexture:
	glt->crc = crc;
	glt->width = width;
	glt->height = height;
	glt->bytesperpixel = bytesperpixel;
	glt->mipmap = mipmap;

	qfglBindTexture (GL_TEXTURE_2D, glt->texnum);

	switch (glt->bytesperpixel) {
	case 1:
		GL_Upload8 (data, width, height, mipmap, alpha);
		break;
	case 3:
		{
			int          count = width * height;
			byte        *data32 = malloc (count * 4);
			byte        *d = data32;

			while (count--) {
				*d++ = *data++;
				*d++ = *data++;
				*d++ = *data++;
				*d++ = 255;			// alpha 1.0
			}
			GL_Upload32 ((GLuint *) data32, width, height, mipmap, 0);
			free (data32);
		}
		break;
	case 4:
		GL_Upload32 ((GLuint *) data, width, height, mipmap, alpha);
		break;
	default:
		Sys_Error ("SetupTexture: unknown bytesperpixel %i",
				   glt->bytesperpixel);
	}

	return glt->texnum;
}
