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

/** \addtogroup utils */
//@{

/** \defgroup qendian Endian handling functions
*/
//@{

#include "QF/qtypes.h"

#ifndef NULL
# define NULL ((void *)0)
#endif

#define Q_MAXCHAR	 ((char)0x7f)
#define Q_MAXSHORT	((short)0x7fff)
#define Q_MAXINT	  ((int)0x7fffffff)
#define Q_MAXLONG	  ((int)0x7fffffff)
#define Q_MAXFLOAT	  3.40282346638528859811704183484516925440e38

#define Q_MINCHAR	 ((char)0x80)
#define Q_MINSHORT	((short)0x8000)
#define Q_MININT	  ((int)0x80000000)
#define Q_MINLONG	  ((int)0x80000000)
#define Q_MINFLOAT	  -3.40282346638528859811704183484516925440e38

#define Q_FLOAT_EPSILON 1.1754943508222875079687365372222456778186655567720875215087517062784172594547271728515625e-38

//============================================================================

#ifndef WORDS_BIGENDIAN
#define BigShort ShortSwap
#define LittleShort ShortNoSwap
#define BigLong LongSwap
#define LittleLong LongNoSwap
#define BigFloat FloatSwap
#define LittleFloat FloatNoSwap
#else
#define BigShort ShortNoSwap
#define LittleShort ShortSwap
#define BigLong LongNoSwap
#define LittleLong LongSwap
#define BigFloat FloatNoSwap
#define LittleFloat FloatSwap
#endif

extern qboolean		bigendien;

short	ShortSwap (short l);
short	ShortNoSwap (short l);
int		LongSwap (int l);
int		LongNoSwap (int l);
float	FloatSwap (float f);
float	FloatNoSwap (float f);

// NOTE: these /always/ read and write /little/ endian entities.
struct QFile_s;
void WriteFloat (struct QFile_s *file, float f);
void WriteByte (struct QFile_s *file, int b);
void WriteShort (struct QFile_s *file, unsigned int s);
void WriteLong (struct QFile_s *file, unsigned int l);
float ReadFloat (struct QFile_s *file);
byte ReadByte (struct QFile_s *file);
unsigned short ReadShort (struct QFile_s *file);
unsigned long ReadLong (struct QFile_s *file);

//@}
//@}

#endif // __qendian_h
