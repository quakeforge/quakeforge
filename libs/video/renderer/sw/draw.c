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
#include "QF/ui/view.h"

#include "d_iface.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

typedef struct {
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
Draw_MakePic (int width, int height, const byte *data)
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
Draw_DestroyPic (qpic_t *pic)
{
	free (pic);
}

qpic_t *
Draw_PicFromWad (const char *name)
{
	return W_GetLumpName (name);
}


qpic_t *
Draw_CachePic (const char *path, qboolean alpha)
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
Draw_UncachePic (const char *path)
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
Draw_TextBox (int x, int y, int width, int lines, byte alpha)
{
	qpic_t     *p;
	int         cx, cy;
	int         n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp", true);
	Draw_Pic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp", true);
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp", true);
	Draw_Pic (cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp", true);
		Draw_Pic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp", true);
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp", true);
			Draw_Pic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp", true);
		Draw_Pic (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp", true);
	Draw_Pic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp", true);
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp", true);
	Draw_Pic (cx, cy + 8, p);
}


void
Draw_Init (void)
{
	draw_chars = W_GetLumpName ("conchars");
	draw_disc = W_GetLumpName ("disc");
	draw_backtile = W_GetLumpName ("backtile");

	r_rectdesc.width = draw_backtile->width;
	r_rectdesc.height = draw_backtile->height;
	r_rectdesc.ptexbytes = draw_backtile->data;
	r_rectdesc.rowbytes = draw_backtile->width;
}

void
draw_character_8 (int x, int y, byte *source, int drawline)
{
	byte       *dest = ((byte*)vid.buffer) + y * vid.rowbytes + x;

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
		dest += vid.rowbytes;
	}
}

void
draw_character_16 (int x, int y, byte *source, int drawline)
{
	uint16_t   *dest = (uint16_t *) vid.buffer + y * (vid.rowbytes >> 1) + x;

	while (drawline--) {
		if (source[0])
			dest[0] = d_8to16table[source[0]];
		if (source[1])
			dest[1] = d_8to16table[source[1]];
		if (source[2])
			dest[2] = d_8to16table[source[2]];
		if (source[3])
			dest[3] = d_8to16table[source[3]];
		if (source[4])
			dest[4] = d_8to16table[source[4]];
		if (source[5])
			dest[5] = d_8to16table[source[5]];
		if (source[6])
			dest[6] = d_8to16table[source[6]];
		if (source[7])
			dest[7] = d_8to16table[source[7]];

		source += 128;
		dest += (vid.rowbytes >> 1);
	}
}

void
draw_character_32 (int x, int y, byte *source, int drawline)
{
	uint32_t   *dest = (uint32_t *) vid.buffer + y * (vid.rowbytes >> 2) + x;

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
		dest += (vid.rowbytes >> 2);
	}
}

/*
	Draw_Character

	Draws one 8*8 graphics character with 0 being transparent.
	It can be clipped to the top of the screen to allow the console to be
	smoothly scrolled off.
*/
inline void
Draw_Character (int x, int y, unsigned int chr)
{
	byte       *source;
	int         drawline;
	int         row, col;

	chr &= 255;

	if (y <= -8)
		return;							// totally off screen

	if (y > vid.conview->ylen - 8 || x < 0 || x > vid.conview->xlen - 8)
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

	sw_ctx->draw->draw_character (x, y, source, drawline);
}


void
Draw_String (int x, int y, const char *str)
{
	while (*str) {
		Draw_Character (x, y, *str++);
		x += 8;
	}
}


void
Draw_nString (int x, int y, const char *str, int count)
{
	while (count-- && *str) {
		Draw_Character (x, y, *str++);
		x += 8;
	}
}


void
Draw_AltString (int x, int y, const char *str)
{
	while (*str) {
		Draw_Character (x, y, (*str++) | 0x80);
		x += 8;
	}
}

void
draw_pixel_8 (int x, int y, byte color)
{
	byte       *dest;

	dest = ((byte*)vid.buffer) + y * vid.rowbytes + x;
	*dest = color;
}

void
draw_pixel_16 (int x, int y, byte color)
{
	uint16_t   *dest;

	dest = ((uint16_t*)vid.buffer) + y * vid.rowbytes + x;
	*dest = d_8to16table[color];
}

void
draw_pixel_32 (int x, int y, byte color)
{
	uint32_t   *dest;

	dest = ((uint32_t*)vid.buffer) + y * vid.rowbytes + x;
	*dest = d_8to24table[color];
}

static void
crosshair_1 (int x, int y)
{
	Draw_Character (x - 4, y - 4, '+');
}

static void
crosshair_2 (int x, int y)
{
	byte        c = crosshaircolor->int_val;

	sw_ctx->draw->draw_pixel (x - 1, y, c);
	sw_ctx->draw->draw_pixel (x - 3, y, c);
	sw_ctx->draw->draw_pixel (x + 1, y, c);
	sw_ctx->draw->draw_pixel (x + 3, y, c);
	sw_ctx->draw->draw_pixel (x, y - 1, c);
	sw_ctx->draw->draw_pixel (x, y - 3, c);
	sw_ctx->draw->draw_pixel (x, y + 1, c);
	sw_ctx->draw->draw_pixel (x, y + 3, c);
}

static void
crosshair_3 (int x, int y)
{
	byte        c = crosshaircolor->int_val;

	sw_ctx->draw->draw_pixel (x - 3, y - 3, c);
	sw_ctx->draw->draw_pixel (x + 3, y - 3, c);
	sw_ctx->draw->draw_pixel (x - 2, y - 2, c);
	sw_ctx->draw->draw_pixel (x + 2, y - 2, c);
	sw_ctx->draw->draw_pixel (x - 3, y + 3, c);
	sw_ctx->draw->draw_pixel (x + 2, y + 2, c);
	sw_ctx->draw->draw_pixel (x - 2, y + 2, c);
	sw_ctx->draw->draw_pixel (x + 3, y + 3, c);
}

static void
crosshair_4 (int x, int y)
{
	//byte        c = crosshaircolor->int_val;

	sw_ctx->draw->draw_pixel (x,     y - 2, 8);
	sw_ctx->draw->draw_pixel (x + 1, y - 2, 9);

	sw_ctx->draw->draw_pixel (x,     y - 1, 6);
	sw_ctx->draw->draw_pixel (x + 1, y - 1, 8);
	sw_ctx->draw->draw_pixel (x + 2, y - 1, 2);

	sw_ctx->draw->draw_pixel (x - 2, y,     6);
	sw_ctx->draw->draw_pixel (x - 1, y,     8);
	sw_ctx->draw->draw_pixel (x,     y,     8);
	sw_ctx->draw->draw_pixel (x + 1, y,     6);
	sw_ctx->draw->draw_pixel (x + 2, y,     8);
	sw_ctx->draw->draw_pixel (x + 3, y,     8);

	sw_ctx->draw->draw_pixel (x - 1, y + 1, 2);
	sw_ctx->draw->draw_pixel (x,     y + 1, 8);
	sw_ctx->draw->draw_pixel (x + 1, y + 1, 8);
	sw_ctx->draw->draw_pixel (x + 2, y + 1, 2);
	sw_ctx->draw->draw_pixel (x + 3, y + 1, 2);
	sw_ctx->draw->draw_pixel (x + 4, y + 1, 2);

	sw_ctx->draw->draw_pixel (x,     y + 2, 7);
	sw_ctx->draw->draw_pixel (x + 1, y + 2, 8);
	sw_ctx->draw->draw_pixel (x + 2, y + 2, 2);

	sw_ctx->draw->draw_pixel (x + 1, y + 3, 2);
	sw_ctx->draw->draw_pixel (x + 2, y + 3, 2);
}

static void
crosshair_5 (int x, int y)
{
	byte        c = crosshaircolor->int_val;

	sw_ctx->draw->draw_pixel (x - 1, y - 3, c);
	sw_ctx->draw->draw_pixel (x + 0, y - 3, c);
	sw_ctx->draw->draw_pixel (x + 1, y - 3, c);

	sw_ctx->draw->draw_pixel (x - 2, y - 2, c);
	sw_ctx->draw->draw_pixel (x + 2, y - 2, c);

	sw_ctx->draw->draw_pixel (x - 3, y - 1, c);
	sw_ctx->draw->draw_pixel (x + 3, y - 1, c);

	sw_ctx->draw->draw_pixel (x - 3, y, c);
	sw_ctx->draw->draw_pixel (x, y, c);
	sw_ctx->draw->draw_pixel (x + 3, y, c);

	sw_ctx->draw->draw_pixel (x - 3, y + 1, c);
	sw_ctx->draw->draw_pixel (x + 3, y + 1, c);

	sw_ctx->draw->draw_pixel (x - 2, y + 2, c);
	sw_ctx->draw->draw_pixel (x + 2, y + 2, c);

	sw_ctx->draw->draw_pixel (x - 1, y + 3, c);
	sw_ctx->draw->draw_pixel (x + 0, y + 3, c);
	sw_ctx->draw->draw_pixel (x + 1, y + 3, c);
}

static void (*crosshair_func[]) (int x, int y) = {
	crosshair_1,
	crosshair_2,
	crosshair_3,
	crosshair_4,
	crosshair_5,
};

void
Draw_Crosshair (void)
{
	int            x, y;
	int            ch;

	ch = crosshair->int_val - 1;
	if ((unsigned) ch >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	x = vid.conview->xlen / 2 + cl_crossx->int_val;
	y = vid.conview->ylen / 2 + cl_crossy->int_val;

	crosshair_func[ch] (x, y);
}

void
Draw_CrosshairAt (int ch, int x, int y)
{
	ch -= 1;
	if ((unsigned) ch >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	crosshair_func[ch] (x, y);
}


void
Draw_Pic (int x, int y, qpic_t *pic)
{
	if (x < 0 || (x + pic->width) > vid.conview->xlen
		|| y < 0 || (y + pic->height) > vid.conview->ylen) {
		Sys_MaskPrintf (SYS_vid, "Draw_Pic: bad coordinates");
	}
	Draw_SubPic (x, y, pic, 0, 0, pic->width, pic->height);
}

void
Draw_Picf (float x, float y, qpic_t *pic)
{
	Draw_Pic (x, y, pic);
}

void
draw_subpic_8 (int x, int y, struct qpic_s *pic, int srcx, int srcy,
			   int width, int height)
{
	byte       *source = pic->data + srcy * pic->width + srcx;
	byte       *dest = (byte *) vid.buffer + y * vid.rowbytes + x;

	for (int v = 0; v < height; v++) {
		for (int u = 0; u < width; u++) {
			byte        tbyte = source[u];
			if (tbyte != TRANSPARENT_COLOR) {
				dest[u] = tbyte;
			}
		}
		dest += vid.rowbytes;
		source += pic->width;
	}
}

void
draw_subpic_16 (int x, int y, struct qpic_s *pic, int srcx, int srcy,
			    int width, int height)
{
	byte       *source = pic->data + srcy * pic->width + srcx;
	uint16_t   *dest = (uint16_t *) vid.buffer + y * (vid.rowbytes >> 1) + x;
	for (int v = 0; v < height; v++) {
		for (int u = 0; u < width; u++) {
			byte        tbyte = source[u];
			if (tbyte != TRANSPARENT_COLOR) {
				dest[u] = d_8to16table[tbyte];
			}
		}
		dest += vid.rowbytes >> 1;
		source += pic->width;
	}
}

void
draw_subpic_32 (int x, int y, struct qpic_s *pic, int srcx, int srcy,
			    int width, int height)
{
	byte       *source = pic->data + srcy * pic->width + srcx;
	uint32_t   *dest = (uint32_t *) vid.buffer + y * (vid.rowbytes >> 2) + x;
	for (int v = 0; v < height; v++) {
		for (int u = 0; u < width; u++) {
			byte        tbyte = source[u];
			if (tbyte != TRANSPARENT_COLOR) {
				dest[u] = d_8to24table[tbyte];
			}
		}
		dest += vid.rowbytes >> 2;
		source += pic->width;
	}
}

void
Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
			 int height)
{
	if ((x < 0) || (x + width > vid.conview->xlen)
		|| (y < 0) || (y + height > vid.conview->ylen)) {
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

	sw_ctx->draw->draw_subpic (x, y, pic, srcx, srcy, width, height);
}

void
draw_console_background_8 (int lines, byte *data)
{
	byte       *dest = vid.buffer;

	for (int y = 0; y < lines; y++, dest += vid.rowbytes) {
		int v = (vid.conview->ylen - lines + y) * 200 / vid.conview->ylen;
		byte *src = data + v * 320;
		if (vid.conview->xlen == 320)
			memcpy (dest, src, vid.conview->xlen);
		else {
			int f = 0;
			int fstep = 320 * 0x10000 / vid.conview->xlen;
			for (int x = 0; x < vid.conview->xlen; x += 4) {
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

void
draw_console_background_16 (int lines, byte *data)
{
	uint16_t   *dest = (uint16_t *) vid.buffer;

	for (int y = 0; y < lines; y++, dest += (vid.rowbytes >> 1)) {
		// FIXME: pre-expand to native format?
		// FIXME: does the endian switching go away in production?
		int v = (vid.conview->ylen - lines + y) * 200 / vid.conview->ylen;
		byte *src = data + v * 320;
		int f = 0;
		int fstep = 320 * 0x10000 / vid.conview->xlen;
		for (int x = 0; x < vid.conview->xlen; x += 4) {
			dest[x] = d_8to16table[src[f >> 16]];
			f += fstep;
			dest[x + 1] = d_8to16table[src[f >> 16]];
			f += fstep;
			dest[x + 2] = d_8to16table[src[f >> 16]];
			f += fstep;
			dest[x + 3] = d_8to16table[src[f >> 16]];
			f += fstep;
		}
	}
}

void
draw_console_background_32 (int lines, byte *data)
{
	uint32_t   *dest = (uint32_t *) vid.buffer;

	for (int y = 0; y < lines; y++, dest += (vid.rowbytes >> 2)) {
		// FIXME: pre-expand to native format?
		// FIXME: does the endian switching go away in production?
		int v = (vid.conview->ylen - lines + y) * 200 / vid.conview->ylen;
		byte *src = data + v * 320;
		int f = 0;
		int fstep = 320 * 0x10000 / vid.conview->xlen;
		for (int x = 0; x < vid.conview->xlen; x += 4) {
			dest[x] = d_8to24table[src[f >> 16]];
			f += fstep;
			dest[x + 1] = d_8to24table[src[f >> 16]];
			f += fstep;
			dest[x + 2] = d_8to24table[src[f >> 16]];
			f += fstep;
			dest[x + 3] = d_8to24table[src[f >> 16]];
			f += fstep;
		}
	}
}

void
Draw_ConsoleBackground (int lines, byte alpha)
{
	qpic_t     *conback = Draw_CachePic ("gfx/conback.lmp", false);

	// draw the pic
	sw_ctx->draw->draw_console_background (lines, conback->data);

	Draw_AltString (vid.conview->xlen - strlen (cl_verstring->string) * 8 - 11,
					lines - 14, cl_verstring->string);
}

void
draw_rect_8 (vrect_t *prect, int rowbytes, byte *psrc, int transparent)
{
	byte        t;
	int         i, j, srcdelta, destdelta;
	byte       *pdest;

	pdest = ((byte*)vid.buffer) + (prect->y * vid.rowbytes) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = vid.rowbytes - prect->width;

	if (transparent) {
		for (i = 0; i < prect->height; i++) {
			for (j = 0; j < prect->width; j++) {
				t = *psrc;
				if (t != TRANSPARENT_COLOR) {
					*pdest = t;
				}

				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	} else {
		for (i = 0; i < prect->height; i++) {
			memcpy (pdest, psrc, prect->width);
			psrc += rowbytes;
			pdest += vid.rowbytes;
		}
	}
}

void
draw_rect_16 (vrect_t *prect, int rowbytes, byte *psrc, int transparent)
{
	int         i, j, srcdelta, destdelta;
	uint16_t   *pdest;

	pdest = (uint16_t *) vid.buffer +
		(prect->y * (vid.rowbytes >> 1)) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = (vid.rowbytes >> 1) - prect->width;

	if (transparent) {
		for (i = 0; i < prect->height; i++) {
			j = prect->width;
			while(j >= 8) {
				j -= 8;
				if (psrc[0] != TRANSPARENT_COLOR)
					pdest[0] = d_8to16table[psrc[0]];
				if (psrc[1] != TRANSPARENT_COLOR)
					pdest[1] = d_8to16table[psrc[1]];
				if (psrc[2] != TRANSPARENT_COLOR)
					pdest[2] = d_8to16table[psrc[2]];
				if (psrc[3] != TRANSPARENT_COLOR)
					pdest[3] = d_8to16table[psrc[3]];
				if (psrc[4] != TRANSPARENT_COLOR)
					pdest[4] = d_8to16table[psrc[4]];
				if (psrc[5] != TRANSPARENT_COLOR)
					pdest[5] = d_8to16table[psrc[5]];
				if (psrc[6] != TRANSPARENT_COLOR)
					pdest[6] = d_8to16table[psrc[6]];
				if (psrc[7] != TRANSPARENT_COLOR)
					pdest[7] = d_8to16table[psrc[7]];
				psrc += 8;
				pdest += 8;
			}
			if (j & 4) {
				if (psrc[0] != TRANSPARENT_COLOR)
					pdest[0] = d_8to16table[psrc[0]];
				if (psrc[1] != TRANSPARENT_COLOR)
					pdest[1] = d_8to16table[psrc[1]];
				if (psrc[2] != TRANSPARENT_COLOR)
					pdest[2] = d_8to16table[psrc[2]];
				if (psrc[3] != TRANSPARENT_COLOR)
					pdest[3] = d_8to16table[psrc[3]];
				psrc += 4;
				pdest += 4;
			}
			if (j & 2) {
				if (psrc[0] != TRANSPARENT_COLOR)
					pdest[0] = d_8to16table[psrc[0]];
				if (psrc[1] != TRANSPARENT_COLOR)
					pdest[1] = d_8to16table[psrc[1]];
				psrc += 2;
				pdest += 2;
			}
			if (j & 1) {
				if (psrc[0] != TRANSPARENT_COLOR)
					pdest[0] = d_8to16table[psrc[0]];
				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	} else {
		for (i = 0; i < prect->height;
			 i++, psrc += srcdelta, pdest += destdelta) {
			j = prect->width;
			while(j >= 8) {
				j -= 8;
				pdest[0] = d_8to16table[psrc[0]];
				pdest[1] = d_8to16table[psrc[1]];
				pdest[2] = d_8to16table[psrc[2]];
				pdest[3] = d_8to16table[psrc[3]];
				pdest[4] = d_8to16table[psrc[4]];
				pdest[5] = d_8to16table[psrc[5]];
				pdest[6] = d_8to16table[psrc[6]];
				pdest[7] = d_8to16table[psrc[7]];
				psrc += 8;
				pdest += 8;
			}
			if (j & 4) {
				pdest[0] = d_8to16table[psrc[0]];
				pdest[1] = d_8to16table[psrc[1]];
				pdest[2] = d_8to16table[psrc[2]];
				pdest[3] = d_8to16table[psrc[3]];
				psrc += 4;
				pdest += 4;
			}
			if (j & 2) {
				pdest[0] = d_8to16table[psrc[0]];
				pdest[1] = d_8to16table[psrc[1]];
				psrc += 2;
				pdest += 2;
			}
			if (j & 1) {
				pdest[0] = d_8to16table[psrc[0]];
				psrc++;
				pdest++;
			}
		}
	}
}

void
draw_rect_32 (vrect_t *prect, int rowbytes, byte *psrc, int transparent)
{
	int         i, j, srcdelta, destdelta;
	uint32_t   *pdest;

	pdest = (uint32_t *)vid.buffer + prect->y * (vid.rowbytes >> 2) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = (vid.rowbytes >> 2) - prect->width;

	if (transparent) {
		for (i = 0; i < prect->height; i++) {
			j = prect->width;
			while(j >= 8) {
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
			if (j & 4) {
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
			if (j & 2) {
				if (psrc[0] != TRANSPARENT_COLOR)
					pdest[0] = d_8to24table[psrc[0]];
				if (psrc[1] != TRANSPARENT_COLOR)
					pdest[1] = d_8to24table[psrc[1]];
				psrc += 2;
				pdest += 2;
			}
			if (j & 1) {
				if (psrc[0] != TRANSPARENT_COLOR)
					pdest[0] = d_8to24table[psrc[0]];
				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	} else {
		for (i = 0; i < prect->height;
			 i++, psrc += srcdelta, pdest += destdelta) {
			j = prect->width;
			while(j >= 8) {
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
			if (j & 4) {
				pdest[0] = d_8to24table[psrc[0]];
				pdest[1] = d_8to24table[psrc[1]];
				pdest[2] = d_8to24table[psrc[2]];
				pdest[3] = d_8to24table[psrc[3]];
				psrc += 4;
				pdest += 4;
			}
			if (j & 2) {
				pdest[0] = d_8to24table[psrc[0]];
				pdest[1] = d_8to24table[psrc[1]];
				psrc += 2;
				pdest += 2;
			}
			if (j & 1) {
				pdest[0] = d_8to24table[psrc[0]];
				psrc++;
				pdest++;
			}
		}
	}
}

static void
R_DrawRect (vrect_t *prect, int rowbytes, byte *psrc, int transparent)
{
	sw_ctx->draw->draw_rect (prect, rowbytes, psrc, transparent);
}

/*
	Draw_TileClear

	This repeats a 64*64 tile graphic to fill the screen around a sized down
	refresh window.
*/
void
Draw_TileClear (int x, int y, int w, int h)
{
	int         width, height, tileoffsetx, tileoffsety;
	byte       *psrc;
	vrect_t     vr;

	CLIP (x, y, w, h, (int) vid.width, (int) vid.height);

	vr.y = y;
	height = h;

	tileoffsety = vr.y % r_rectdesc.height;

	while (height > 0) {
		vr.x = x;
		width = w;

		vr.height = r_rectdesc.height - tileoffsety;

		if (vr.height > height)
			vr.height = height;

		tileoffsetx = vr.x % r_rectdesc.width;

		while (width > 0) {
			vr.width = r_rectdesc.width - tileoffsetx;
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

void
draw_fill_8 (int x, int y, int w, int h, int c)
{
	byte       *dest = ((byte*)vid.buffer) + y * vid.rowbytes + x;
	for (int v = 0; v < h; v++, dest += vid.rowbytes) {
		for (int u = 0; u < w; u++) {
			dest[u] = c;
		}
	}
}

void
draw_fill_16 (int x, int y, int w, int h, int c)
{
	byte       *dest = ((byte*)vid.buffer) + y * (vid.rowbytes >> 1) + x;
	c = d_8to16table[c];
	for (int v = 0; v < h; v++, dest += vid.rowbytes >> 1) {
		for (int u = 0; u < w; u++) {
			dest[u] = c;
		}
	}
}

void
draw_fill_32 (int x, int y, int w, int h, int c)
{
	byte       *dest = ((byte*)vid.buffer) + y * (vid.rowbytes >> 2) + x;
	c = d_8to24table[c];
	for (int v = 0; v < h; v++, dest += vid.rowbytes >> 2) {
		for (int u = 0; u < w; u++) {
			dest[u] = c;
		}
	}
}

/*
	Draw_Fill

	Fills a box of pixels with a single color
*/
void
Draw_Fill (int x, int y, int w, int h, int c)
{
	if (x < 0 || x + w > vid.conview->xlen
		|| y < 0 || y + h > vid.conview->ylen) {
		Sys_MaskPrintf (SYS_vid, "Bad Draw_Fill(%d, %d, %d, %d, %c)\n",
						x, y, w, h, c);
	}
	CLIP (x, y, w, h, (int) vid.width, (int) vid.height);

	sw_ctx->draw->draw_fill (x, y, w, h, c);
}

void
draw_fadescreen_8 (void)
{
	int         height = vid.conview->ylen;
	int         width = vid.conview->xlen / 4;
	int         offset = vid.rowbytes;
	uint32_t   *pbuf;

	for (int y = 0; y < height; y++) {
		uint32_t    mask;

		pbuf = (uint32_t *) ((byte *)vid.buffer + offset * y);
		mask = 0xff << ((y & 1) << 4);

		for (int x = 0; x < width; x++) {
			*pbuf++ &= mask;
		}
	}
}

void
draw_fadescreen_16 (void)
{
	int         height = vid.conview->ylen;
	int         width = vid.conview->xlen / 4;
	int         offset = vid.rowbytes >> 1;
	uint32_t   *pbuf;

	for (int y = 0; y < height; y++) {
		pbuf = (uint32_t *) ((byte *)vid.buffer + offset * y);

		for (int x = 0; x < width; x++, pbuf++) {
			*pbuf = (*pbuf >> 1) & 0x7bef7bef;
		}
	}
}

void
draw_fadescreen_32 (void)
{
	int         height = vid.conview->ylen;
	int         width = vid.conview->xlen / 4;
	int         offset = vid.rowbytes >> 2;
	uint32_t   *pbuf;

	for (int y = 0; y < height; y++) {
		pbuf = (uint32_t *) ((byte *)vid.buffer + offset * y);

		for (int x = 0; x < width; x++, pbuf++) {
			*pbuf = (*pbuf >> 1) & 0x7f7f7f7f;
		}
	}
}

void
Draw_FadeScreen (void)
{

	S_ExtraUpdate ();

	sw_ctx->draw->draw_fadescreen ();
	vr_data.scr_copyeverything = 1;

	S_ExtraUpdate ();
}

void
draw_blendscreen_8 (quat_t color)
{
	int         r, g, b, i;
	const byte *basepal;
	byte       *newpal;
	byte        pal[768];
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
	vid.vid_internal->set_palette (vid.vid_internal->data, pal);
}

void
draw_blendscreen_16 (quat_t color)
{
	int         i, r, b;
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

void
draw_blendscreen_32 (quat_t color)
{
	unsigned    x, y;
	int         i, r, g, b;

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

void
Draw_BlendScreen (quat_t color)
{
	sw_ctx->draw->draw_blendscreen (color);
}
