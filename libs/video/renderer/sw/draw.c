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

	dest = ((byte*)vid.conbuffer) + y * vid.conrowbytes + x;

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

	dest = ((byte*)vid.conbuffer) + y * vid.conrowbytes + x;
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
	byte       *dest, *source, tbyte;
	int         v, u;

	if (x < 0 || (x + pic->width) > vid.conview->xlen
		|| y < 0 || (y + pic->height) > vid.conview->ylen) {
		Sys_MaskPrintf (SYS_vid, "Draw_Pic: bad coordinates");
		Draw_SubPic (x, y, pic, 0, 0, pic->width, pic->height);
		return;
	}

	source = pic->data;

	dest = ((byte*)vid.buffer) + y * vid.rowbytes + x;

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

	source = pic->data + srcy * pic->width + srcx;

	dest = ((byte*)vid.buffer) + y * vid.rowbytes + x;

	if (width & 7) {			// general
		for (v = 0; v < height; v++) {
			for (u = 0; u < width; u++)
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = tbyte;

			dest += vid.rowbytes;
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
			dest += vid.rowbytes;
			source += pic->width;
		}
	}
}


void
Draw_ConsoleBackground (int lines, byte alpha)
{
	int         x, y, v;
	byte       *src, *dest;
	int         f, fstep;
	qpic_t     *conback;

	conback = Draw_CachePic ("gfx/conback.lmp", false);

	// draw the pic
	dest = vid.conbuffer;

	for (y = 0; y < lines; y++, dest += vid.conrowbytes) {
		v = (vid.conview->ylen - lines + y) * 200 / vid.conview->ylen;
		src = conback->data + v * 320;
		if (vid.conview->xlen == 320)
			memcpy (dest, src, vid.conview->xlen);
		else {
			f = 0;
			fstep = 320 * 0x10000 / vid.conview->xlen;
			for (x = 0; x < vid.conview->xlen; x += 4) {
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

	Draw_AltString (vid.conview->xlen - strlen (cl_verstring->string) * 8 - 11,
					lines - 14, cl_verstring->string);
}

static void
R_DrawRect (vrect_t *prect, int rowbytes, byte * psrc, int transparent)
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
Draw_Fill (int x, int y, int w, int h, int c)
{
	byte       *dest;
	int         u, v;

	if (x < 0 || x + w > vid.conview->xlen
		|| y < 0 || y + h > vid.conview->ylen) {
		Sys_MaskPrintf (SYS_vid, "Bad Draw_Fill(%d, %d, %d, %d, %c)\n",
						x, y, w, h, c);
	}
	CLIP (x, y, w, h, (int) vid.width, (int) vid.height);

	dest = ((byte*)vid.buffer) + y * vid.rowbytes + x;
	for (v = 0; v < h; v++, dest += vid.rowbytes)
		for (u = 0; u < w; u++)
			dest[u] = c;
}


void
Draw_FadeScreen (void)
{
	int         x, y;
	int         height = vid.conview->ylen;
	int         width = vid.conview->xlen / 4;
	uint32_t   *pbuf;

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();

	for (y = 0; y < height; y++) {
		uint32_t    mask;

		pbuf = (uint32_t *) ((byte *)vid.buffer + vid.rowbytes * y);
		mask = 0xff << ((y & 1) << 4);

		for (x = 0; x < width; x++) {
			*pbuf++ &= mask;
		}
	}
	vr_data.scr_copyeverything = 1;

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
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
	vid.vid_internal->set_palette (pal);
}
