/*
	vid_common_sw.c

	general video driver functions

	Copyright (C) 1996-1997 Id Software, Inc.

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

#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "compat.h"
#include "r_internal.h"
#include "vid_internal.h"

unsigned short d_8to16table[256];


/*
	VID_MakeColormap32

	LordHavoc: makes a 32bit color*light table, RGBA order, no endian,
	           may need to be re-ordered to hardware at display time
*/
static void
VID_MakeColormap32 (void *outcolormap, byte *pal)
{
	int c, l;
	byte *out;
	out = (byte *)&d_8to24table;

	/*
	 * Generates colors not affected by lighting, such as
	 * HUD pieces and general sprites (such as explosions)
	 */
	for (c = 0; c < 256; c++) {
		*out++ = pal[c*3+2];
		*out++ = pal[c*3+1];
		*out++ = pal[c*3+0];
		*out++ = 255;
	}
	d_8to24table[255] = 0;				// 255 is transparent
	out = (byte *) outcolormap;

	/*
	 * Generates colors affected by lighting, such as the
	 * world and other models that give it life, like foes and pickups.
	 */
	for (l = 0;l < VID_GRADES;l++)
	{
		for (c = 0;c < vid.fullbright;c++)
		{
			out[(l*256+c)*4+0] = bound(0, (pal[c*3+2] * l) >> (VID_CBITS - 1),
									   255);
			out[(l*256+c)*4+1] = bound(0, (pal[c*3+1] * l) >> (VID_CBITS - 1),
									   255);
			out[(l*256+c)*4+2] = bound(0, (pal[c*3+0] * l) >> (VID_CBITS - 1),
									   255);
			out[(l*256+c)*4+3] = 255;
		}
		for (;c < 255;c++)
		{
			out[(l*256+c)*4+0] = pal[c*3+2];
			out[(l*256+c)*4+1] = pal[c*3+1];
			out[(l*256+c)*4+2] = pal[c*3+0];
			out[(l*256+c)*4+3] = 255;
		}
		out[(l*256+255)*4+0] = 0;
		out[(l*256+255)*4+1] = 0;
		out[(l*256+255)*4+2] = 0;
		out[(l*256+255)*4+3] = 0;
	}
}

static unsigned short
lh24to16bit (int red, int green, int blue)
{
	red = bound(0, red, 255);
	green = bound(0, green, 255);
	blue = bound(0, blue, 255);
	red >>= 3;
	green >>= 2;
	blue >>= 3;
	red <<= 11;
	green <<= 5;
	return (unsigned short) (red | green | blue);
}

/*
	VID_MakeColormap16

	LordHavoc: makes a 16bit color*light table, RGB order, native endian,
	           may need to be translated to hardware order at display time
*/
static void
VID_MakeColormap16 (void *outcolormap, byte *pal)
{
	int c, l;
	unsigned short *out;
	out = (unsigned short *)&d_8to16table;
	for (c = 0; c < 256; c++)
		*out++ = lh24to16bit(pal[c*3+0], pal[c*3+1], pal[c*3+2]);
	d_8to16table[255] = 0;				// 255 is transparent
	out = (unsigned short *) outcolormap;
	for (l = 0;l < VID_GRADES;l++)
	{
		for (c = 0;c < vid.fullbright;c++)
			out[l*256+c] = lh24to16bit(
			(pal[c*3+0] * l) >> (VID_CBITS - 1),
			(pal[c*3+1] * l) >> (VID_CBITS - 1),
			(pal[c*3+2] * l) >> (VID_CBITS - 1));
		for (;c < 255;c++)
			out[l*256+c] = lh24to16bit(pal[c*3+0], pal[c*3+1], pal[c*3+2]);
		out[l*256+255] = 0;
	}
}

/*
	VID_MakeColormaps

	LordHavoc: makes 8bit, 16bit, and 32bit colormaps and palettes
*/
void
VID_MakeColormaps (void)
{
	vid.colormap16 = malloc (256*VID_GRADES * sizeof (unsigned short));
	vid.colormap32 = malloc (256*VID_GRADES * sizeof (unsigned int));
	SYS_CHECKMEM (vid.colormap16 && vid.colormap32);
	VID_MakeColormap16(vid.colormap16, vid.palette);
	VID_MakeColormap32(vid.colormap32, vid.palette);
}
