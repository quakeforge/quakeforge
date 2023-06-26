/*
	pcx.h

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

#ifndef __QF_pcx_h
#define __QF_pcx_h

#include "QF/qtypes.h"
#include "QF/quakeio.h"

/** A ZSoft PC Paintbrush (PCX) header */
typedef struct
{
    char	manufacturer;	///< Manufacturer: Must be 10
    char	version;		///< PCX version: must be 5
    char	encoding;		///< Coding used: must be 1
    char	bits_per_pixel;	///< BPP: must be 8 for QF
    unsigned short	xmin, ymin, xmax, ymax;
    unsigned short	hres, vres;	///< resolution of image (DPI)
    unsigned char	palette[48];
    char	reserved;		///< should be 0
    char	color_planes;	///< Number of color planes
    unsigned short	bytes_per_line;	///< Number of bytes for a scan line (per plane). According to ZSoft, must be an even number.
    unsigned short	palette_type;	///< Palette info: 1 = color, 2 = greyscale
    char	filler[58];
} pcx_t;

/**
	Convert \b paletted image data to PCX format.

	\param data A pointer to the buffer containing the source image
	\param width The width, in pixels, of the source image
	\param height The height, in pixels, of the source image
	\param rowbytes The number of bytes in a row of an image (usually the same
	as the width)
	\param palette The palette in use for the texture.
	\param flip If true, flip the order of lines output.
	\param[out] length The length of the encoded PCX data.

	\return A pointer to the newly-coded PCX data.
	\warning Uses Hunk_TempAlloc() to allocate the output PCX content.
*/
pcx_t *EncodePCX (const byte *data, int width, int height, int rowbytes,
                  const byte *palette, bool flip, int *length);

/**
	Load a texture from a PCX file.

	\param f The file to read the texture from
	\param convert If true, the texture is converted to RGB on load
	\param pal The palette to apply during conversion
	\param load If false, only the format and size info is loaded

	\return A pointer to the texture.
	\warning Uses Hunk_TempAlloc() to allocate the texture.
*/
struct tex_s *LoadPCX (QFile *f, bool convert, const byte *pal, int load);

#endif//__QF_pcx_h
