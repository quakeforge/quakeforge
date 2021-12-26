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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/image.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/tga.h"
#include "QF/zone.h"
#include "QF/GL/defines.h"

#include "compat.h"

typedef struct {
	byte        red;
	byte        green;
	byte        blue;
	byte        alpha;
} cmap_t;

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
blit_l (byte *buf, int count, byte lum)
{
	while (count--) {
		*buf++ = lum;
	}
	return buf;
}

static inline byte *
reverse_blit_l (byte *buf, int count, byte lum)
{
	while (count--) {
		*buf-- = lum;
	}
	return buf;
}

static inline byte *
read_bgr (byte *buf, int count, byte **data, cmap_t *cmap)
{
	byte		blue = *(*data)++;
	byte		green = *(*data)++;
	byte		red = *(*data)++;

	return blit_rgb (buf, count, red, green, blue);
}

static inline byte *
read_bgra (byte *buf, int count, byte **data, cmap_t *cmap)
{
	byte		blue = *(*data)++;
	byte		green = *(*data)++;
	byte		red = *(*data)++;
	byte		alpha = *(*data)++;

	return blit_rgba (buf, count, red, green, blue, alpha);
}

static inline byte *
reverse_read_bgr (byte *buf, int count, byte **data, cmap_t *cmap)
{
	byte		blue = *(*data)++;
	byte		green = *(*data)++;
	byte		red = *(*data)++;

	return reverse_blit_rgb (buf, count, red, green, blue);
}

static inline byte *
reverse_read_bgra (byte *buf, int count, byte **data, cmap_t *cmap)
{
	byte		blue = *(*data)++;
	byte		green = *(*data)++;
	byte		red = *(*data)++;
	byte		alpha = *(*data)++;

	return reverse_blit_rgba (buf, count, red, green, blue, alpha);
}

static inline byte *
read_cmap (byte *buf, int count, byte **data, cmap_t *cmap)
{
	byte        ind = *(*data)++;
	byte		red = cmap[ind].red;
	byte		green = cmap[ind].green;
	byte		blue = cmap[ind].blue;
	byte		alpha = cmap[ind].alpha;

	return blit_rgba (buf, count, red, green, blue, alpha);
}

static inline byte *
reverse_read_cmap (byte *buf, int count, byte **data, cmap_t *cmap)
{
	byte        ind = *(*data)++;
	byte		red = cmap[ind].red;
	byte		green = cmap[ind].green;
	byte		blue = cmap[ind].blue;
	byte		alpha = cmap[ind].alpha;

	return reverse_blit_rgba (buf, count, red, green, blue, alpha);
}

static inline byte *
read_l (byte *buf, int count, byte **data, cmap_t *cmap)
{
	byte        lum = *(*data)++;

	return blit_l (buf, count, lum);
}

static inline byte *
reverse_read_l (byte *buf, int count, byte **data, cmap_t *cmap)
{
	byte        lum = *(*data)++;

	return reverse_blit_l (buf, count, lum);
}

static inline void
setup_pixrow_span (TargaHeader *targa, tex_t *tex, byte **_pixrow, int *_span,
				   int pixbytes)
{
	byte       *pixrow;
	int         span;

	span = targa->width * pixbytes;
	pixrow = tex->data;
	if (targa->attributes & 0x10) {
		// right to left
		pixrow += span - pixbytes;
	}
	if (!(targa->attributes & 0x20)) {
		// bottom to top
		pixrow += (targa->height - 1) * span;
		span = -span;
	}

	*_pixrow = pixrow;
	*_span = span;
}

static byte *
skip_colormap (TargaHeader *targa, byte *data)
{
	int         bpe;
	if (!targa->colormap_type)
		return data;
	Sys_MaskPrintf (SYS_dev, "LoadTGA: skipping colormap\n");
	bpe = (targa->pixel_size +7) / 8;
	return data + bpe * targa->colormap_length;
}

