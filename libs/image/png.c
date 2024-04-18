/*
	png.c

	PNG image handling

	Copyright (C) 2003 Harry Roberts

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

#ifdef HAVE_PNG

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <png.h>

#include "QF/image.h"
#include "QF/png.h"
#include "QF/qendian.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"
#include "qfalloca.h"


/* Qread wrapper for libpng */
static void
user_read_data (png_structp png_ptr, png_bytep data, png_size_t length)
{
	Qread ((QFile *) png_get_io_ptr (png_ptr), data, length);
}

/* Qwrite wrapper for libpng */
static void
user_write_data (png_structp png_ptr, png_bytep data, png_size_t length)
{
	Qwrite ((QFile *) png_get_io_ptr (png_ptr), data, length);
}

/* Qflush wrapper for libpng */
static void
user_flush_data (png_structp png_ptr)
{
	Qflush ((QFile *) png_get_io_ptr (png_ptr));
}

/* Basicly taken from the libpng example rpng-x */
static int
readpng_init (QFile *infile, png_structp *png_ptr, png_infop *info_ptr)
{
	unsigned char	sig[8];

	/* Check the signiture */
	Qread (infile, sig, 8);
	if (!png_check_sig (sig, 8)) {
		Sys_Printf ("Bad png file\n");
		return (1);
	}

	*png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL,
									   NULL);
	if (!*png_ptr)
		return (2); /* Out of memory! */

	*info_ptr = png_create_info_struct (*png_ptr);
	if (!*info_ptr) {
		png_destroy_read_struct (png_ptr, NULL, NULL);
		return (3); /* Out of memory! */
	}

	/* setjmp() must be called in every function that calls a PNG-reading
	 * libpng function */

	if (setjmp (png_jmpbuf (*png_ptr))) {
		png_destroy_read_struct (png_ptr, info_ptr, NULL);
		return (4);
	}

	png_set_read_fn (*png_ptr, infile, user_read_data);

	/* Is png_set_sig_bytes needed? */
	png_set_sig_bytes (*png_ptr, 8); // We allready read the 8 signiture bytes

	png_read_info (*png_ptr, *info_ptr);//read all png info upto the image data

	return (0);
}

/* Load the png file and return a texture */
VISIBLE tex_t *
LoadPNG (QFile *infile, int load)
{
	double			gamma;
	png_structp		png_ptr = NULL;
	png_infop		info_ptr = NULL;
	png_uint_32		height, width, rowbytes, i;
	png_bytepp		row_pointers = NULL;
	int				bit_depth, color_type;
	tex_t		   *tex;

	if (readpng_init (infile, &png_ptr, &info_ptr) != 0)
		return (NULL);

	png_get_IHDR (png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
				  NULL, NULL, NULL);

	if (load) {
		if (color_type == PNG_COLOR_TYPE_PALETTE)
			png_set_expand (png_ptr);

		if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
			png_set_expand (png_ptr);

		if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS))
			png_set_expand (png_ptr);

		if (bit_depth == 16)
			png_set_strip_16 (png_ptr);

		if (color_type == PNG_COLOR_TYPE_GRAY
			|| color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb (png_ptr);

		/* NOTE: gamma support? */
		/* unlike the example in the libpng documentation, we have *no* idea
		 * wherethis file may have come from--so if it doesn't have a file
		 * gamma, don't do any correction ("do no harm")
		 */
		if (png_get_gAMA(png_ptr, info_ptr, &gamma))
			png_set_gamma (png_ptr, 1.0, gamma);

		/* All transformations have been registered, now update the info_ptr
		 * structure */
		png_read_update_info (png_ptr, info_ptr);

		/* Allocate tex_t structure */
		rowbytes = png_get_rowbytes(png_ptr, info_ptr);
		tex = Hunk_TempAlloc (0, sizeof (tex_t) + height * rowbytes);
		*tex = (tex_t) {
			.data = (byte *) (tex + 1),
		};
	} else {
		tex = Hunk_TempAlloc (0, sizeof (tex_t));
		*tex = (tex_t) { };
	}

	tex->width = width;
	tex->height = height;
	if (color_type & PNG_COLOR_MASK_ALPHA)
		tex->format = tex_rgba;
	else
		tex->format = tex_rgb;
	tex->palette = NULL;
	tex->loaded = load;

	if (!load) {
		png_read_end (png_ptr, NULL);
		return tex;
	}

	if ((row_pointers = (png_bytepp) malloc (height * sizeof (png_bytep)))
		== NULL) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		return (NULL); /* Out of memory */
	}

	for (i = 0; i < height; ++i)
		row_pointers[i] = tex->data + (i * rowbytes);

	/* Now we can go ahead and read the whole image */
	png_read_image (png_ptr, row_pointers);

	free (row_pointers);
	row_pointers = NULL;

	png_read_end (png_ptr, NULL);

	return (tex);
}

#define WRITEPNG_BIT_DEPTH 8

VISIBLE int
WritePNG (QFile *outfile, const tex_t *tex)
{
	int         i;
	png_structp png_ptr;
	png_infop   info_ptr;
	png_bytepp  row_pointers = NULL;

	/* initialize write struct */
	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		Sys_Printf ("png_Create_write_struct failed\n");
		return 0;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (png_ptr == NULL) {
		png_destroy_write_struct (&png_ptr, NULL);
		Sys_Printf ("png_create_info_struct failed\n");
		return 0;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct (&png_ptr, &info_ptr);
		return 0;
	}

	png_set_write_fn (png_ptr, outfile, user_write_data, user_flush_data);

	/* Write the header */
	if (setjmp(png_jmpbuf(png_ptr))) {
		Sys_Printf ("Error writing png header\n");
		return 0;
	}

	png_set_IHDR (png_ptr, info_ptr, tex->width, tex->height,
				  WRITEPNG_BIT_DEPTH,
				  PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
				  PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	/* NOTE: Write gamma support? */
	/* png_set_gAMA (png_ptr, info_ptr, gamma); */

	if (tex->bgr) {
		png_set_bgr(png_ptr);
	}

	png_write_info (png_ptr, info_ptr);

	/* Setup row pointers */
	row_pointers = (png_bytepp)alloca(tex->height * sizeof(png_bytep));

	int         rowbytes = tex->width * 3;
	if (tex->flipped) {
		for (i = 0; i < tex->height; i++) {
			row_pointers[tex->height - i - 1] = tex->data + (i * rowbytes);
		}
	} else {
		for (i = 0; i < tex->height; i++) {
			row_pointers[i] = tex->data + (i * rowbytes);
		}
	}


	if (setjmp(png_jmpbuf(png_ptr))) {
		Sys_Printf ("Error writing PNG image data\n");
		return 0;
	}

	png_write_image (png_ptr, row_pointers);

	/* end write */
	if (setjmp(png_jmpbuf(png_ptr))) {
		Sys_Printf ("Error writing end of PNG image\n");
		return 0;
	}

	png_write_end (png_ptr, NULL);

	return 1;
}

#else

#include "QF/image.h"
#include "QF/png.h"

VISIBLE tex_t *
LoadPNG (QFile *infile, int load)
{
	return 0;
}

VISIBLE int
WritePNG (QFile *outfile, const tex_t *tex)
{
	return 0;
}

#endif
