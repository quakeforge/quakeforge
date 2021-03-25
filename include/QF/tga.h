/*
	tga.h

	targa image hangling

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

#ifndef __QF_tga_h
#define __QF_tga_h

#include "QF/qtypes.h"
#include "QF/quakeio.h"

#ifndef __GNUC__
# if defined (__BORLANDC__) || defined (_MSC_VER)
#  if (defined(__BORLANDC__) && (__BORLANDC__ < 0x550))
#   pragma option -a1
#  else
#   pragma pack(push, tgainclude)
#   pragma pack(1)
#  endif
# else
#  error do some data packing magic here (#pragma pack?)
# endif
#endif

typedef struct _TargaHeader {
	unsigned char id_length; // __attribute__((packed));
	unsigned char colormap_type; // __attribute__((packed));
	unsigned char image_type; // __attribute__((packed));
	unsigned short colormap_index __attribute__((packed));
	unsigned short colormap_length __attribute__((packed));
	unsigned char colormap_size; // __attribute__((packed));
	unsigned short x_origin __attribute__((packed));
	unsigned short y_origin __attribute__((packed));
	unsigned short width __attribute__((packed));
	unsigned short height __attribute__((packed));
	unsigned char pixel_size; // __attribute__((packed));
	unsigned char attributes; // __attribute__((packed));
} TargaHeader;

#if defined (__BORLANDC__) || defined (_MSC_VER)
# if (defined(__BORLANDC__) && (__BORLANDC__ < 0x550))
#  pragma option -a4
# else
#  pragma pack(pop, tgainclude)
# endif
#endif

struct tex_s *LoadTGA (QFile *fin, int load);
void WriteTGAfile (const char *tganame, byte *data, int width, int height);

#endif//__QF_tga_h