static cmap_t *
parse_colormap (TargaHeader *targa, byte **dataByte)
{
	byte       *data;
	cmap_t     *cm, *cmap;
	int         i;
	unsigned short word;

	if (!targa->colormap_type)
		Sys_Error ("LoadTGA: colormap missing");
	if (targa->colormap_type != 1)
		Sys_Error ("LoadTGA: unkown colormap type");
	if (targa->colormap_index + targa->colormap_length > 256)
		Sys_Error ("LoadTGA: unsupported color map size");
	if (targa->pixel_size != 8)
		Sys_Error ("LoadTGA: unsupported color map index bits");

	switch (targa->colormap_size) {
		case 32:
			if (!targa->colormap_index && targa->colormap_length == 256) {
				cmap = (cmap_t *) *dataByte;
				*dataByte += 256 * sizeof (cmap_t);
				return cmap;
			}
		case 24:
		case 16:
		case 15:
			cmap = Hunk_AllocName (0, 256 * sizeof (cmap_t), "TGA cmap");
			break;
		default:
			Sys_Error ("LoadTGA: unsupported color map size");
			break;
	}

	data = *dataByte;
	cm = cmap + targa->colormap_index;

	switch (targa->colormap_size) {
		case 15:
			for (i = 0; i < targa->colormap_length; i++, cm++) {
				word = *data++;
				word |= (*data++) << 8;
				cm->red   = (word << 3) & 0xf8;
				cm->green = (word >> 2) & 0xf8;
				cm->blue  = (word >> 7) & 0xf8;
				cm->alpha = 255;
			}
			break;
		case 16:
			for (i = 0; i < targa->colormap_length; i++, cm++) {
				word = *data++;
				word |= (*data++) << 8;
				cm->red   = (word << 3) & 0xf8;
				cm->green = (word >> 2) & 0xf8;
				cm->blue  = (word >> 7) & 0xf8;
				cm->alpha = (word & 0x8000) ? 255 : 0;
			}
			break;
		case 24:
			for (i = 0; i < targa->colormap_length; i++, cm++) {
				cm->red   = *data++;
				cm->green = *data++;
				cm->blue  = *data++;
				cm->alpha = 255;
			}
			break;
		case 32:
			for (i = 0; i < targa->colormap_length; i++, cm++) {
				cm->red   = *data++;
				cm->green = *data++;
				cm->blue  = *data++;
				cm->alpha = *data++;
			}
			break;
	}
	return cmap;
}

static void
decode_colormap (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	cmap_t     *cmap;
	int         column, columns, rows, span;

	cmap = parse_colormap (targa, &dataByte);

	tex->format = tex_rgba;

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span, 4);

	if (targa->attributes & 0x10) {
		// right to left
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = reverse_read_cmap (pixcol, 1, &dataByte, cmap);
			pixrow += span;
		}
	} else {
		// left to right
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = read_cmap (pixcol, 1, &dataByte, cmap);
			pixrow += span;
		}
	}
}

static void
decode_truecolor_24 (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	int         column, columns, rows, span;

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span, 4);

	if (targa->attributes & 0x10) {
		// right to left
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = reverse_read_bgr (pixcol, 1, &dataByte, 0);
			pixrow += span;
		}
	} else {
		// left to right
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = read_bgr (pixcol, 1, &dataByte, 0);
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

	setup_pixrow_span (targa, tex, &pixrow, &span, 4);

	if (targa->attributes & 0x10) {
		// right to left
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = reverse_read_bgra (pixcol, 1, &dataByte, 0);
			pixrow += span;
		}
	} else {
		// left to right
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = read_bgra (pixcol, 1, &dataByte, 0);
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
					expand (pixcol, count, &dataByte, cmap);                  \
				} else {						/* non run-length packet */   \
					while (count--)                                           \
						expand (pixcol, 1, &dataByte, cmap);                  \
				}                                                             \
				column = columns;                                             \
				pixcol = pixrow;                                              \
				pixrow += span;                                               \
				if (rows-- <= 0)                                              \
					goto done_##expand;                                       \
			}                                                                 \
			column -= packetSize;                                             \
			if (packetHeader & 0x80) {			/* run-length packet */       \
				pixcol = expand (pixcol, packetSize, &dataByte, cmap);        \
			} else {							/* non run-length packet */   \
				while (packetSize--)                                          \
					pixcol = expand (pixcol, 1, &dataByte, cmap);             \
			}                                                                 \
		}                                                                     \
	}                                                                         \
	done_##expand:;                                                           \
} while (0)

static void
decode_colormap_rle (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	cmap_t     *cmap;
	int         column, columns, rows, span;

	cmap = parse_colormap (targa, &dataByte);

	tex->format = tex_rgba;

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span, 4);

	if (targa->attributes & 0x10)	// right to left
		rle_expand (reverse_read_cmap);
	else							// left to right
		rle_expand (read_cmap);
}

static void
decode_truecolor_24_rle (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	int         column, columns, rows, span;
	cmap_t     *cmap = 0;		// not really used but needed for rle_expand

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span, 4);

	if (targa->attributes & 0x10)	// right to left
		rle_expand (reverse_read_bgr);
	else							// left to right
		rle_expand (read_bgr);
}

static void
decode_truecolor_32_rle (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	int         column, columns, rows, span;
	cmap_t     *cmap = 0;		// not really used but needed for rle_expand

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span, 4);

	if (targa->attributes & 0x10)	// right to left
		rle_expand (reverse_read_bgra);
	else							// left to right
		rle_expand (read_bgra);
}

static void
decode_truecolor (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	dataByte = skip_colormap (targa, dataByte);

	switch (targa->pixel_size) {
		case 24:
			tex->format = tex_rgba;		// image gets expadned to rgba
			decode_truecolor_24 (targa, tex, dataByte);
			break;
		case 32:
			tex->format = tex_rgba;
			decode_truecolor_32 (targa, tex, dataByte);
			break;
		default:
			Sys_Error ("LoadTGA: unsupported pixel size");
	}
}

