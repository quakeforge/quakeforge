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

#include "QF/quakefs.h"
#include "QF/ui/font.h"
#include "QF/ui/view.h"

#include "d_iface.h"
#include "d_local.h"
#include "r_internal.h"
#include "vid_internal.h"

typedef struct swfont_s {
	font_t     *font;
} swfont_t;

typedef struct swfontset_s
    DARRAY_TYPE (swfont_t) swfontset_t;

static swfontset_t sw_fonts = DARRAY_STATIC_INIT (16);

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

qpic_t * __attribute__((const))
Draw_PicFromWad (const char *name)
{
	return W_GetLumpName (name);
}


qpic_t *
Draw_CachePic (const char *path, bool alpha)
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

	if (!draw_chars) {
		qpic_t     *pic = Draw_Font8x8Pic ();
		draw_chars = pic->data;	// FIXME indirect hold on the memory
		//FIXME param to Draw_Font8x8Pic
		for (int i = 0; i < pic->width * pic->height; i++) {
			pic->data[i] = pic->data[i] == 255 ? 0 : pic->data[i];
		}
	}
	if (draw_backtile) {
		r_rectdesc.width = draw_backtile->width;
		r_rectdesc.height = draw_backtile->height;
		r_rectdesc.ptexbytes = draw_backtile->data;
		r_rectdesc.rowbytes = draw_backtile->width;
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
	byte       *dest;
	byte       *source;
	int         drawline;
	int         row, col;

	chr &= 255;

	if (y <= -8)
		return;							// totally off screen

	if (y > (int) vid.height - 8 || x < 0 || x > (int) vid.width - 8)
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

	dest = d_viewbuffer + y * d_rowbytes + x;

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
		dest += d_rowbytes;
	}
}

