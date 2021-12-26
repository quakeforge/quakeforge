/*
	pcx.c

	pcx image handling

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

#include "QF/image.h"
#include "QF/pcx.h"
#include "QF/qendian.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"


VISIBLE tex_t *
LoadPCX (QFile *f, qboolean convert, const byte *pal, int load)
{
	pcx_t      *pcx;
	size_t      pcx_mark;
	byte       *palette;
	byte       *end;
	byte       *pix;
	byte       *dataByte;
	int         runLength = 1;
	int         count;
	tex_t      *tex;
	int			fsize = sizeof (pcx_t);

	if (load) {
		fsize = Qfilesize(f);
	}
	// parse the PCX file
	pcx_mark = Hunk_LowMark (0);
	pcx = Hunk_AllocName (0, fsize, "PCX");
	Qread (f, pcx, fsize);

	pcx->xmax = LittleShort (pcx->xmax);
	pcx->xmin = LittleShort (pcx->xmin);
	pcx->ymax = LittleShort (pcx->ymax);
	pcx->ymin = LittleShort (pcx->ymin);
	pcx->hres = LittleShort (pcx->hres);
	pcx->vres = LittleShort (pcx->vres);
	pcx->bytes_per_line = LittleShort (pcx->bytes_per_line);
	pcx->palette_type = LittleShort (pcx->palette_type);

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8) {
		Sys_Printf ("Bad pcx file: %x %d %d %d\n",
					pcx->manufacturer, pcx->version, pcx->encoding,
					pcx->bits_per_pixel);
		Hunk_FreeToLowMark (0, pcx_mark);
		return 0;
	}

	end = palette = ((byte *) pcx) + fsize - 768;
	dataByte = (byte *) &pcx[1];

	count = load ? (pcx->xmax + 1) * (pcx->ymax + 1) : 0;
	if (convert) {
		tex = Hunk_TempAlloc (0, sizeof (tex_t) + count * 3);
		tex->data = (byte *) (tex + 1);
		tex->format = tex_rgb;
		tex->palette = 0;
	} else {
		tex = Hunk_TempAlloc (0, sizeof (tex_t) + count);
		tex->data = (byte *) (tex + 1);
		tex->format = tex_palette;
		if (pal)
			tex->palette = pal;
		else
			tex->palette = palette;
	}
	tex->width = pcx->xmax + 1;
	tex->height = pcx->ymax + 1;
	tex->loaded = load;
	if (!load) {
		Hunk_FreeToLowMark (0, pcx_mark);
		return tex;
	}
	pix = tex->data;

	while (count) {
		if (dataByte >= end)
			break;

		if ((*dataByte & 0xC0) == 0xC0) {
			runLength = *dataByte++ & 0x3F;
			if (dataByte >= end)
				break;
		} else {
			runLength = 1;
		}

		if (runLength > count)
			runLength = count;
		count -= runLength;

		if (convert) {
			for (; runLength > 0; runLength--) {
				*pix++ = palette[*dataByte * 3];
				*pix++ = palette[*dataByte * 3 + 1];
				*pix++ = palette[*dataByte * 3 + 2];
			}
		} else {
			for (; runLength > 0; runLength--) {
				*pix++ = *dataByte;
			}
		}
		dataByte++;
	}
	Hunk_FreeToLowMark (0, pcx_mark);
	if (count || runLength) {
		Sys_Printf ("PCX was malformed. You should delete it.\n");
		return 0;
	}
	return tex;
}

VISIBLE pcx_t *
EncodePCX (const byte *data, int width, int height,
		   int rowbytes, const byte *palette, qboolean flip, int *length)
{
	int         i, run, pix, size;
	pcx_t      *pcx;
	byte       *pack;
	const byte *dataend;

	size = width * height * 2 + 1000;
	if (!(pcx = Hunk_TempAlloc (0, size))) {
		Sys_Printf ("EncodePCX: not enough memory\n");
		return 0;
	}
	memset (pcx, 0, size);

	pcx->manufacturer = 0x0a;			// PCX id
	pcx->version = 5;					// 256 color
	pcx->encoding = 1;					// RLE
	pcx->bits_per_pixel = 8;			// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort ((short) (width - 1));
	pcx->ymax = LittleShort ((short) (height - 1));
	pcx->hres = LittleShort ((short) width);
	pcx->vres = LittleShort ((short) height);
	pcx->color_planes = 1;				// chunky image
	pcx->bytes_per_line = LittleShort ((short) width);
	pcx->palette_type = LittleShort (1);	// not a grey scale

	// pack the image
	pack = (byte *) &pcx[1];

	if (flip)
		data += rowbytes * (height - 1);

	for (i = 0; i < height; i++) {
		// from darkplaces lmp2pcx
		for (dataend = data + width; data < dataend; /* empty */) {
			for (pix = *data++, run = 0xC1;
				 data < dataend && run < 0xFF && *data == pix;
				 data++, run++)
				/* empty */;
			if (run > 0xC1 || pix >= 0xC0)
				*pack++ = run;
			*pack++ = pix;
		}

		if (width & 1)
			*pack++ = 0;

		data += rowbytes - width;
		if (flip)
			data -= rowbytes * 2;
	}

	// write the palette
	*pack++ = 0x0c;						// palette ID byte
	for (i = 0; i < 768; i++)
		*pack++ = *palette++;

	// write output file
	*length = pack - (byte *) pcx;
	return pcx;
}
