/*
	draw.c

	this is the only file outside the refresh that touches the vid buffer

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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/quakefs.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "d_iface.h"
#include "r_internal.h"
#include "vid_internal.h"

typedef struct {
	vrect_t     rect;
	int         width;
	int         height;
	byte       *ptexbytes;
	int         rowbytes;
} rectdesc_t;

static rectdesc_t r_rectdesc;

static qpic_t     *draw_disc;
static qpic_t     *draw_backtile;


/* Support Routines */

typedef struct cachepic_s {
	char        name[MAX_QPATH];
	cache_user_t cache;
} cachepic_t;

#define	MAX_CACHED_PICS		128
static cachepic_t  cachepics[MAX_CACHED_PICS];
static int         numcachepics;

#define CLIP(x,y,w,h,mw,mh)		\
	do {						\
		if (y < 0) {			\
			h += y;				\
			y = 0;				\
		}						\
		if (y + h > mh)			\
			h = mh - y;			\
		if (h <= 0)				\
			return;				\
		if (x < 0) {			\
			w += x;				\
			x = 0;				\
		}						\
		if (x + w > mw)			\
			w = mw - x;			\
		if (w <= 0)				\
			return;				\
	} while (0)


qpic_t *
sw32_Draw_MakePic (int width, int height, const byte *data)
{
	qpic_t	   *pic;
	int         size = width * height;

	pic = malloc (field_offset (qpic_t, data[size]));
	pic->width = width;
	pic->height = height;
	memcpy (pic->data, data, size);
	return pic;
}

void
sw32_Draw_DestroyPic (qpic_t *pic)
{
	free (pic);
}

qpic_t *
sw32_Draw_PicFromWad (const char *name)
{
	return W_GetLumpName (name);
}


qpic_t *
sw32_Draw_CachePic (const char *path, qboolean alpha)
{
	cachepic_t *pic;
	int         i;
	qpic_t     *dat;

	for (pic = cachepics, i = 0; i < numcachepics; pic++, i++)
		if (!strcmp (path, pic->name))
			break;

	if (i == numcachepics) {
		for (pic = cachepics, i = 0; i < numcachepics; pic++, i++)
			if (!pic->name[0])
				break;
		if (i == numcachepics) {
			if (numcachepics == MAX_CACHED_PICS)
				Sys_Error ("numcachepics == MAX_CACHED_PICS");
			numcachepics++;
		}
		strcpy (pic->name, path);
	}

	dat = Cache_Check (&pic->cache);

	if (dat)
		return dat;

	// load the pic from disk
	QFS_LoadCacheFile (QFS_FOpenFile (path), &pic->cache);

	dat = (qpic_t *) pic->cache.data;
	if (!dat) {
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	}

	SwapPic (dat);

	return dat;
}

void
sw32_Draw_UncachePic (const char *path)
{
	cachepic_t *pic;
	int         i;

	for (pic = cachepics, i = 0; i < numcachepics; pic++, i++) {
		if (!strcmp (path, pic->name)) {
			Cache_Release (&pic->cache);
			pic->name[0] = 0;
			break;
		}
	}
}


