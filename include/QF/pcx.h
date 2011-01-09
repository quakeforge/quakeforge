/*
	pcx.h

	pcx image hangling

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

#ifndef __pcx_h
#define __pcx_h

#include "QF/qtypes.h"
#include "QF/quakeio.h"

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin, ymin, xmax, ymax;
    unsigned short	hres, vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
} pcx_t;

pcx_t *EncodePCX (byte * data, int width, int height,
				  int rowbytes, byte * palette, qboolean flip, int *length);
struct tex_s *LoadPCX (QFile *f, qboolean convert, byte *pal);

#endif // __pcx_h