void
sw_Draw_CharBuffer (int x, int y, draw_charbuffer_t *buffer)
{
	const byte *line = (byte *) buffer->chars;
	int         width = buffer->width;
	int         height = buffer->height;
	while (height-- > 0) {
		for (int i = 0; i < width; i++) {
			Draw_Character (x + i * 8, y, line[i]);
		}
		line += width;
		y += 8;
	}
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


static void
Draw_Pixel (int x, int y, byte color)
{
	byte       *dest;

	dest = d_viewbuffer + y * d_rowbytes + x;
	*dest = color;
}

static void
crosshair_1 (int x, int y)
{
	Draw_Character (x - 4, y - 4, '+');
}

static void
crosshair_2 (int x, int y)
{
	byte        c = crosshaircolor;

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
	byte        c = crosshaircolor;

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
	//byte        c = crosshaircolor;

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
	byte        c = crosshaircolor;

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
Draw_Crosshair (void)
{
	int            x, y;
	int            ch;

	ch = crosshair - 1;
	if ((unsigned) ch >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	x = vid.width / 2 + cl_crossx;
	y = vid.height / 2 + cl_crossy;

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
	byte       *dest, *source, tbyte;
	int         v, u;

	if (x < 0 || (x + pic->width) > (int) vid.width
		|| y < 0 || (y + pic->height) > (int) vid.height) {
		Sys_MaskPrintf (SYS_vid, "Draw_Pic: bad coordinates");
		Draw_SubPic (x, y, pic, 0, 0, pic->width, pic->height);
		return;
	}

	source = pic->data;

	dest = d_viewbuffer + y * d_rowbytes + x;

	for (v = 0; v < pic->height; v++) {
		for (u = 0; u < pic->width; u++)
			if ((tbyte = source[u]) != TRANSPARENT_COLOR)
				dest[u] = tbyte;

		dest += d_rowbytes;
		source += pic->width;
	}
}

void
Draw_FitPic (int x, int y, int width, int height, qpic_t *pic)
{
	int         v_width = vid.width;
	int         v_height = vid.height;
	if (x > v_width || y > v_width || x + width <= 0 || y + height <= 0) {
		return;
	}
	if (width == pic->width && height == pic->height) {
		Draw_Pic (x, y, pic);
		return;
	}
	int         sstep = pic->width * 0x10000 / width;
	int         tstep = pic->height * 0x10000 / height;
	int         sx = 0, ex = width;
	int         sy = 0, ey = height;

	if (x < 0) {
		sx -= x;
		ex += x;
	}
	if (y < 0) {
		sy -= y;
		ey += y;
	}
	if (x + width > v_width) {
		ex -= x + width - v_width;
	}
	if (y + height > v_height) {
		ey -= y + height - v_height;
	}
	x += sx;
	y += sy;

	byte       *src, *dst;

	// draw the pic
	dst = d_viewbuffer + y * d_rowbytes + x;

	for (int y = sy; y < sy + ey; y++, dst += d_rowbytes) {
		src = pic->data + ((y * tstep) >> 16) * pic->width;
		if (width == pic->width)
			memcpy (dst, src, width);
		else {
			int         f = sx * sstep;
			for (int x = 0; x < ex; x++, f += sstep) {
				dst[x] = src[f >> 16];
			}
		}
	}
}

void
Draw_Picf (float x, float y, qpic_t *pic)
{
	Draw_Pic (x, y, pic);
}

void
Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
			 int height)
{
	byte       *dest, *source, tbyte;
	int         u, v;

	if ((x < 0) || (x + width > (int) vid.width)
		|| (y < 0) || (y + height > (int) vid.height)) {
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

	dest = d_viewbuffer + y * d_rowbytes + x;

	if (width & 7) {			// general
		for (v = 0; v < height; v++) {
			for (u = 0; u < width; u++)
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = tbyte;

			dest += d_rowbytes;
			source += pic->width;
		}
	} else {						// unwound
		for (v = 0; v < height; v++) {
			for (u = 0; u < width; u += 8) {
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
			dest += d_rowbytes;
			source += pic->width;
		}
	}
}


void
Draw_ConsoleBackground (int lines, byte alpha)
{
	byte       *src, *dest;
	int         f, fstep;
	qpic_t     *conback;

	conback = Draw_CachePic ("gfx/conback.lmp", false);

	// draw the pic
	dest = d_viewbuffer;

	for (int y = 0; y < lines; y++, dest += d_rowbytes) {
		int         v = (vid.height - lines + y) * 200 / vid.height;
		src = conback->data + v * 320;
		if (vid.width == 320)
			memcpy (dest, src, vid.width);
		else {
			f = 0;
			fstep = 320 * 0x10000 / vid.width;
			for (unsigned x = 0; x < vid.width; x++) {
				dest[x] = src[f >> 16];
				f += fstep;
			}
		}
	}

	Draw_AltString (vid.width - strlen (cl_verstring) * 8 - 11,
					lines - 14, cl_verstring);
}

static void
R_DrawRect (vrect_t *prect, int rowbytes, byte * psrc, int transparent)
{
	byte        t;
	int         i, j, srcdelta, destdelta;
	byte       *pdest;

	pdest = d_viewbuffer + (prect->y * d_rowbytes) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = d_rowbytes - prect->width;

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
			pdest += d_rowbytes;
		}
	}
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

	if (!r_rectdesc.height) {
		return;
	}
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


/*
	Draw_Fill

	Fills a box of pixels with a single color
*/
void
Draw_Fill (int x, int y, int w, int h, int c)
{
	byte       *dest;
	int         u, v;

	if (x < 0 || x + w > (int) vid.width || y < 0 || y + h > (int) vid.height) {
		Sys_MaskPrintf (SYS_vid, "Bad Draw_Fill(%d, %d, %d, %d, %c)\n",
						x, y, w, h, c);
	}
	CLIP (x, y, w, h, (int) vid.width, (int) vid.height);

	dest = d_viewbuffer + y * d_rowbytes + x;
	for (v = 0; v < h; v++, dest += d_rowbytes)
		for (u = 0; u < w; u++)
			dest[u] = c;
}

static inline byte *
draw_horizontal (byte *dest, int xs, int len, int c)
{
	while (len-- > 0) {
		*dest = c;
		dest += xs;
	}
	return dest;
}

static inline byte *
draw_vertical (byte *dest, int len, int c)
{
	while (len-- > 0) {
		*dest = c;
		dest += d_rowbytes;
	}
	return dest;
}

static void
draw_line (int x0, int y0, int x1, int y1, int c)
{
	// Bresenham's line slice algorith (ala Michael Abrash)
	int         dx, dy, sx;
	// always go top to bottom
	if (y1 < y0) {
		int         t;
		t = y1; y1 = y0; y0 = t;
		t = x1; x1 = x0; x0 = t;
	}
	dy = y1 - y0;

	if ((dx = x1 - x0) < 0) {
		sx = -1;
		dx = -dx;
	} else  {
		sx = 1;
	}

	byte        *dest = d_viewbuffer + y0 * d_rowbytes + x0;

	if (!dx) {
		draw_vertical (dest, dy, c);
		return;
	}
	if (!dy) {
		draw_horizontal (dest, sx, dx, c);
		return;
	}
	if (dx == dy) {
		while (dy-- > 0) {
			*dest = c;
			dest += d_rowbytes + sx;
		}
		return;
	}
	if (dx > dy) {
		int         step = dx / dy;
		int         adj_up = (dx % dy) * 2;
		int         adj_down = dy * 2;
		int         error = (dx % dy) - adj_down;
		int         initial = step / 2 + 1;
		int         final = initial;

		if (!adj_up && !(step & 1)) {
			initial--;
		}
		if (step & 1) {
			error += dy;
		}
		dest = draw_horizontal (dest, sx, initial, c);
		dest += d_rowbytes;
		while (dy-- > 1) {
			int         run = step;
			if ((error += adj_up) > 0) {
				run++;
				error -= adj_down;
			}
			dest = draw_horizontal (dest, sx, run, c);
			dest += d_rowbytes;
		}
		dest = draw_horizontal (dest, sx, final, c);
	} else {
		int         step = dy / dx;
		int         adj_up = (dy % dx) * 2;
		int         adj_down = dx * 2;
		int         error = (dy % dx) - adj_down;
		int         initial = step / 2 + 1;
		int         final = initial;

		if (!adj_up && !(step & 1)) {
			initial--;
		}
		if (step & 1) {
			error += dx;
		}
		dest = draw_vertical (dest, initial, c);
		dest += sx;
		while (dx-- > 1) {
			int         run = step;
			if ((error += adj_up) > 0) {
				run++;
				error -= adj_down;
			}
			dest = draw_vertical (dest, run, c);
			dest += sx;
		}
		dest = draw_vertical (dest, final, c);
	}
}

static inline byte
test_point (int x, int y)
{
	byte        c = 0;

	if (x < 0) {
		c |= 1;
	} else if (x >= (int) vid.width) {
		c |= 2;
	}
	if (y < 0) {
		c |= 4;
	} else if (y >= (int) vid.height) {
		c |= 8;
	}
	return c;
}

void
Draw_Line (int x0, int y0, int x1, int y1, int c)
{
	byte        c0 = test_point (x0, y0);
	byte        c1 = test_point (x1, y1);
	int         xmax = vid.width - 1;
	int         ymax = vid.height - 1;

	while (c0 | c1) {
		// Cohen-Sutherland line clipping
		if (c0 & c1) {
			return;
		}
		int     dx = x1 - x0;
		int     dy = y1 - y0;
		if (c0 & 1) { y0 = ((   0 - x0) * dy + dx * y0) / dx; x0 =    0; }
		if (c0 & 2) { y0 = ((xmax - x0) * dy + dx * y0) / dx; x0 = xmax; }
		if (c1 & 1) { y1 = ((   0 - x1) * dy + dx * y1) / dx; x1 =    0; }
		if (c1 & 2) { y1 = ((xmax - x1) * dy + dx * y1) / dx; x1 = xmax; }

		if (c0 & 4) { x0 = ((   0 - y0) * dx + dy * x0) / dy; y0 =    0; }
		if (c0 & 8) { x0 = ((ymax - y0) * dx + dy * x0) / dy; y0 = ymax; }
		if (c1 & 4) { x1 = ((   0 - y1) * dx + dy * x1) / dy; y1 =    0; }
		if (c1 & 8) { x1 = ((ymax - y1) * dx + dy * x1) / dy; y1 = ymax; }
		c0 = test_point (x0, y0);
		c1 = test_point (x1, y1);
	}
	draw_line (x0, y0, x1, y1, c);
}

void
Draw_FadeScreen (void)
{
	int         x, y;
	int         height = vid.height;
	int         width = vid.width / 4;
	uint32_t   *pbuf;

	for (y = 0; y < height; y++) {
		uint32_t    mask;

		pbuf = (uint32_t *) (d_viewbuffer + d_rowbytes * y);
		mask = 0xff << ((y & 1) << 4);

		for (x = 0; x < width; x++) {
			*pbuf++ &= mask;
		}
	}
	vr_data.scr_copyeverything = 1;
}

void
Draw_BlendScreen (quat_t color)
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
	vid.vid_internal->set_palette (vid.vid_internal->ctx, pal);
}

int
Draw_AddFont (struct font_s *rfont)
{
	int         fontid = sw_fonts.size;
	DARRAY_OPEN_AT (&sw_fonts, fontid, 1);
	swfont_t   *font = &sw_fonts.a[fontid];

	font->font = rfont;
	return fontid;
}
#if 0
typedef struct {
	vrect_t    *glyph_rects;
	byte       *bitmap;
	int         width;
	byte        color;
} swrgctx_t;

static void
sw_render_glyph (uint32_t glyphid, int x, int y, void *_rgctx)
{
	swrgctx_t  *rgctx = _rgctx;

	float       w = rect->width;
	float       h = rect->height;
	if (x < 0 || y < 0 || x + w > vid.width || y + h > vid.height) {
		return;
	}
	int         u = rect->x;
	int         v = rect->y;
	byte        c = rgctx->color;
	byte       *src = rgctx->bitmap + v * rgctx->width + u;
	byte       *dst = d_viewbuffer + y * d_rowbytes + x;
	while (h-- > 0) {
		for (int i = 0; i < w; i++) {
			if (src[i] > 127) {
				dst[i] = c;
			}
		}
		src += rgctx->width;
		dst += d_rowbytes;
	}
}
#endif
void
Draw_Glyph (int x, int y, int fontid, int glyphid, int c)
{
	if (fontid < 0 || (unsigned) fontid > sw_fonts.size) {
		return;
	}
	swfont_t   *font = &sw_fonts.a[fontid];
	font_t     *rfont = font->font;
	vrect_t    *rect = &rfont->glyph_rects[glyphid];
	int         width = rfont->scrap.width;

	float       w = rect->width;
	float       h = rect->height;
	if (x < 0 || y < 0 || x + w > vid.width || y + h > vid.height) {
		return;
	}
	int         u = rect->x;
	int         v = rect->y;
	byte       *src = rfont->scrap_bitmap + v * width + u;
	byte       *dst = d_viewbuffer + y * d_rowbytes + x;
	while (h-- > 0) {
		for (int i = 0; i < w; i++) {
			if (src[i] > 127) {
				dst[i] = c;
			}
		}
		src += width;
		dst += d_rowbytes;
	}
}