void
sw32_Draw_TextBox (int x, int y, int width, int lines, byte alpha)
{
	qpic_t     *p;
	int         cx, cy;
	int         n;

	// draw left side
	cx = x;
	cy = y;
	p = sw32_Draw_CachePic ("gfx/box_tl.lmp", true);
	sw32_Draw_Pic (cx, cy, p);
	p = sw32_Draw_CachePic ("gfx/box_ml.lmp", true);
	for (n = 0; n < lines; n++) {
		cy += 8;
		sw32_Draw_Pic (cx, cy, p);
	}
	p = sw32_Draw_CachePic ("gfx/box_bl.lmp", true);
	sw32_Draw_Pic (cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = sw32_Draw_CachePic ("gfx/box_tm.lmp", true);
		sw32_Draw_Pic (cx, cy, p);
		p = sw32_Draw_CachePic ("gfx/box_mm.lmp", true);
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = sw32_Draw_CachePic ("gfx/box_mm2.lmp", true);
			sw32_Draw_Pic (cx, cy, p);
		}
		p = sw32_Draw_CachePic ("gfx/box_bm.lmp", true);
		sw32_Draw_Pic (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = sw32_Draw_CachePic ("gfx/box_tr.lmp", true);
	sw32_Draw_Pic (cx, cy, p);
	p = sw32_Draw_CachePic ("gfx/box_mr.lmp", true);
	for (n = 0; n < lines; n++) {
		cy += 8;
		sw32_Draw_Pic (cx, cy, p);
	}
	p = sw32_Draw_CachePic ("gfx/box_br.lmp", true);
	sw32_Draw_Pic (cx, cy + 8, p);
}


void
sw32_Draw_Init (void)
{
	draw_chars = W_GetLumpName ("conchars");
	draw_disc = W_GetLumpName ("disc");
	draw_backtile = W_GetLumpName ("backtile");

	r_rectdesc.width = draw_backtile->width;
	r_rectdesc.height = draw_backtile->height;
	r_rectdesc.ptexbytes = draw_backtile->data;
	r_rectdesc.rowbytes = draw_backtile->width;
}


/*
	Draw_Character

	Draws one 8*8 graphics character with 0 being transparent.
	It can be clipped to the top of the screen to allow the console to be
	smoothly scrolled off.
*/
void
sw32_Draw_Character (int x, int y, unsigned int chr)
{
	byte       *source;
	int         drawline;
	int         row, col;

	chr &= 255;

	if (y <= -8)
		return;							// totally off screen

	if (y > vid.conheight - 8 || x < 0 || x > vid.conwidth - 8)
		return;
	if (chr > 255)
		return;

	row = chr >> 4;
	col = chr & 15;
	source = draw_chars + (row << 10) + (col << 3);

	if (y < 0) {						// clipped
		drawline = 8 + y;
		source -= 128 * y;
		y = 0;
	} else
		drawline = 8;


	switch(sw32_r_pixbytes) {
	case 1:
	{
		byte       *dest = (byte *) vid.conbuffer + y * vid.conrowbytes + x;

		while (drawline--) {
			if (source[0])
				dest[0] = source[0];
			if (source[1])
				dest[1] = source[1];
			if (source[2])
				dest[2] = source[2];
			if (source[3])
				dest[3] = source[3];
			if (source[4])
				dest[4] = source[4];
			if (source[5])
				dest[5] = source[5];
			if (source[6])
				dest[6] = source[6];
			if (source[7])
				dest[7] = source[7];
			source += 128;
			dest += vid.conrowbytes;
		}
	}
	break;
	case 2:
	{
		unsigned short *dest = (unsigned short *) vid.conbuffer + y *
			(vid.conrowbytes >> 1) + x;

		while (drawline--) {
			if (source[0])
				dest[0] = sw32_8to16table[source[0]];
			if (source[1])
				dest[1] = sw32_8to16table[source[1]];
			if (source[2])
				dest[2] = sw32_8to16table[source[2]];
			if (source[3])
				dest[3] = sw32_8to16table[source[3]];
			if (source[4])
				dest[4] = sw32_8to16table[source[4]];
			if (source[5])
				dest[5] = sw32_8to16table[source[5]];
			if (source[6])
				dest[6] = sw32_8to16table[source[6]];
			if (source[7])
				dest[7] = sw32_8to16table[source[7]];

			source += 128;
			dest += (vid.conrowbytes >> 1);
		}
	}
	break;
	case 4:
	{
		unsigned int *dest = (unsigned int *) vid.conbuffer + y *
			(vid.conrowbytes >> 2) + x;

		while (drawline--) {
			if (source[0])
				dest[0] = d_8to24table[source[0]];
			if (source[1])
				dest[1] = d_8to24table[source[1]];
			if (source[2])
				dest[2] = d_8to24table[source[2]];
			if (source[3])
				dest[3] = d_8to24table[source[3]];
			if (source[4])
				dest[4] = d_8to24table[source[4]];
			if (source[5])
				dest[5] = d_8to24table[source[5]];
			if (source[6])
				dest[6] = d_8to24table[source[6]];
			if (source[7])
				dest[7] = d_8to24table[source[7]];

			source += 128;
			dest += (vid.conrowbytes >> 2);
		}
	}
	break;
	default:
		Sys_Error("Draw_Character: unsupported r_pixbytes %i", sw32_r_pixbytes);
	}
}

void
sw32_Draw_String (int x, int y, const char *str)
{
	while (*str) {
		sw32_Draw_Character (x, y, *str++);
		x += 8;
	}
}

void
sw32_Draw_nString (int x, int y, const char *str, int count)
{
	while (count-- && *str) {
		sw32_Draw_Character (x, y, *str++);
		x += 8;
	}
}


void
sw32_Draw_AltString (int x, int y, const char *str)
{
	while (*str) {
		sw32_Draw_Character (x, y, (*str++) | 0x80);
		x += 8;
	}
}


static void
Draw_Pixel (int x, int y, byte color)
{
	switch(sw32_r_pixbytes)
	{
	case 1:
		((byte *) vid.conbuffer)[y * vid.conrowbytes + x] = color;
		break;
	case 2:
		((unsigned short *) vid.conbuffer)[y * (vid.conrowbytes >> 1) + x] =
			sw32_8to16table[color];
		break;
	case 4:
		((unsigned int *) vid.conbuffer)[y * (vid.conrowbytes >> 2) + x] =
			d_8to24table[color];
		break;
	default:
		Sys_Error("Draw_Pixel: unsupported r_pixbytes %i", sw32_r_pixbytes);
	}
}

static void
crosshair_1 (int x, int y)
{
	sw32_Draw_Character (x - 4, y - 4, '+');
}

static void
crosshair_2 (int x, int y)
{
	byte        c = crosshaircolor->int_val;

	Draw_Pixel (x - 1, y, c);
	Draw_Pixel (x - 3, y, c);
	Draw_Pixel (x + 1, y, c);
	Draw_Pixel (x + 3, y, c);
	Draw_Pixel (x, y - 1, c);
	Draw_Pixel (x, y - 3, c);
	Draw_Pixel (x, y + 1, c);
	Draw_Pixel (x, y + 3, c);
}

static void
crosshair_3 (int x, int y)
{
	byte        c = crosshaircolor->int_val;

	Draw_Pixel (x - 3, y - 3, c);
	Draw_Pixel (x + 3, y - 3, c);
	Draw_Pixel (x - 2, y - 2, c);
	Draw_Pixel (x + 2, y - 2, c);
	Draw_Pixel (x - 3, y + 3, c);
	Draw_Pixel (x + 2, y + 2, c);
	Draw_Pixel (x - 2, y + 2, c);
	Draw_Pixel (x + 3, y + 3, c);
}

static void
crosshair_4 (int x, int y)
{
	//byte        c = crosshaircolor->int_val;

	Draw_Pixel (x,     y - 2, 8);
	Draw_Pixel (x + 1, y - 2, 9);

	Draw_Pixel (x,     y - 1, 6);
	Draw_Pixel (x + 1, y - 1, 8);
	Draw_Pixel (x + 2, y - 1, 2);

	Draw_Pixel (x - 2, y,     6);
	Draw_Pixel (x - 1, y,     8);
	Draw_Pixel (x,     y,     8);
	Draw_Pixel (x + 1, y,     6);
	Draw_Pixel (x + 2, y,     8);
	Draw_Pixel (x + 3, y,     8);

	Draw_Pixel (x - 1, y + 1, 2);
	Draw_Pixel (x,     y + 1, 8);
	Draw_Pixel (x + 1, y + 1, 8);
	Draw_Pixel (x + 2, y + 1, 2);
	Draw_Pixel (x + 3, y + 1, 2);
	Draw_Pixel (x + 4, y + 1, 2);

	Draw_Pixel (x,     y + 2, 7);
	Draw_Pixel (x + 1, y + 2, 8);
	Draw_Pixel (x + 2, y + 2, 2);

	Draw_Pixel (x + 1, y + 3, 2);
	Draw_Pixel (x + 2, y + 3, 2);
}

static void
crosshair_5 (int x, int y)
{
	byte        c = crosshaircolor->int_val;

	Draw_Pixel (x - 1, y - 3, c);
	Draw_Pixel (x + 0, y - 3, c);
	Draw_Pixel (x + 1, y - 3, c);

	Draw_Pixel (x - 2, y - 2, c);
	Draw_Pixel (x + 2, y - 2, c);

	Draw_Pixel (x - 3, y - 1, c);
	Draw_Pixel (x + 3, y - 1, c);

	Draw_Pixel (x - 3, y, c);
	Draw_Pixel (x, y, c);
	Draw_Pixel (x + 3, y, c);

	Draw_Pixel (x - 3, y + 1, c);
	Draw_Pixel (x + 3, y + 1, c);

	Draw_Pixel (x - 2, y + 2, c);
	Draw_Pixel (x + 2, y + 2, c);

	Draw_Pixel (x - 1, y + 3, c);
	Draw_Pixel (x + 0, y + 3, c);
	Draw_Pixel (x + 1, y + 3, c);
}

static void (*crosshair_func[]) (int x, int y) = {
	crosshair_1,
	crosshair_2,
	crosshair_3,
	crosshair_4,
	crosshair_5,
};

void
sw32_Draw_Crosshair (void)
{
	int            x, y;
	int            ch;

	ch = crosshair->int_val - 1;
	if ((unsigned) ch >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	x = vid.conwidth / 2 + cl_crossx->int_val;
	y = vid.conheight / 2 + cl_crossy->int_val;

	crosshair_func[ch] (x, y);
}

void
sw32_Draw_CrosshairAt (int ch, int x, int y)
{
	ch -= 1;
	if ((unsigned) ch >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	crosshair_func[ch] (x, y);
}

void
sw32_Draw_Pic (int x, int y, qpic_t *pic)
{
	byte       *source, tbyte;
	int         v, u;

	if (x < 0 || (x + pic->width) > vid.conwidth
		|| y < 0 || (y + pic->height) > vid.conheight) {
		Sys_MaskPrintf (SYS_vid, "Draw_Pic: bad coordinates");
		sw32_Draw_SubPic (x, y, pic, 0, 0, pic->width, pic->height);
		return;
	}

	source = pic->data;

	switch(sw32_r_pixbytes) {
	case 1:
	{
		byte       *dest = (byte *) vid.buffer + y * vid.rowbytes + x;

		if (pic->width & 7) {			// general
			for (v = 0; v < pic->height; v++) {
				for (u = 0; u < pic->width; u++)
					if ((tbyte = source[u]) != TRANSPARENT_COLOR)
						dest[u] = tbyte;

				dest += vid.rowbytes;
				source += pic->width;
			}
		} else {						// unwound
			for (v = 0; v < pic->height; v++) {
				for (u = 0; u < pic->width; u += 8) {
					if ((tbyte = source[u]) != TRANSPARENT_COLOR)
						dest[u] = tbyte;
					if ((tbyte = source[u + 1]) != TRANSPARENT_COLOR)
						dest[u + 1] = tbyte;
					if ((tbyte = source[u + 2]) != TRANSPARENT_COLOR)
						dest[u + 2] = tbyte;
					if ((tbyte = source[u + 3]) != TRANSPARENT_COLOR)
						dest[u + 3] = tbyte;
					if ((tbyte = source[u + 4]) != TRANSPARENT_COLOR)
						dest[u + 4] = tbyte;
					if ((tbyte = source[u + 5]) != TRANSPARENT_COLOR)
						dest[u + 5] = tbyte;
					if ((tbyte = source[u + 6]) != TRANSPARENT_COLOR)
						dest[u + 6] = tbyte;
					if ((tbyte = source[u + 7]) != TRANSPARENT_COLOR)
						dest[u + 7] = tbyte;
				}
				dest += vid.rowbytes;
				source += pic->width;
			}
		}
	}
	break;
	case 2:
	{
		unsigned short *dest = (unsigned short *) vid.buffer + y *
			(vid.rowbytes >> 1) + x;

		for (v = 0; v < pic->height; v++) {
			for (u = 0; u < pic->width; u++) {
				tbyte = source[u];
				if (tbyte != TRANSPARENT_COLOR)
					dest[u] = sw32_8to16table[tbyte];
			}

			dest += vid.rowbytes >> 1;
			source += pic->width;
		}
	}
	break;
	case 4:
	{
		unsigned int *dest = (unsigned int *) vid.buffer + y *
			(vid.rowbytes >> 2) + x;
		for (v = 0; v < pic->height; v++) {
			for (u = 0; u < pic->width; u++) {
				tbyte = source[u];
				if (tbyte != TRANSPARENT_COLOR)
					dest[u] = d_8to24table[tbyte];
			}
			dest += vid.rowbytes >> 2;
			source += pic->width;
		}
	}
	break;
	default:
		Sys_Error("Draw_Pic: unsupported r_pixbytes %i", sw32_r_pixbytes);
	}
}

void
sw32_Draw_Picf (float x, float y, qpic_t *pic)
{
	sw32_Draw_Pic (x, y, pic);
}

void
sw32_Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
				  int height)
{
	byte       *source, tbyte;
	int   v, u;

	if ((x < 0) || (x + width > vid.conwidth)
		|| (y < 0) || (y + height > vid.conheight)) {
		Sys_MaskPrintf (SYS_vid, "Draw_SubPic: bad coordinates");
	}
	// first, clip to screen
	if (x < 0) {
		srcx += x;
		width += x;
		x = 0;
	}
	if ((unsigned) (x + width) > vid.width)
		width = vid.width - x;
	if (width <= 0)
		return;
	if (y < 0) {
		srcy += y;
		height += y;
		y = 0;
	}
	if ((unsigned) (y + height) > vid.height)
		height = vid.height - y;
	if (height <= 0)
		return;
	// next, clip to pic
	CLIP (srcx, srcy, width, height, pic->width, pic->height);

	source = pic->data + srcy * pic->width + srcx;

	switch (sw32_r_pixbytes) {
	case 1:
	{
		byte       *dest = (byte *) vid.buffer + y * vid.rowbytes + x;

		for (v = 0; v < height; v++) {
			for (u = 0; u < width; u++)
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = tbyte;
			dest += vid.rowbytes;
			source += pic->width;
		}
	}
	break;
	case 2:
	{
		unsigned short *dest = (unsigned short *) vid.buffer + y *
			(vid.rowbytes >> 1) + x;
		for (v = 0; v < height; v++, dest += vid.rowbytes >> 1,
				 source += pic->width)
			for (u = 0; u < width; u++)
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = sw32_8to16table[tbyte];
	}
	break;
	case 4:
	{
		unsigned int *dest = (unsigned int *) vid.buffer + y *
			(vid.rowbytes >> 2) + x;
		for (v = 0; v < height; v++, dest += vid.rowbytes >> 2,
				 source += pic->width)
			for (u = 0; u < width; u++)
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = d_8to24table[tbyte];
	}
	break;
	default:
		Sys_Error("Draw_SubPic: unsupported r_pixbytes %i", sw32_r_pixbytes);
	}
}


void
sw32_Draw_ConsoleBackground (int lines, byte alpha)
{
	int         x, y, v;
	byte       *src;
	int         f, fstep;
	qpic_t     *conback;

	conback = sw32_Draw_CachePic ("gfx/conback.lmp", true);

	// draw the pic
	switch(sw32_r_pixbytes) {
	case 1:
	{
		byte       *dest = vid.conbuffer;

		for (y = 0; y < lines; y++, dest += vid.conrowbytes) {
			v = (vid.conheight - lines + y) * 200 / vid.conheight;
			src = conback->data + v * 320;
			if (vid.conwidth == 320)
				memcpy (dest, src, vid.conwidth);
			else {
				f = 0;
				fstep = 320 * 0x10000 / vid.conwidth;
				for (x = 0; x < vid.conwidth; x += 4) {
					dest[x] = src[f >> 16];
					f += fstep;
					dest[x + 1] = src[f >> 16];
					f += fstep;
					dest[x + 2] = src[f >> 16];
					f += fstep;
					dest[x + 3] = src[f >> 16];
					f += fstep;
				}
			}
		}
	}
	break;
	case 2:
	{
		unsigned short *dest = (unsigned short *) vid.conbuffer;

		for (y = 0; y < lines; y++, dest += (vid.conrowbytes >> 1)) {
			// FIXME: pre-expand to native format?
			// FIXME: does the endian switching go away in production?
			v = (vid.conheight - lines + y) * 200 / vid.conheight;
			src = conback->data + v * 320;
			f = 0;
			fstep = 320 * 0x10000 / vid.conwidth;
			for (x = 0; x < vid.conwidth; x += 4) {
				dest[x] = sw32_8to16table[src[f >> 16]];
				f += fstep;
				dest[x + 1] = sw32_8to16table[src[f >> 16]];
				f += fstep;
				dest[x + 2] = sw32_8to16table[src[f >> 16]];
				f += fstep;
				dest[x + 3] = sw32_8to16table[src[f >> 16]];
				f += fstep;
			}
		}
	}
	break;
	case 4:
	{
		unsigned int *dest = (unsigned int *) vid.conbuffer;
		for (y = 0; y < lines; y++, dest += (vid.conrowbytes >> 2)) {
			v = (vid.conheight - lines + y) * 200 / vid.conheight;
			src = conback->data + v * 320;
			f = 0;
			fstep = 320 * 0x10000 / vid.conwidth;
			for (x = 0; x < vid.conwidth; x += 4) {
				dest[x    ] = d_8to24table[src[f >> 16]];f += fstep;
				dest[x + 1] = d_8to24table[src[f >> 16]];f += fstep;
				dest[x + 2] = d_8to24table[src[f >> 16]];f += fstep;
				dest[x + 3] = d_8to24table[src[f >> 16]];f += fstep;
			}
		}
	}
	break;

	default:
		Sys_Error("Draw_ConsoleBackground: unsupported r_pixbytes %i",
				  sw32_r_pixbytes);
	}

//	if (!cls.download)
	sw32_Draw_AltString (vid.conwidth - strlen (cl_verstring->string) * 8 - 11,
						 lines - 14, cl_verstring->string);
}

static void
R_DrawRect (vrect_t *prect, int rowbytes, byte * psrc, int transparent)
{
	switch(sw32_r_pixbytes) {
	case 1:
	{
		byte        t;
		int         i, j, srcdelta, destdelta;
		byte       *pdest;

		pdest = (byte *) vid.buffer + prect->y * vid.rowbytes + prect->x;

		srcdelta = rowbytes - prect->width;
		destdelta = vid.rowbytes - prect->width;

		if (transparent)
		{
			for (i = 0; i < prect->height; i++)
			{
				for (j = 0; j < prect->width; j++)
				{
					t = *psrc;
					if (t != TRANSPARENT_COLOR)
						*pdest = t;
					psrc++;
					pdest++;
				}

				psrc += srcdelta;
				pdest += destdelta;
			}
		}
		else
		{
			for (i = 0; i < prect->height; i++)
			{
				memcpy (pdest, psrc, prect->width);
				psrc += rowbytes;
				pdest += vid.rowbytes;
			}
		}
	}
	break;
	case 2:
	{
		int         i, j, srcdelta, destdelta;
		unsigned short *pdest;

		pdest = (unsigned short *) vid.buffer +
			(prect->y * (vid.rowbytes >> 1)) + prect->x;

		srcdelta = rowbytes - prect->width;
		destdelta = (vid.rowbytes >> 1) - prect->width;

		if (transparent) {
			for (i = 0; i < prect->height; i++)
			{
				j = prect->width;
				while(j >= 8)
				{
					j -= 8;
					if (psrc[0] != TRANSPARENT_COLOR)
						pdest[0] = sw32_8to16table[psrc[0]];
					if (psrc[1] != TRANSPARENT_COLOR)
						pdest[1] = sw32_8to16table[psrc[1]];
					if (psrc[2] != TRANSPARENT_COLOR)
						pdest[2] = sw32_8to16table[psrc[2]];
					if (psrc[3] != TRANSPARENT_COLOR)
						pdest[3] = sw32_8to16table[psrc[3]];
					if (psrc[4] != TRANSPARENT_COLOR)
						pdest[4] = sw32_8to16table[psrc[4]];
					if (psrc[5] != TRANSPARENT_COLOR)
						pdest[5] = sw32_8to16table[psrc[5]];
					if (psrc[6] != TRANSPARENT_COLOR)
						pdest[6] = sw32_8to16table[psrc[6]];
					if (psrc[7] != TRANSPARENT_COLOR)
						pdest[7] = sw32_8to16table[psrc[7]];
					psrc += 8;
					pdest += 8;
				}
				if (j & 4)
				{
					if (psrc[0] != TRANSPARENT_COLOR)
						pdest[0] = sw32_8to16table[psrc[0]];
					if (psrc[1] != TRANSPARENT_COLOR)
						pdest[1] = sw32_8to16table[psrc[1]];
					if (psrc[2] != TRANSPARENT_COLOR)
						pdest[2] = sw32_8to16table[psrc[2]];
					if (psrc[3] != TRANSPARENT_COLOR)
						pdest[3] = sw32_8to16table[psrc[3]];
					psrc += 4;
					pdest += 4;
				}
				if (j & 2)
				{
					if (psrc[0] != TRANSPARENT_COLOR)
						pdest[0] = sw32_8to16table[psrc[0]];
					if (psrc[1] != TRANSPARENT_COLOR)
						pdest[1] = sw32_8to16table[psrc[1]];
					psrc += 2;
					pdest += 2;
				}
				if (j & 1)
				{
					if (psrc[0] != TRANSPARENT_COLOR)
						pdest[0] = sw32_8to16table[psrc[0]];
					psrc++;
					pdest++;
				}

				psrc += srcdelta;
				pdest += destdelta;
			}
		} else {
			for (i = 0; i < prect->height; i++, psrc += srcdelta,
					 pdest += destdelta)
			{
				j = prect->width;
				while(j >= 8)
				{
					j -= 8;
					pdest[0] = sw32_8to16table[psrc[0]];
					pdest[1] = sw32_8to16table[psrc[1]];
					pdest[2] = sw32_8to16table[psrc[2]];
					pdest[3] = sw32_8to16table[psrc[3]];
					pdest[4] = sw32_8to16table[psrc[4]];
					pdest[5] = sw32_8to16table[psrc[5]];
					pdest[6] = sw32_8to16table[psrc[6]];
					pdest[7] = sw32_8to16table[psrc[7]];
					psrc += 8;
					pdest += 8;
				}
				if (j & 4)
				{
					pdest[0] = sw32_8to16table[psrc[0]];
					pdest[1] = sw32_8to16table[psrc[1]];
					pdest[2] = sw32_8to16table[psrc[2]];
					pdest[3] = sw32_8to16table[psrc[3]];
					psrc += 4;
					pdest += 4;
				}
				if (j & 2)
				{
					pdest[0] = sw32_8to16table[psrc[0]];
					pdest[1] = sw32_8to16table[psrc[1]];
					psrc += 2;
					pdest += 2;
				}
				if (j & 1)
				{
					pdest[0] = sw32_8to16table[psrc[0]];
					psrc++;
					pdest++;
				}
			}
		}
	}
	break;
	case 4:
	{
		int         i, j, srcdelta, destdelta;
		int        *pdest;

		pdest = (int *) vid.buffer + prect->y * (vid.rowbytes >> 2) + prect->x;

		srcdelta = rowbytes - prect->width;
		destdelta = (vid.rowbytes >> 2) - prect->width;

		if (transparent)
		{
			for (i = 0; i < prect->height; i++)
			{
				j = prect->width;
				while(j >= 8)
				{
					j -= 8;
					if (psrc[0] != TRANSPARENT_COLOR)
						pdest[0] = d_8to24table[psrc[0]];
					if (psrc[1] != TRANSPARENT_COLOR)
						pdest[1] = d_8to24table[psrc[1]];
					if (psrc[2] != TRANSPARENT_COLOR)
						pdest[2] = d_8to24table[psrc[2]];
					if (psrc[3] != TRANSPARENT_COLOR)
						pdest[3] = d_8to24table[psrc[3]];
					if (psrc[4] != TRANSPARENT_COLOR)
						pdest[4] = d_8to24table[psrc[4]];
					if (psrc[5] != TRANSPARENT_COLOR)
						pdest[5] = d_8to24table[psrc[5]];
					if (psrc[6] != TRANSPARENT_COLOR)
						pdest[6] = d_8to24table[psrc[6]];
					if (psrc[7] != TRANSPARENT_COLOR)
						pdest[7] = d_8to24table[psrc[7]];
					psrc += 8;
					pdest += 8;
				}
				if (j & 4)
				{
					if (psrc[0] != TRANSPARENT_COLOR)
						pdest[0] = d_8to24table[psrc[0]];
					if (psrc[1] != TRANSPARENT_COLOR)
						pdest[1] = d_8to24table[psrc[1]];
					if (psrc[2] != TRANSPARENT_COLOR)
						pdest[2] = d_8to24table[psrc[2]];
					if (psrc[3] != TRANSPARENT_COLOR)
						pdest[3] = d_8to24table[psrc[3]];
					psrc += 4;
					pdest += 4;
				}
				if (j & 2)
				{
					if (psrc[0] != TRANSPARENT_COLOR)
						pdest[0] = d_8to24table[psrc[0]];
					if (psrc[1] != TRANSPARENT_COLOR)
						pdest[1] = d_8to24table[psrc[1]];
					psrc += 2;
					pdest += 2;
				}
				if (j & 1)
				{
					if (psrc[0] != TRANSPARENT_COLOR)
						pdest[0] = d_8to24table[psrc[0]];
					psrc++;
					pdest++;
				}

				psrc += srcdelta;
				pdest += destdelta;
			}
		}
		else
		{
			for (i = 0; i < prect->height; i++, psrc += srcdelta,
					 pdest += destdelta)
			{
				j = prect->width;
				while(j >= 8)
				{
					j -= 8;
					pdest[0] = d_8to24table[psrc[0]];
					pdest[1] = d_8to24table[psrc[1]];
					pdest[2] = d_8to24table[psrc[2]];
					pdest[3] = d_8to24table[psrc[3]];
					pdest[4] = d_8to24table[psrc[4]];
					pdest[5] = d_8to24table[psrc[5]];
					pdest[6] = d_8to24table[psrc[6]];
					pdest[7] = d_8to24table[psrc[7]];
					psrc += 8;
					pdest += 8;
				}
				if (j & 4)
				{
					pdest[0] = d_8to24table[psrc[0]];
					pdest[1] = d_8to24table[psrc[1]];
					pdest[2] = d_8to24table[psrc[2]];
					pdest[3] = d_8to24table[psrc[3]];
					psrc += 4;
					pdest += 4;
				}
				if (j & 2)
				{
					pdest[0] = d_8to24table[psrc[0]];
					pdest[1] = d_8to24table[psrc[1]];
					psrc += 2;
					pdest += 2;
				}
				if (j & 1)
				{
					pdest[0] = d_8to24table[psrc[0]];
					psrc++;
					pdest++;
				}
			}
		}
	}
	break;
	default:
		Sys_Error("R_DrawRect: unsupported r_pixbytes %i", sw32_r_pixbytes);
	}
}

/*
	Draw_TileClear

	This repeats a 64*64 tile graphic to fill the screen around a sized down
	refresh window.
*/
void
sw32_Draw_TileClear (int x, int y, int w, int h)
{
	int         width, height, tileoffsetx, tileoffsety;
	byte       *psrc;
	vrect_t     vr;

	CLIP (x, y, w, h, (int) vid.width, (int) vid.height);

	r_rectdesc.rect.x = x;
	r_rectdesc.rect.y = y;
	r_rectdesc.rect.width = w;
	r_rectdesc.rect.height = h;

	vr.y = r_rectdesc.rect.y;
	height = r_rectdesc.rect.height;

	tileoffsety = vr.y % r_rectdesc.height;

	while (height > 0) {
		vr.x = r_rectdesc.rect.x;
		width = r_rectdesc.rect.width;

		if (tileoffsety != 0)
			vr.height = r_rectdesc.height - tileoffsety;
		else
			vr.height = r_rectdesc.height;

		if (vr.height > height)
			vr.height = height;

		tileoffsetx = vr.x % r_rectdesc.width;

		while (width > 0) {
			if (tileoffsetx != 0)
				vr.width = r_rectdesc.width - tileoffsetx;
			else
				vr.width = r_rectdesc.width;

			if (vr.width > width)
				vr.width = width;

			psrc = r_rectdesc.ptexbytes +
				(tileoffsety * r_rectdesc.rowbytes) + tileoffsetx;

			R_DrawRect (&vr, r_rectdesc.rowbytes, psrc, 0);

			vr.x += vr.width;
			width -= vr.width;
			tileoffsetx = 0;	// only the left tile can be left-clipped
		}

		vr.y += vr.height;
		height -= vr.height;
		tileoffsety = 0;		// only the top tile can be top-clipped
	}
}


/*
	Draw_Fill

	Fills a box of pixels with a single color
*/
void
sw32_Draw_Fill (int x, int y, int w, int h, int c)
{
	int         u, v;

	if (x < 0 || x + w > vid.conwidth
		|| y < 0 || y + h > vid.conheight) {
		Sys_MaskPrintf (SYS_vid, "Bad Draw_Fill(%d, %d, %d, %d, %c)\n",
						x, y, w, h, c);
	}
	CLIP (x, y, w, h, (int) vid.width, (int) vid.height);

	switch (sw32_r_pixbytes) {
	case 1:
	{
		byte       *dest = (byte *) vid.buffer + y * vid.rowbytes + x;
		for (v = 0; v < h; v++, dest += vid.rowbytes)
			for (u = 0; u < w; u++)
				dest[u] = c;
	}
	break;
	case 2:
	{
		unsigned short *dest = (unsigned short *) vid.buffer + y *
			(vid.rowbytes >> 1) + x;
		c = sw32_8to16table[c];
		for (v = 0; v < h; v++, dest += (vid.rowbytes >> 1))
			for (u = 0; u < w; u++)
				dest[u] = c;
	}
	break;
	case 4:
	{
		unsigned int *dest = (unsigned int *) vid.buffer + y *
			(vid.rowbytes >> 2) + x;
		c = d_8to24table[c];
		for (v = 0; v < h; v++, dest += (vid.rowbytes >> 2))
			for (u = 0; u < w; u++)
				dest[u] = c;
	}
	break;
	default:
		Sys_Error("Draw_Fill: unsupported r_pixbytes %i", sw32_r_pixbytes);
	}
}


void
sw32_Draw_FadeScreen (void)
{
	int         x, y;

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();

	switch(sw32_r_pixbytes) {
	case 1:
	{
		for (y = 0; y < vid.conheight; y++) {
			unsigned int t;
			byte     *pbuf = (byte *) ((byte *) vid.buffer + vid.rowbytes * y);
			t = (y & 1) << 1;

			for (x = 0; x < vid.conwidth; x++) {
				if ((x & 3) != t)
					pbuf[x] = 0;
			}
		}
	}
	break;
	case 2:
	{
		for (y = 0; y < vid.conheight; y++) {
			unsigned short *pbuf = (unsigned short *)
				((byte *) vid.buffer + vid.rowbytes * y);
			pbuf = (unsigned short *) vid.buffer + (vid.rowbytes >> 1) * y;
			for (x = 0; x < vid.conwidth; x++)
				pbuf[x] = (pbuf[x] >> 1) & 0x7BEF;
		}
	}
	break;
	case 4:
	{
		for (y = 0; y < vid.conheight; y++) {
			unsigned int *pbuf = (unsigned int *)
				((byte *) vid.buffer + vid.rowbytes * y);
			for (x = 0; x < vid.conwidth; x++)
				pbuf[x] = (pbuf[x] >> 1) & 0x7F7F7F7F;
		}
	}
	break;
	default:
		Sys_Error("Draw_FadeScreen: unsupported r_pixbytes %i", sw32_r_pixbytes);
	}
	vr_data.scr_copyeverything = 1;

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}

void
sw32_Draw_BlendScreen (quat_t color)
{
	int         r, g, b, i;
	const byte *basepal;
	byte       *newpal;
	byte        pal[768];

	switch(sw32_r_pixbytes) {
	case 1:
	{
		basepal = vid.basepal;
		newpal = pal;

		for (i = 0; i < 256; i++) {
			r = basepal[0];
			g = basepal[1];
			b = basepal[2];
			basepal += 3;

			r += (int) (color[3] * (color[0] * 256 - r));
			g += (int) (color[3] * (color[1] * 256 - g));
			b += (int) (color[3] * (color[2] * 256 - b));

			newpal[0] = vid.gammatable[r];
			newpal[1] = vid.gammatable[g];
			newpal[2] = vid.gammatable[b];
			newpal += 3;
		}
		vid.vid_internal->set_palette (pal);
	}
	break;
	case 2:
	{
		int         g1, g2;
		unsigned    x, y;
		unsigned short rramp[32], gramp[64], bramp[32], *temp;
		for (i = 0; i < 32; i++) {
			r = i << 3;
			g1 = i << 3;
			g2 = g1 + 4;
			b = i << 3;

			r += (int) (color[3] * (color[0] * 256 - r));
			g1 += (int) (color[3] * (color[1] - g1));
			g2 += (int) (color[3] * (color[1] - g2));
			b += (int) (color[3] * (color[2] - b));

			rramp[i] = (vid.gammatable[r] << 8) & 0xF800;
			gramp[i*2+0] = (vid.gammatable[g1] << 3) & 0x07E0;
			gramp[i*2+1] = (vid.gammatable[g2] << 3) & 0x07E0;
			bramp[i] = (vid.gammatable[b] >> 3) & 0x001F;
		}
		temp = vid.buffer;
		for (y = 0;y < vid.height;y++, temp += (vid.rowbytes >> 1))
			for (x = 0;x < vid.width;x++)
				temp[x] = rramp[(temp[x] & 0xF800) >> 11]
					+ gramp[(temp[x] & 0x07E0) >> 5] + bramp[temp[x] & 0x001F];
	}
	break;
	case 4:
	{
		unsigned    x, y;

		byte ramp[256][4], *temp;
		for (i = 0; i < 256; i++) {
			r = i;
			g = i;
			b = i;

			r += (int) (color[3] * (color[0] * 256 - r));
			g += (int) (color[3] * (color[1] * 256 - g));
			b += (int) (color[3] * (color[2] * 256 - b));

			ramp[i][0] = vid.gammatable[r];
			ramp[i][1] = vid.gammatable[g];
			ramp[i][2] = vid.gammatable[b];
			ramp[i][3] = 0;
		}
		temp = vid.buffer;
		for (y = 0; y < vid.height; y++, temp += vid.rowbytes)
		{
			for (x = 0;x < vid.width;x++)
			{
				temp[x*4+0] = ramp[temp[x*4+0]][0];
				temp[x*4+1] = ramp[temp[x*4+1]][1];
				temp[x*4+2] = ramp[temp[x*4+2]][2];
				temp[x*4+3] = 0;
			}
		}
	}
	break;
	default:
		Sys_Error("sw32_Draw_BlendScreen: unsupported r_pixbytes %i",
				  sw32_r_pixbytes);
	}
}
