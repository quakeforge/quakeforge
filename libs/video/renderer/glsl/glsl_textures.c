/*
	glsl_textures.c

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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/mathlib.h"
#include "QF/model.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_textures.h"

#include "QF/ui/vrect.h"

#include "r_scrap.h"

struct scrap_s {
	rscrap_t    rscrap;
	GLuint      tnum;
	int         format;
	int         bpp;
	byte       *data;	// local copy of the texture so updates can be batched
	vrect_t    *batch;
	subpic_t   *subpics;
	struct scrap_s *next;
};

static scrap_t *scrap_list;

static int max_tex_size;

int
GLSL_LoadQuakeTexture (const char *identifier, int width, int height,
					   const byte *data)
{
	GLuint      tnum;

	qfeglGenTextures (1, &tnum);
	qfeglBindTexture (GL_TEXTURE_2D, tnum);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE,
					 width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qfeglGenerateMipmap (GL_TEXTURE_2D);

	return tnum;
}

static void
GLSL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight,
						unsigned char *out, int outwidth, int outheight)
{
	// Improvements here should be mirrored in build_skin_8 in gl_skin.c
	unsigned char *inrow;
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

static void
GLSL_Mipmap8BitTexture (const byte *src, unsigned width, unsigned height,
					  byte *mip)
{
	unsigned    mw = width >> 1;
	unsigned    mh = height >> 1;
	unsigned    i, j;

	mw = max (mw, 1);
	mh = max (mh, 1);

	for (j = 0; j < mh; j++) {
		for (i = 0; i < mw; i++) {
			*mip++ = src [j * 2 * width + i * 2];
		}
	}
}

int
GLSL_LoadQuakeMipTex (const texture_t *tex)
{
	unsigned    swidth, sheight;
	GLuint      tnum;
	byte       *data = (byte *) tex;
	byte       *buffer = 0;
	byte       *scaled;
	int         lod;

	for (swidth = 1; swidth < tex->width; swidth <<= 1);
	for (sheight = 1; sheight < tex->height; sheight <<= 1);

	qfeglGenTextures (1, &tnum);
	qfeglBindTexture (GL_TEXTURE_2D, tnum);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					   GL_NEAREST_MIPMAP_NEAREST);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	if (swidth != tex->width || sheight != tex->height)
		buffer = malloc (swidth * sheight);

	// preshift so swidth and sheight are the correct sizes end the end of
	// the loop
	swidth <<= 1;
	sheight <<= 1;
	for (lod = 0; lod < MIPLEVELS; lod++) {
		// preshift so swidth and sheight are the correct sizes end the end of
		// the loop
		swidth >>= 1;
		sheight >>= 1;
		swidth = max (swidth, 1);
		sheight = max (sheight, 1);
		if (buffer) {
			unsigned    w = tex->width;
			unsigned    h = tex->height;

			w = max (w >> lod, 1);
			h = max (h >> lod, 1);
			GLSL_Resample8BitTexture (data + tex->offsets[lod], w, h,
									buffer, swidth, sheight);
			scaled = buffer;
		} else {
			scaled = data + tex->offsets[lod];
		}
		qfeglTexImage2D (GL_TEXTURE_2D, lod, GL_LUMINANCE, swidth, sheight,
						 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, scaled);
	}
	if (swidth > 1 || sheight > 1) {
		// mipmap will hold the reduced image, so this is more than enough
		byte       *mipmap = malloc (swidth * sheight);
		byte       *mip = mipmap;
		while (swidth > 1 || sheight > 1) {
			// scaled holds the source of the last lod uploaded
			GLSL_Mipmap8BitTexture (scaled, swidth, sheight, mip);
			swidth >>= 1;
			sheight >>= 1;
			swidth = max (swidth, 1);
			sheight = max (sheight, 1);
			qfeglTexImage2D (GL_TEXTURE_2D, lod, GL_LUMINANCE, swidth, sheight,
							 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, mip);
			scaled = mip;
			mip += swidth * sheight;
			lod++;
		}
		free (mipmap);
	}
	if (buffer)
		free (buffer);
	return tnum;
}

int
GLSL_LoadRGBTexture (const char *identifier, int width, int height,
					 const byte *data)
{
	GLuint      tnum;

	qfeglGenTextures (1, &tnum);
	qfeglBindTexture (GL_TEXTURE_2D, tnum);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_RGB,
					 width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qfeglGenerateMipmap (GL_TEXTURE_2D);

	return tnum;
}

int
GLSL_LoadRGBATexture (const char *identifier, int width, int height,
					  const byte *data)
{
	GLuint      tnum;

	qfeglGenTextures (1, &tnum);
	qfeglBindTexture (GL_TEXTURE_2D, tnum);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
					 width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qfeglGenerateMipmap (GL_TEXTURE_2D);

	return tnum;
}

void
GLSL_ReleaseTexture (int tex)
{
	GLuint      tnum = tex;
	qfeglDeleteTextures (1, &tnum);
}

static void
glsl_scraps_f (void)
{
	scrap_t    *scrap;
	int         area;
	int         size;

	if (!scrap_list) {
		Sys_Printf ("No scraps\n");
		return;
	}
	for (scrap = scrap_list; scrap; scrap = scrap->next) {
		area = R_ScrapArea (&scrap->rscrap);
		// always square
		size = scrap->rscrap.width;
		Sys_Printf ("tnum=%u size=%d format=%04x bpp=%d free=%d%%\n",
					scrap->tnum, size, scrap->format, scrap->bpp,
					area * 100 / (size * size));
		if (Cmd_Argc () > 1) {
			R_ScrapDump (&scrap->rscrap);
		}
	}
}

void
GLSL_TextureInit (void)
{
	qfeglGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_tex_size);
	Sys_MaskPrintf (SYS_glsl, "max texture size: %d\n", max_tex_size);

	Cmd_AddCommand ("glsl_scraps", glsl_scraps_f, "Dump GLSL scrap stats");
}

scrap_t *
GLSL_CreateScrap (int size, int format, int linear)
{
	int         i;
	int         bpp;
	scrap_t    *scrap;

	for (i = 0; i < 16; i++)
		if (size <= 1 << i)
			break;
	size = 1 << i;
	size = min (size, max_tex_size);
	switch (format) {
		case GL_ALPHA:
		case GL_LUMINANCE:
			bpp = 1;
			break;
		case GL_LUMINANCE_ALPHA:
			bpp = 2;
			break;
		case GL_RGB:
			bpp = 3;
			break;
		case GL_RGBA:
			bpp = 4;
			break;
		default:
			Sys_Error ("GL_CreateScrap: Invalid texture format");
	}
	scrap = malloc (sizeof (scrap_t));
	qfeglGenTextures (1, &scrap->tnum);
	R_ScrapInit (&scrap->rscrap, size, size);
	scrap->format = format;
	scrap->bpp = bpp;
	scrap->subpics = 0;
	scrap->next = scrap_list;
	scrap_list = scrap;

	scrap->data = calloc (1, size * size * bpp);
	scrap->batch = 0;

	qfeglBindTexture (GL_TEXTURE_2D, scrap->tnum);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, format,
					 size, size, 0, format, GL_UNSIGNED_BYTE, scrap->data);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if (linear) {
		qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	qfeglGenerateMipmap (GL_TEXTURE_2D);

	return scrap;
}

void
GLSL_ScrapClear (scrap_t *scrap)
{
	subpic_t   *sp;
	while (scrap->subpics) {
		sp = scrap->subpics;
		scrap->subpics = (subpic_t *) sp->next;
		free (sp);
	}
	R_ScrapClear (&scrap->rscrap);
}

void
GLSL_DestroyScrap (scrap_t *scrap)
{
	scrap_t   **s;

	for (s = &scrap_list; *s; s = &(*s)->next) {
		if (*s == scrap) {
			*s = scrap->next;
			break;
		}
	}
	GLSL_ScrapClear (scrap);
	R_ScrapDelete (&scrap->rscrap);
	GLSL_ReleaseTexture (scrap->tnum);
	free (scrap->data);
	free (scrap);
}

int
GLSL_ScrapTexture (scrap_t *scrap)
{
	return scrap->tnum;
}

subpic_t *
GLSL_ScrapSubpic (scrap_t *scrap, int width, int height)
{
	vrect_t    *rect;
	subpic_t   *subpic;

	rect = R_ScrapAlloc (&scrap->rscrap, width, height);
	if (!rect) {
		return 0;
	}

	subpic = malloc (sizeof (subpic_t));
	*((subpic_t **) &subpic->next) = scrap->subpics;
	scrap->subpics = subpic;
	*((scrap_t **) &subpic->scrap) = scrap;
	*((vrect_t **) &subpic->rect) = rect;
	*((int *) &subpic->width) = width;
	*((int *) &subpic->height) = height;
	*((float *) &subpic->size) = 1.0 / scrap->rscrap.width;
	return subpic;
}

void
GLSL_SubpicDelete (subpic_t *subpic)
{
	scrap_t    *scrap = (scrap_t *) subpic->scrap;
	vrect_t    *rect = (vrect_t *) subpic->rect;
	subpic_t  **sp;

	for (sp = &scrap->subpics; *sp; sp = (subpic_t **) &(*sp)->next)
		if (*sp == subpic)
			break;
	if (*sp != subpic)
		Sys_Error ("GLSL_ScrapDelSubpic: broken subpic");
	*sp = (subpic_t *) subpic->next;
	free (subpic);
	R_ScrapFree (&scrap->rscrap, rect);
}

void
GLSL_SubpicUpdate (subpic_t *subpic, byte *data, int batch)
{
	scrap_t    *scrap = (scrap_t *) subpic->scrap;
	vrect_t    *rect = (vrect_t *) subpic->rect;
	byte       *dest;
	int         step, sbytes;
	int         i;

	if (batch) {
		if (scrap->batch) {
			vrect_t    *r = scrap->batch;
			scrap->batch = VRect_Union (r, rect);
			VRect_Delete (r);
		} else {
			scrap->batch = VRect_New (rect->x, rect->y,
									  rect->width, rect->height);
		}
		step = scrap->rscrap.width * scrap->bpp;
		sbytes = subpic->width * scrap->bpp;
		dest = scrap->data + rect->y * step + rect->x * scrap->bpp;
		for (i = 0; i < subpic->height; i++, dest += step, data += sbytes)
			memcpy (dest, data, sbytes);
	} else {
		qfeglBindTexture (GL_TEXTURE_2D, scrap->tnum);
		qfeglTexSubImage2D (GL_TEXTURE_2D, 0, rect->x, rect->y,
						   subpic->width, subpic->height, scrap->format,
						   GL_UNSIGNED_BYTE, data);
	}
}

void
GLSL_ScrapFlush (scrap_t *scrap)
{
	vrect_t    *rect = scrap->batch;
	int         size = scrap->rscrap.width;

	if (!rect)
		return;
	//FIXME: it seems gl (as opposed to egl) allows row step to be specified.
	//should update to not update the entire horizontal block
	qfeglBindTexture (GL_TEXTURE_2D, scrap->tnum);
	qfeglTexSubImage2D (GL_TEXTURE_2D, 0, 0, rect->y,
					   size, rect->height, scrap->format,
					   GL_UNSIGNED_BYTE,
					   scrap->data + rect->y * size * scrap->bpp);
	VRect_Delete (rect);
	scrap->batch = 0;
}