static void
decode_truecolor_rle (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	dataByte = skip_colormap (targa, dataByte);

	switch (targa->pixel_size) {
		case 24:
			tex->format = tex_rgba;		// image gets expadned to rgba
			decode_truecolor_24_rle (targa, tex, dataByte);
			break;
		case 32:
			tex->format = tex_rgba;
			decode_truecolor_32_rle (targa, tex, dataByte);
			break;
		default:
			Sys_Error ("LoadTGA: unsupported truecolor pixel size");
	}
}

static void
decode_greyscale (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	int         column, columns, rows, span;

	dataByte = skip_colormap (targa, dataByte);

	if (targa->pixel_size != 8)
		Sys_Error ("LoadTGA: unsupported truecolor pixel size");
	tex->format = tex_l;

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span, 1);

	if (targa->attributes & 0x10) {
		// right to left
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = reverse_read_l (pixcol, 1, &dataByte, 0);
			pixrow += span;
		}
	} else {
		// left to right
		while (rows-- > 0) {
			pixcol = pixrow;
			for (column = columns; column > 0; column--)
				pixcol = read_l (pixcol, 1, &dataByte, 0);
			pixrow += span;
		}
	}
}

static void
decode_greyscale_rle (TargaHeader *targa, tex_t *tex, byte *dataByte)
{
	byte       *pixcol, *pixrow;
	int         column, columns, rows, span;
	cmap_t     *cmap = 0;		// not really used but needed for rle_expand

	dataByte = skip_colormap (targa, dataByte);

	if (targa->pixel_size != 8)
		Sys_Error ("LoadTGA: unsupported truecolor pixel size");
	tex->format = tex_l;

	columns = targa->width;
	rows = targa->height;

	setup_pixrow_span (targa, tex, &pixrow, &span, 1);

	if (targa->attributes & 0x10)	// right to left
		rle_expand (reverse_read_l);
	else							// left to right
		rle_expand (read_l);
}

typedef void (*decoder_t) (TargaHeader *, tex_t *, byte *);
static decoder_t decoder_functions[] = {
	0,					// 0 invalid
	decode_colormap,
	decode_truecolor,
	decode_greyscale,
	0, 0, 0, 0,			// 5-7 invalid
	0,					// 8 invalid
	decode_colormap_rle,
	decode_truecolor_rle,
	decode_greyscale_rle,
	0, 0, 0, 0,			// 12-15 invalid
};
#define NUM_DECODERS (sizeof (decoder_functions) \
					  / sizeof (decoder_functions[0]))

struct tex_s *
LoadTGA (QFile *fin, int load)
{
	byte       *dataByte;
	decoder_t   decode;
	int         fsize = sizeof (TargaHeader);
	int			numPixels;
	size_t      targa_mark;
	TargaHeader *targa;
	tex_t      *tex;

	if (load) {
		fsize = Qfilesize (fin);
	}
	targa_mark = Hunk_LowMark (0);
	targa = Hunk_AllocName (0, fsize, "TGA");
	Qread (fin, targa, fsize);

	targa->colormap_index = LittleShort (targa->colormap_index);
	targa->colormap_length = LittleShort (targa->colormap_length);
	targa->x_origin = LittleShort (targa->x_origin);
	targa->y_origin = LittleShort (targa->y_origin);
	targa->width = LittleShort (targa->width);
	targa->height = LittleShort (targa->height);

	if (targa->image_type >= NUM_DECODERS
		|| !(decode = decoder_functions[targa->image_type])) {
		Sys_Printf ("LoadTGA: Unsupported targa type");
		Hunk_FreeToLowMark (0, targa_mark);
		return 0;
	}

	if (load) {
		numPixels = targa->width * targa->height;
	} else {
		numPixels = 0;
	}
	tex = Hunk_TempAlloc (0, sizeof (tex_t) + numPixels * 4);
	tex->data = (byte *) (tex + 1);
	tex->width = targa->width;
	tex->height = targa->height;
	tex->palette = 0;
	tex->loaded = load;

	if (load) {
		// skip TARGA image comment
		dataByte = (byte *) (targa + 1);
		dataByte += targa->id_length;

		decode (targa, tex, dataByte);
	}

	Hunk_FreeToLowMark (0, targa_mark);
	return tex;
}

VISIBLE void
WriteTGAfile (const char *tganame, byte *data, int width, int height)
{
	QFile      *qfile;
	TargaHeader header;

	memset (&header, 0, sizeof (header));
	header.image_type = 2;				// uncompressed type
	header.width = LittleShort (width);
	header.height = LittleShort (height);
	header.pixel_size = 24;

	qfile = QFS_WOpen (tganame, 0);
	if (!qfile) {
		Sys_Printf ("Error opening %s", tganame);
		return;
	}
	Qwrite (qfile, &header, sizeof (header));
	Qwrite (qfile, data, width * height * 3);
	Qclose (qfile);
}
