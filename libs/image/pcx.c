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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

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


tex_t *
LoadPCX (QFile *f, int convert, byte *pal)
{
	pcx_t      *pcx;
	int         pcx_mark;
	byte       *palette;
	byte       *pix;
	byte       *dataByte;
	int         runLength = 1;
	int         count;
	tex_t      *tex;

	// parse the PCX file
	pcx_mark = Hunk_LowMark ();
	pcx = Hunk_AllocName (qfs_filesize, "PCX");
	Qread (f, pcx, qfs_filesize);

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
		Sys_Printf ("Bad pcx file\n");
		return 0;
	}

	palette = ((byte *) pcx) + qfs_filesize - 768;
	dataByte = (byte *) &pcx[1];

	count = (pcx->xmax + 1) * (pcx->ymax + 1);
	if (convert) {
		tex = Hunk_TempAlloc (field_offset (tex_t, data[count * 3]));
		tex->format = tex_rgb;
		tex->palette = 0;
	} else {
		tex = Hunk_TempAlloc (field_offset (tex_t, data[count]));
		tex->format = tex_palette;
		if (pal)
			tex->palette = pal;
		else
			tex->palette = palette;
	}
	tex->width = pcx->xmax + 1;
	tex->height = pcx->ymax + 1;
	pix = tex->data;

	while (count) {
		if (dataByte >= palette)
			break;

		if ((*dataByte & 0xC0) == 0xC0) {
			runLength = *dataByte++ & 0x3F;
			if (dataByte >= palette)
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
	Hunk_FreeToLowMark (pcx_mark);
	if (count || runLength) {
		Sys_Printf ("PCX was malformed. You should delete it.\n");
		return 0;
	}
	return tex;
}

pcx_t *
EncodePCX (byte * data, int width, int height,
		   int rowbytes, byte * palette, qboolean flip, int *length)
{
	int 	i, j, size;
	pcx_t	*pcx;
	byte	*pack;

	size = width * height * 2 + 1000;
	if (!(pcx = Hunk_TempAlloc (size))) {
		Sys_Printf ("EncodePCX: not enough memory\n");
		return 0;
	}
	memset (pcx, 0, size);

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
	pcx->color_planes = 1;				// chunky image
	pcx->bytes_per_line = LittleShort ((short) width);
	pcx->palette_type = LittleShort (2);	// not a grey scale

	// pack the image
	pack = (byte *) &pcx[1];

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
