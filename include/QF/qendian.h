/*
	qendian.h

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

	$Id$
*/

#ifndef __qendian_h
#define __qendian_h

#include "QF/qtypes.h"

#ifndef NULL
# define NULL ((void *)0)
#endif

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT	((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)
#define Q_MAXFLOAT ((int)0x7fffffff)

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT 	((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)
#define Q_MINFLOAT ((int)0x7fffffff)

//============================================================================

extern	qboolean	bigendien;
extern	short		(*BigShort) (short l);
extern	short		(*LittleShort) (short l);
extern	int			(*BigLong) (int l);
extern	int			(*LittleLong) (int l);
extern	float		(*BigFloat) (float l);
extern	float		(*LittleFloat) (float l);

short   ShortSwap (short l);
short	ShortNoSwap (short l);
int    LongSwap (int l);
int	LongNoSwap (int l);
float FloatSwap (float f);
float FloatNoSwap (float f);

//============================================================================

#endif // __qendian_h
