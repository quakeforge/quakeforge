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
blit_rgb (byte *buf, int count, byte red, byte green, byte blue, int dir)
{
	while (count--) {
		*buf = red;
		buf += dir;
		*buf = green;
		buf += dir;
		*buf = blue;
		buf += dir;
		*buf = 255;
		buf += dir;
	}
	return buf;
}

static inline byte *
blit_rgba (byte *buf, int count, byte red, byte green, byte blue, byte alpha,
		   int dir)
{
	while (count--) {
		*buf = red;
		buf += dir;
		*buf = green;
		buf += dir;
		*buf = blue;
		buf += dir;
		*buf = alpha;
		buf += dir;
	}
	return buf;
}

static inline byte *
read_bgr (byte *buf, int count, byte **data, int dir)
{
	byte		blue = *(*data)++;
	byte		green = *(*data)++;
	byte		red = *(*data)++;

	return blit_rgb (buf, count, red, green, blue, dir);
}

static inline byte *
read_bgra (byte *buf, int count, byte **data, int dir)
{
	byte		blue = *(*data)++;
	byte		green = *(*data)++;
	byte		red = *(*data)++;
	byte		alpha = *(*data)++;

	return blit_rgba (buf, count, red, green, blue, alpha, dir);
}

struct tex_s *
LoadTGA (QFile *fin)
{
	byte       *dataByte, *pixcol, *pixrow;
	int         column, row, columns, rows, numPixels, span, targa_mark;
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

	columns = targa->width;
	rows = targa->height;
	numPixels = columns * rows;

	switch (targa->pixel_size) {
		case 24:
			tex = Hunk_TempAlloc (field_offset (tex_t, data[numPixels * 4]));
			tex->format = tex_rgb;
			break;
		default:
		case 32:
			tex = Hunk_TempAlloc (field_offset (tex_t, data[numPixels * 4]));
			tex->format = tex_rgba;
			break;
	}

	tex->width = columns;
	tex->height = rows;
	tex->palette = 0;

	// skip TARGA image comment
	dataByte = (byte *) (targa + 1);
	dataByte += targa->id_length;

	span = columns * 4; // tex->format

	if (targa->image_type == 2) {	// Uncompressed image
		switch (targa->attributes & 48) {
		case 0:						// Origin at bottom left
			pixrow = tex->data + (rows - 1) * span;
			switch (targa->pixel_size) {
			case 24:
				for (row = rows - 1; row >= 0; row--, pixrow -= span) {
					pixcol = pixrow;
					for (column = 0; column < columns; column++)
						pixcol = read_bgr (pixcol, 1, &dataByte, 1);
				}
				break;
			case 32:
				for (row = rows - 1; row >= 0; row--, pixrow -= span) {
					pixcol = pixrow;
					for (column = 0; column < columns; column++)
						pixcol = read_bgra (pixcol, 1, &dataByte, 1);
				}
				break;
			}
			break;
		case 16:						// Origin at bottom right
			pixrow = tex->data + rows * span - 4;
			switch (targa->pixel_size) {
			case 24:
				for (row = rows - 1; row >= 0; row--, pixrow -= span) {
					pixcol = pixrow;
					for (column = columns - 1; column >= 0; column--)
						pixcol = read_bgr (pixcol, 1, &dataByte, -1);
				}
				break;
			case 32:
				for (row = rows - 1; row >= 0; row--, pixrow -= span) {
					pixcol = pixrow;
					for (column = columns - 1; column >= 0; column--)
						pixcol = read_bgra (pixcol, 1, &dataByte, -1);
				}
				break;
			}
			break;
		case 32:						// Origin at top left
			pixrow = tex->data;
			switch (targa->pixel_size) {
			case 24:
				for (row = 0; row < rows; row++, pixrow += span) {
					pixcol = pixrow;
					for (column = 0; column < columns; column++)
						pixcol = read_bgr (pixcol, 1, &dataByte, 1);
				}
				break;
			case 32:
				for (row = 0; row < rows; row++, pixrow += span) {
					pixcol = pixrow;
					for (column = 0; column < columns; column++)
						pixcol = read_bgra (pixcol, 1, &dataByte, 1);
				}
				break;
			}
			break;
		case 48:						// Origin at top right
			pixrow = tex->data + span - 4;
			switch (targa->pixel_size) {
			case 24:
				for (row = 0; row < rows; row++, pixrow += span) {
					pixcol = pixrow;
					for (column = columns - 1; column >= 0; column--)
						pixcol = read_bgr (pixcol, 1, &dataByte, -1);
				}
				break;
			case 32:
				for (row = 0; row < rows; row++, pixrow += span) {
					pixcol = pixrow;
					for (column = columns - 1; column >= 0; column--)
						pixcol = read_bgra (pixcol, 1, &dataByte, -1);
				}
				break;
			}
			break;
		}
	} else if (targa->image_type == 10) {	// RLE compressed image
		unsigned char packetHeader, packetSize;

		byte *(*expand) (byte *buf, int count, byte **data, int dir);

		pixrow = tex->data + (rows - 1) * span;

		if (targa->pixel_size == 24)
			expand = read_bgr;
		else
			expand = read_bgra;

		for (row = rows - 1; row >= 0; row--, pixrow -= span) {
			pixcol = pixrow;
			for (column = 0; column < columns;) {
				packetHeader = *dataByte++;
				packetSize = 1 + (packetHeader & 0x7f);
				while (packetSize > columns - column) {
					int count = columns - column;

					packetSize -= count;
					if (packetHeader & 0x80) {		// run-length packet
						expand (pixcol, count, &dataByte, 1);
					} else {						// non run-length packet
						while (count--)
							expand (pixcol, 1, &dataByte, 1);
					}
					column = 0;
					pixcol = (pixrow -= span);
					if (--row < 0)
						goto done;
				}
				column += packetSize;
				if (packetHeader & 0x80) {			// run-length packet
					pixcol = expand (pixcol, packetSize, &dataByte, 1);
				} else {							// non run-length packet
					while (packetSize--)
						pixcol = expand (pixcol, 1, &dataByte, 1);
				}
			}
		}
	done:;
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
