/*
	fullbright.c

	fullbright skin handling

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
// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "r_local.h"

VISIBLE int
Mod_CalcFullbright (const byte *in, byte *out, int pixels)
{
	byte	fb = 0;

	while (pixels-- > 0) {
		byte        pix = *in++;
		if (pix >= 256 - 32) {
			fb = 1;
			*out++ = pix;
		} else {
			*out++ = 0;
		}
	}
	return fb;
}

VISIBLE void
Mod_ClearFullbright (const byte *in, byte *out, int pixels)
{
	while (pixels-- > 0) {
		byte        pix = *in++;
		if (pix >= 256 - 32) {
			*out++ = 0;
		} else {
			*out++ = pix;
		}
	}
}
