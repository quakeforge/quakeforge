/*
	tga.c

	targa image handling

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

#include <stdlib.h>

#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/texture.h"
#include "QF/tga.h"
#include "QF/zone.h"

#include "compat.h"


static inline byte *
blit_rgb (byte *buf, int count, byte red, byte green, byte blue)
{
	while (count--) {
		*buf++ = red;
		*buf++ = green;
		*buf++ = blue;
		*buf++ = 255;
	}
	return buf;
}

static inline byte *
blit_rgba (byte *buf, int count, byte red, byte green, byte blue, byte alpha)
{
	while (count--) {
		*buf++ = red;
		*buf++ = green;
		*buf++ = blue;
		*buf++ = alpha;
	}
	return buf;
}

static inline byte *
reverse_blit_rgb (byte *buf, int count, byte red, byte green, byte blue)
{
	while (count--) {
		*buf-- = 255;
		*buf-- = blue;
		*buf-- = green;
		*buf-- = red;
	}
	return buf;
}

static inline byte *
reverse_blit_rgba (byte *buf, int count, byte red, byte green, byte blue,
				   byte alpha)
{
	while (count--) {
		*buf-- = alpha;
		*buf-- = blue;
		*buf-- = green;
		*buf-- = red;
	}
	return buf;
}

static inline byte *
read_bgr (byte *buf, int count, byte **data)
{
	byte		blue = *(*data)++;
	byte		green = *(*data)++;
	byte		red = *(*data)++;

	return blit_rgb (buf, count, red, green, blue);
}

static inline byte *
read_bgra (byte *buf, int count, byte **data)
{
	byte		blue = *(*data)++;
	byte		green = *(*data)++;
	byte		red = *(*data)++;
	byte		alpha = *(*data)++;

	return blit_rgba (buf, count, red, green, blue, alpha);
}

static inline byte *
reverse_read_bgr (byte *buf, int count, byte **data)
{
	byte		blue = *(*data)++;
	byte		green = *(*data)++;
	byte		red = *(*data)++;

	return reverse_blit_rgb (buf, count, red, green, blue);
}

static inline byte *
reverse_read_bgra (byte *buf, int count, byte **data)
{
	byte		blue = *(*data)++;
	byte		green = *(*data)++;
	byte		red = *(*data)++;
	byte		alpha = *(*data)++;

	return reverse_blit_rgba (buf, count, red, green, blue, alpha);
}

static inline void
setup_pixrow_span (TargaHeader *targa, tex_t *tex, byte **_pixrow, int *_span)
{
	byte       *pixrow;
	int         span;

	span = targa->width * 4;
	pixrow = tex->data;
	if (targa->attributes & 0x10) {
		// right to left
		pixrow += span - 4;
	}
	if (!(targa->attributes & 0x20)) {
		// bottom to top
		pixrow += (targa->height - 1) * span;
		span = -span;
	}

	*_pixrow = pixrow;
	*_span = span;
}

static void
decode_truecolor_24 (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	int         column, columns, rows, span;

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span);

	if (targa->attributes & 0x10) {
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = reverse_read_bgr (pixcol, 1, &dataByte);
			pixrow += span;
		}
	} else {
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = read_bgr (pixcol, 1, &dataByte);
			pixrow += span;
		}
	}
}

static void
decode_truecolor_32 (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	int         column, columns, rows, span;

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span);

	if (targa->attributes & 0x10) {
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = reverse_read_bgra (pixcol, 1, &dataByte);
			pixrow += span;
		}
	} else {
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = 0; column < columns; column++)
				pixcol = read_bgra (pixcol, 1, &dataByte);
			pixrow += span;
		}
	}
}

#define rle_expand(expand)                                                    \
do {                                                                          \
	unsigned char packetHeader, packetSize;                                   \
	while (rows-- > 0) {                                                      \
		pixcol = pixrow;                                                      \
		pixrow += span;                                                       \
		for (column = columns; column > 0; ) {                                \
			packetHeader = *dataByte++;                                       \
			packetSize = 1 + (packetHeader & 0x7f);                           \
			while (packetSize > column) {                                     \
				int count = column;                                           \
                                                                              \
				packetSize -= count;                                          \
				if (packetHeader & 0x80) {		/* run-length packet */       \
					expand (pixcol, count, &dataByte);                        \
				} else {						/* non run-length packet */   \
					while (count--)                                           \
						expand (pixcol, 1, &dataByte);                        \
				}                                                             \
				column = columns;                                             \
				pixcol = pixrow;                                              \
				pixrow += span;                                               \
				if (rows-- <= 0)                                              \
					goto done_##expand;                                        \
			}                                                                 \
			column -= packetSize;                                             \
			if (packetHeader & 0x80) {			/* run-length packet */       \
				pixcol = expand (pixcol, packetSize, &dataByte);              \
			} else {							/* non run-length packet */   \
				while (packetSize--)                                          \
					pixcol = expand (pixcol, 1, &dataByte);                   \
			}                                                                 \
		}                                                                     \
	}                                                                         \
	done_##expand:;                                                            \
} while (0)

