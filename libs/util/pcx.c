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

	$Id$
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

#include "QF/console.h"
#include "QF/pcx.h"
#include "QF/qendian.h"
#include "QF/qtypes.h"
#include "QF/texture.h"
#include "QF/vfs.h"
#include "QF/zone.h"

#include "compat.h"


tex_t *
LoadPCX (VFile *f, int convert, byte *pal)
{
	pcx_t      *pcx;
	int         pcx_mark;
	byte       *palette;
	byte       *pix;
	byte       *dataByte;
	int         x, y;
	int         runLength = 1;
	int         count;
	tex_t      *tex;

	// parse the PCX file
	pcx_mark = Hunk_LowMark ();
	pcx = Hunk_AllocName (com_filesize, "PCX");
	Qread (f, pcx, com_filesize);

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
		|| pcx->bits_per_pixel != 8 || pcx->xmax >= 320 || pcx->ymax >= 256) {
		Con_Printf ("Bad pcx file\n");
		return 0;
	}

	palette = ((byte*)pcx) + com_filesize - 768;
	dataByte = (byte*)&pcx[1];

	count = (pcx->xmax + 1) * (pcx->ymax + 1);
	if (convert)
		count *= 4;
	tex = Hunk_TempAlloc (field_offset (tex_t, data[count]));
	tex->width = pcx->xmax + 1;
	tex->height = pcx->ymax + 1;
	if (convert) {
		tex->palette = 0;
	} else {
		tex->palette = pal;
	}
	pix = tex->data;

	for (y = 0; y < tex->height; y++) {
		for (x = 0; x < tex->width;) {
			runLength = 1;
			if (dataByte >= palette)
				break;

			if ((*dataByte & 0xC0) == 0xC0) {
				runLength = *dataByte++ & 0x3F;
				if (dataByte >= palette)
					break;
			}

			if (convert) {
				while (count && runLength > 0) {
					pix[0] = palette[*dataByte * 3];
					pix[1] = palette[*dataByte * 3 + 1];
					pix[2] = palette[*dataByte * 3 + 2];
					pix[3] = 255;
					pix += 4;
					count -= 4;
					runLength--;
					x++;
				}
			} else {
				while (count && runLength > 0) {
					*pix++ = *dataByte;
					count--;
					runLength--;
					x++;
				}
			}
			if (runLength)
				break;
			dataByte++;
		}
		if (runLength)
			break;
	}
	Hunk_FreeToLowMark (pcx_mark);
	if (count || runLength) {
		Con_Printf ("PCX was malformed. You should delete it.\n");
		return 0;
	}
	return tex;
}

pcx_t *
EncodePCX (byte * data, int width, int height,
		   int rowbytes, byte * palette, qboolean flip, int *length)
{
	int 	i, j;
	pcx_t	*pcx;
	byte	*pack;

	if (!(pcx = Hunk_TempAlloc (width * height * 2 + 1000))) {
		Con_Printf ("EncodePCX: not enough memory\n");
		return 0;
	}

	pcx->manufacturer = 0x0a;			// PCX id
	pcx->version = 5;					// 256 color
	pcx->encoding = 1;					// uncompressed
	pcx->bits_per_pixel = 8;			// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort ((short) (width - 1));
	pcx->ymax = LittleShort ((short) (height - 1));
	pcx->hres = LittleShort ((short) width);
	pcx->vres = LittleShort ((short) height);
	memset (pcx->palette, 0, sizeof (pcx->palette));
	pcx->color_planes = 1;				// chunky image
	pcx->bytes_per_line = LittleShort ((short) width);
	pcx->palette_type = LittleShort (2);	// not a grey scale
	memset (pcx->filler, 0, sizeof (pcx->filler));

	// pack the image
	pack = (byte*)&pcx[1];

	if (flip)
		data += rowbytes * (height - 1);

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			if ((*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else {
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}

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
