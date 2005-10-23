/*
	fendian.c

	endian neutral file read/write routines

	Copyright (C) 2001       Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/7/9

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#include "QF/qendian.h"
#include "QF/quakeio.h"


void
WriteFloat (QFile *file, float f)
{
	// a float in C is /defined/ to be 32 bits. byte order, can, of course
	// still make a mess.
	union {
		float       f;
		byte        b[4];
	} dat;

	dat.f = LittleFloat (f);
	Qwrite (file, dat.b, sizeof (dat.b));
}

void
WriteByte (QFile *file, int b)
{
	byte        dat = b & 0xff;
	Qwrite (file, &dat, 1);
}

void
WriteShort (QFile *file, unsigned int s)
{
	byte        dat[2];

	dat[0] = s & 0xff;
	dat[1] = (s >> 8) & 0xff;
	Qwrite (file, dat, sizeof (dat));
}

void
WriteLong (QFile *file, unsigned int l)
{
	byte        dat[4];

	dat[0] = l & 0xff;
	dat[1] = (l >> 8) & 0xff;
	dat[2] = (l >> 16) & 0xff;
	dat[3] = (l >> 24) & 0xff;
	Qwrite (file, dat, sizeof (dat));
}

float
ReadFloat (QFile *file)
{
	// a float in C is /defined/ to be 32 bits. byte order, can, of course
	// still make a mess.
	union {
		float       f;
		byte        b[4];
	} dat;

	Qread (file, dat.b, sizeof (dat.b));
	return LittleFloat (dat.f);
}

byte
ReadByte (QFile *file)
{
	byte        dat;
	Qread (file, &dat, 1);
	return dat;
}

unsigned short
ReadShort (QFile *file)
{
	byte        dat[2];

	Qread (file, dat, sizeof (dat));
	return (dat[1] << 8) | dat[0];
}

unsigned int
ReadLong (QFile *file)
{
	byte        dat[4];

	Qread (file, dat, sizeof (dat));
	return (dat[3] << 24) | (dat[2] << 16) | (dat[1] << 8) | dat[0];
}
