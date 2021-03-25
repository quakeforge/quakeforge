/*
	qendian.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#include <ctype.h>

#include "QF/qendian.h"
#include "QF/qtypes.h"
#include "QF/quakeio.h"

/*
					BYTE ORDER FUNCTIONS
*/

#ifndef WORDS_BIGENDIAN
VISIBLE qboolean    bigendien = false;
#else
VISIBLE qboolean    bigendien = true;
#endif


VISIBLE uint16_t
_ShortSwap (uint16_t l)
{
	byte        b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

VISIBLE uint16_t
_ShortNoSwap (uint16_t l)
{
	return l;
}

VISIBLE uint32_t
_LongSwap (uint32_t l)
{
	byte        b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int) b1 << 24) + ((int) b2 << 16) + ((int) b3 << 8) + b4;
}

VISIBLE uint32_t
_LongNoSwap (uint32_t l)
{
	return l;
}

VISIBLE float
_FloatSwap (float f)
{
	union {
		float       f;
		byte        b[4];
	} dat1     , dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

VISIBLE float
_FloatNoSwap (float f)
{
	return f;
}
