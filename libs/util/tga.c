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
static const char rcsid[] = 
	"$Id$";

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

#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/texture.h"
#include "QF/tga.h"
#include "QF/vfs.h"

#include "compat.h"


static int
fgetLittleShort (VFile *f)
{
	byte        b1, b2;

	b1 = Qgetc (f);
	b2 = Qgetc (f);

	return (short) (b1 + b2 * 256);
}

/*
static int
fgetLittleLong (VFile *f)
{
	byte	b1, b2, b3, b4;

	b1 = Qgetc(f);
	b2 = Qgetc(f);
	b3 = Qgetc(f);
	b4 = Qgetc(f);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}
*/

struct tex_s *
LoadTGA (VFile *fin)
{
	byte		   *pixbuf;
	byte			red = 0, green = 0, blue = 0, alphabyte = 0;
	int				column, row, columns, rows, numPixels;

	TargaHeader		targa_header;
	tex_t		   *targa_data;

	targa_header.id_length = Qgetc (fin);
	targa_header.colormap_type = Qgetc (fin);
	targa_header.image_type = Qgetc (fin);

	targa_header.colormap_index = fgetLittleShort (fin);
	targa_header.colormap_length = fgetLittleShort (fin);
	targa_header.colormap_size = Qgetc (fin);
	targa_header.x_origin = fgetLittleShort (fin);
	targa_header.y_origin = fgetLittleShort (fin);
	targa_header.width = fgetLittleShort (fin);
	targa_header.height = fgetLittleShort (fin);
	targa_header.pixel_size = Qgetc (fin);
	targa_header.attributes = Qgetc (fin);

	if (targa_header.image_type != 2 && targa_header.image_type != 10)
		Sys_Error ("LoadTGA: Only type 2 and 10 targa RGB images supported\n");

	if (targa_header.colormap_type != 0
		|| (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
		Sys_Error ("Texture_LoadTGA: Only 32 or 24 bit images supported "
				   "(no colormaps)\n");

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	switch (targa_header.pixel_size) {
		case 24:
			targa_data = malloc
				(field_offset (tex_t, data[numPixels * 3]));
			if (!targa_data)
				Sys_Error ("LoadTGA: Memory Allocation Failure\n");
			targa_data->format = tex_rgb;
			break;
		default:
		case 32:
			targa_data = malloc
				(field_offset (tex_t, data[numPixels * 4]));
			if (!targa_data)
				Sys_Error ("LoadTGA: Memory Allocation Failure\n");
			targa_data->format = tex_rgba;
			break;
	}

	targa_data->width = columns;
	targa_data->height = rows;
	targa_data->palette = 0;

	if (targa_header.id_length != 0)
		Qseek (fin, targa_header.id_length, SEEK_CUR);	// skip TARGA image
														// comment

	if (targa_header.image_type == 2) {	// Uncompressed image
		switch (targa_header.pixel_size) {
			case 24:
				for (row = rows - 1; row >= 0; row--) {
					pixbuf = targa_data->data + row * columns *
						targa_data->format;
					for (column = 0; column < columns; column++) {
						blue = Qgetc (fin);
						green = Qgetc (fin);
						red = Qgetc (fin);
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
					}
				}
				break;
			case 32:
				for (row = rows - 1; row >= 0; row--) {
					pixbuf = targa_data->data + row * columns *
						targa_data->format;
					for (column = 0; column < columns; column++) {
						blue = Qgetc (fin);
						green = Qgetc (fin);
						red = Qgetc (fin);
						alphabyte = Qgetc (fin);
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
					}
				}
				break;
		}
	} else if (targa_header.image_type == 10) {	// RLE compressed image
		unsigned char packetHeader, packetSize, j;

		for (row = rows - 1; row >= 0; row--) {
			pixbuf = targa_data->data + row * columns * targa_data->format;
			for (column = 0; column < columns;) {
				packetHeader = Qgetc (fin);
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {	// run-length packet
					switch (targa_header.pixel_size) {
						case 24:
							blue = Qgetc (fin);
							green = Qgetc (fin);
							red = Qgetc (fin);
							break;
						case 32:
							blue = Qgetc (fin);
							green = Qgetc (fin);
							red = Qgetc (fin);
							alphabyte = Qgetc (fin);
							break;
					}

					for (j = 0; j < packetSize; j++) {
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						if (targa_header.pixel_size > 24)
							*pixbuf++ = alphabyte;
						column++;
						if (column == columns) {	// run spans across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_data->data + row * columns *
								targa_data->format;
						}
					}
				} else {				// non run-length packet
					for (j = 0; j < packetSize; j++) {
						switch (targa_header.pixel_size) {
							case 24:
								blue = Qgetc (fin);
								green = Qgetc (fin);
								red = Qgetc (fin);
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								break;
							case 32:
								blue = Qgetc (fin);
								green = Qgetc (fin);
								red = Qgetc (fin);
								alphabyte = Qgetc (fin);
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = alphabyte;
								break;
						}
						column++;
						if (column == columns) {	// pixel packet run spans
													// across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_data->data + row * columns *
								targa_data->format;
						}
					}
				}
			}
		breakOut:;
		}
	}

	Qclose (fin);
	return targa_data;
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

	COM_WriteBuffers (tganame, 2, &header, sizeof (header), data,
					  width * height * 3);
}
