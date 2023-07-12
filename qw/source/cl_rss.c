/*
	cl_screen.c

	master for refresh, status bar, console, chat, notify, etc

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

#include <time.h>

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/pcx.h"
#include "QF/sys.h"

#include "qw/include/cl_parse.h"
#include "qw/include/client.h"

/*
	Find closest color in the palette for named color
*/
static int
rgb_palette (int r, int g, int b, int bgr)
{
	float       bestdist, dist;
	int         r1, g1, b1, i;
	int         best = 0;
	static int  lastbest;
	static int  lr = -1, lg = -1, lb = -1;

	if (bgr) {
		int         t = r;
		r = b;
		b = t;
	}

	if (r == lr && g == lg && b == lb)
		return lastbest;

	bestdist = 256 * 256 * 3;

	for (i = 0; i < 256; i++) {
		int         j;
		j = i * 3;
		r1 = r_data->vid->palette[j] - r;
		g1 = r_data->vid->palette[j + 1] - g;
		b1 = r_data->vid->palette[j + 2] - b;
		dist = r1 * r1 + g1 * g1 + b1 * b1;
		if (dist < bestdist) {
			bestdist = dist;
			best = i;
		}
	}
	lr = r;
	lg = g;
	lb = b;
	lastbest = best;
	return best;
}

static void
snap_drawchar (int num, byte *dest, int width)
{
	byte       *source;
	int         col, row, drawline, x;

	row = num >> 4;
	col = num & 15;
	source = draw_chars + (row << 10) + (col << 3);

	drawline = 8;

	while (drawline--) {
		for (x = 0; x < 8; x++)
			if (source[x])
				dest[x] = source[x];
			else
				dest[x] = 98;
		source += 128;
		dest -= width;
	}

}

static void
snap_drawstring (const char *s, tex_t *tex, int x, int y)
{
	byte       *dest;
	byte       *buf = tex->data;
	const byte *p;
	int         width = tex->width;

	dest = buf + ((y * width) + x);

	p = (const byte *) s;
	while (*p) {
		snap_drawchar (*p++, dest, width);
		dest += 8;
	}
}

static tex_t *
cruch_snap (tex_t *snap, unsigned width, unsigned height)
{
	byte       *src, *dest;
	float       fracw, frach;
	unsigned    count, dex, dey, dx, dy, nx, r, g, b, x, y, w, h;
	tex_t      *tex;

	//FIXME casts
	w = ((unsigned)snap->width < width) ? (unsigned)snap->width : width;
	h = ((unsigned)snap->height < height) ? (unsigned)snap->height : height;

	fracw = (float) snap->width / (float) w;
	frach = (float) snap->height / (float) h;

	tex = malloc (sizeof (tex_t) + w * h);
	tex->data = (byte *) (tex + 1);
	if (!tex)
		return 0;

	tex->width = w;
	tex->height = h;
	tex->flagbits = 0;
	tex->loaded = 1;
	tex->flipped = snap->flipped;
	tex->palette = r_data->vid->palette;

	for (y = 0; y < h; y++) {
		dest = tex->data + (w * y);

		for (x = 0; x < w; x++) {
			r = g = b = 0;

			dx = x * fracw;
			dex = (x + 1) * fracw;
			if (dex == dx)
				dex++;					// at least one
			dy = y * frach;
			dey = (y + 1) * frach;
			if (dey == dy)
				dey++;					// at least one

			count = 0;
			for (; dy < dey; dy++) {
				src = snap->data + (snap->width * 3 * dy) + dx * 3;
				for (nx = dx; nx < dex; nx++) {
					r += *src++;
					g += *src++;
					b += *src++;
					count++;
				}
			}
			r /= count;
			g /= count;
			b /= count;
			*dest++ = rgb_palette (r, g, b, snap->bgr);
		}
	}
	free (snap);

	return tex;
}

static int snap_pending;

static void
snap_capture (tex_t *snap, void *data)
{
	int         pcx_len;
	pcx_t      *pcx = 0;
	tex_t      *tex = cruch_snap (snap, RSSHOT_WIDTH, RSSHOT_HEIGHT);

	snap_pending = 0;
	if (tex) {
		time_t      now;
		time (&now);
		dstring_t  *st = dstring_strdup (ctime (&now));
		dstring_snip (st, strlen (st->str) - 1, 1);
		snap_drawstring (st->str, tex, tex->width - strlen (st->str) * 8,
						 tex->height - 1);

		dstring_copystr (st, cls.servername->str);
		snap_drawstring (st->str, tex, tex->width - strlen (st->str) * 8,
						 tex->height - 11);

		dstring_copystr (st, cl_name);
		snap_drawstring (st->str, tex, tex->width - strlen (st->str) * 8,
						 tex->height - 21);

		pcx = EncodePCX (tex->data, tex->width, tex->height, tex->width,
						 r_data->vid->basepal, tex->flipped, &pcx_len);
		free (tex);
	}
	if (pcx) {
		CL_StartUpload ((void *)pcx, pcx_len);
		Sys_Printf ("Wrote %s\n", "rss.pcx");
		Sys_Printf ("Sending shot to server...\n");
	}
}

void
CL_RSShot_f (void)
{
	if (snap_pending || CL_IsUploading ())
		return;							// already one pending
	if (cls.state < ca_onserver)
		return;							// must be connected

	Sys_Printf ("Remote screen shot requested.\n");

	snap_pending = 1;
	r_funcs->capture_screen (snap_capture, 0);
}