static void
decode_truecolor_24_rle (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	int         column, columns, rows, span;

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span);

	if (targa->attributes & 0x10)
		rle_expand (reverse_read_bgr);
	else
		rle_expand (read_bgr);
}

static void
decode_truecolor_32_rle (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	int         column, columns, rows, span;

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span);

	if (targa->attributes & 0x10)
		rle_expand (reverse_read_bgr);
	else
		rle_expand (read_bgr);
}

struct tex_s *
LoadTGA (QFile *fin)
{
	byte       *dataByte;
	int         numPixels, targa_mark;
	TargaHeader *targa;
	tex_t      *tex;

	targa_mark = Hunk_LowMark ();
	targa = Hunk_AllocName (qfs_filesize, "TGA");
	Qread (fin, targa, qfs_filesize);
	
	targa->colormap_index = LittleShort (targa->colormap_index);
	targa->colormap_length = LittleShort (targa->colormap_length);
	targa->x_origin = LittleShort (targa->x_origin);
	targa->y_origin = LittleShort (targa->y_origin);
	targa->width = LittleShort (targa->width);
	targa->height = LittleShort (targa->height);

	if (targa->image_type != 2 && targa->image_type != 10)
		Sys_Error ("LoadTGA: Only type 2 and 10 targa RGB images supported");

	if (targa->colormap_type != 0
		|| (targa->pixel_size != 32 && targa->pixel_size != 24))
		Sys_Error ("Texture_LoadTGA: Only 32 or 24 bit images supported "
				   "(no colormaps)");

	numPixels = targa->width * targa->height;

	tex = Hunk_TempAlloc (field_offset (tex_t, data[numPixels * 4]));
	tex->width = targa->width;
	tex->height = targa->height;
	tex->palette = 0;

	switch (targa->pixel_size) {
		case 24:
			tex->format = tex_rgb;
			break;
		default:
		case 32:
			tex->format = tex_rgba;
			break;
	}

	// skip TARGA image comment
	dataByte = (byte *) (targa + 1);
	dataByte += targa->id_length;

	if (targa->image_type == 2) {	// Uncompressed image
		switch (targa->pixel_size) {
		case 24:
			decode_truecolor_24 (targa, tex, dataByte);
			break;
		case 32:
			decode_truecolor_32 (targa, tex, dataByte);
			break;
		}
	} else if (targa->image_type == 10) {	// RLE compressed image
		switch (targa->pixel_size) {
		case 24:
			decode_truecolor_24_rle (targa, tex, dataByte);
			break;
		case 32:
			decode_truecolor_32_rle (targa, tex, dataByte);
			break;
		}
	}

	Hunk_FreeToLowMark (targa_mark);
	return tex;
}

void
WriteTGAfile (const char *tganame, byte *data, int width, int height)
{
	TargaHeader header;

	memset (&header, 0, sizeof (header));
	header.image_type = 2;				// uncompressed type
	header.width = LittleShort (width);
	header.height = LittleShort (height);
	header.pixel_size = 24;

	QFS_WriteBuffers (tganame, 2, &header, sizeof (header), data,
					  width * height * 3);
}
