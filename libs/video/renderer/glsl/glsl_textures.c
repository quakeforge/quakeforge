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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/vrect.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_textures.h"

struct scrap_s {
	GLuint      tnum;
	int         size;	// in pixels, for now, always square, power of 2
	int         format;
	int         bpp;
	vrect_t    *free_rects;
	vrect_t    *rects;
	struct scrap_s *next;
};

static scrap_t *scrap_list;

static int max_tex_size;

int
GL_LoadQuakeTexture (const char *identifier, int width, int height, byte *data)
{
	GLuint      tnum;

	qfglGenTextures (1, &tnum);
	qfglBindTexture (GL_TEXTURE_2D, tnum);
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE,
					width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qfglGenerateMipmap (GL_TEXTURE_2D);

	return tnum;
}

int
GL_LoadRGBTexture (const char *identifier, int width, int height, byte *data)
{
	GLuint      tnum;

	qfglGenTextures (1, &tnum);
	qfglBindTexture (GL_TEXTURE_2D, tnum);
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_RGB,
					width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qfglGenerateMipmap (GL_TEXTURE_2D);

	return tnum;
}

void
GL_ReleaseTexture (int tex)
{
	GLuint      tnum = tex;
	qfglDeleteTextures (1, &tnum);
}

void
GL_TextureInit (void)
{
	qfglGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_tex_size);
	Sys_MaskPrintf (SYS_GLSL, "max texture size: %d\n", max_tex_size);
}

scrap_t *
GL_CreateScrap (int size, int format)
{
	int         i;
	int         bpp;
	scrap_t    *scrap;
	byte       *data;

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
	qfglGenTextures (1, &scrap->tnum);
	scrap->size = size;
	scrap->format = format;
	scrap->bpp = bpp;
	scrap->free_rects = VRect_New (0, 0, size, size);
	scrap->rects = 0;
	scrap->next = scrap_list;
	scrap_list = scrap;

	data = calloc (1, size * size * bpp);

	qfglBindTexture (GL_TEXTURE_2D, scrap->tnum);
	qfglTexImage2D (GL_TEXTURE_2D, 0, format,
					size, size, 0, format, GL_UNSIGNED_BYTE, data);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qfglGenerateMipmap (GL_TEXTURE_2D);
	free (data);

	return scrap;
}

void
GL_DestroyScrap (scrap_t *scrap)
{
	scrap_t   **s;
	vrect_t    *t;

	for (s = &scrap_list; *s; s = &(*s)->next) {
		if (*s == scrap) {
			*s = scrap->next;
			break;
		}
	}
	GL_ReleaseTexture (scrap->tnum);
	while (scrap->free_rects) {
		t = scrap->free_rects;
		scrap->free_rects = t->next;
		VRect_Delete (t);
	}
	while (scrap->rects) {
		t = scrap->rects;
		scrap->rects = t->next;
		VRect_Delete (t);
	}
	free (scrap);
}

subpic_t *
GL_ScrapSubpic (scrap_t *scrap, int width, int height)
{
	int         i, w, h;
	vrect_t   **t, **best;
	vrect_t    *old, *frags, *rect;
	subpic_t   *subpic;

	for (i = 0; i < 16; i++)
		if (width <= 1 << i)
			break;
	w = 1 << i;
	for (i = 0; i < 16; i++)
		if (height <= 1 << i)
			break;
	h = 1 << i;

	best = 0;
	for (t = &scrap->free_rects; *t; t = &(*t)->next) {
		if ((*t)->width < w || (*t)->height < h)
			continue;						// won't fit
		if (!best) {
			best = t;
			continue;
		}
		if ((*t)->width <= (*best)->width || (*t)->height <= (*best)->height)
			best = t;
	}
	if (!best)
		return 0;							// couldn't find a spot
	old = *best;
	*best = old->next;
	rect = VRect_New (old->x, old->y, width, height);
	frags = VRect_Difference (old, rect);
	VRect_Delete (old);
	if (frags) {
		// old was bigger than the requested size
		for (old = frags; old->next; old = old->next)
			;
		old->next = scrap->free_rects;
		scrap->free_rects = frags;
	}
	rect->next = scrap->rects;
	scrap->rects = rect;

	subpic = malloc (sizeof (subpic_t));
	*((scrap_t **) &subpic->scrap) = scrap;
	*((vrect_t **) &subpic->rect) = rect;
	*((int *) &subpic->tnum) = scrap->tnum;
	*((int *) &subpic->width) = width;
	*((int *) &subpic->height) = height;
	*((float *) &subpic->size) = scrap->size;
	return subpic;
}

void
GL_SubpicDelete (subpic_t *subpic)
{
	scrap_t    *scrap = (scrap_t *) subpic->scrap;
	vrect_t    *rect = (vrect_t *) subpic->rect;
	vrect_t    *old, *merge;
	vrect_t   **t;

	free (subpic);
	for (t = &scrap->rects; *t; t = &(*t)->next)
		if (*t == rect)
			break;
	if (*t != rect)
		Sys_Error ("GL_ScrapDelSubpic: broken subpic");
	*t = rect->next;

	do {
		for (t = &scrap->free_rects; *t; t = &(*t)->next) {
			merge = VRect_Merge (*t, rect);
			if (merge) {
				old = *t;
				*t = (*t)->next;
				VRect_Delete (old);
				VRect_Delete (rect);
				rect = merge;
				break;
			}
		}
	} while (merge);
	rect->next = scrap->free_rects;
	scrap->free_rects = rect;
}

void
GL_SubpicUpdate (subpic_t *subpic, byte *data)
{
	scrap_t    *scrap = (scrap_t *) subpic->scrap;
	vrect_t    *rect = (vrect_t *) subpic->rect;

	qfglBindTexture (GL_TEXTURE_2D, scrap->tnum);
	qfglTexSubImage2D (GL_TEXTURE_2D, 0, rect->x, rect->y,
					   subpic->width, subpic->height, scrap->format,
					   GL_UNSIGNED_BYTE, data);
}
