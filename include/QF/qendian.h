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

*/

#ifndef __QF_qendian_h
#define __QF_qendian_h

/** \defgroup qendian Endian handling functions
	\ingroup utils
*/
///@{

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

uint16_t	_ShortSwap (uint16_t l) __attribute__((const));
uint16_t	_ShortNoSwap (uint16_t l) __attribute__((const));
uint32_t _LongSwap (uint32_t l) __attribute__((const));
uint32_t _LongNoSwap (uint32_t l) __attribute__((const));
float	_FloatSwap (float f) __attribute__((const));
float	_FloatNoSwap (float f) __attribute__((const));


#ifdef __GNUC__
#define ShortSwap(l)	({  uint16_t x = (uint16_t) (l);		\
							x = (  ((x >> 8) & 0xff)			\
								 | ((x << 8) & 0xff00));		\
							x;	})

#define LongSwap(l)		({  uint32_t z = (uint32_t) (l);				\
							z = (ShortSwap (z >> 16)) | (ShortSwap (z) << 16); \
							z;	})

#define FloatSwap(l)	({	union { uint32_t i; float f; } y;		\
							y.f = (l);								\
							y.i = LongSwap (y.i);					\
							y.f;	})
#else
#define ShortSwap(l) _ShortSwap (l)
#define LongSwap(l)  _LongSwap (l)
#define FloatSwap(l) _FloatSwap (l)
#endif

#define ShortNoSwap(l) ((uint16_t) (l))
#define LongNoSwap(l) ((uint32_t) (l))
#define FloatNoSwap(l) ((float) (l))

// NOTE: these /always/ read and write /little/ endian entities.
struct QFile_s;
void WriteFloat (struct QFile_s *file, float f);
void WriteByte (struct QFile_s *file, int b);
void WriteShort (struct QFile_s *file, unsigned int s);
void WriteLong (struct QFile_s *file, unsigned int l);
float ReadFloat (struct QFile_s *file);
byte ReadByte (struct QFile_s *file);
unsigned short ReadShort (struct QFile_s *file);
unsigned int ReadLong (struct QFile_s *file);

///@}

#endif//__QF_qendian_h
