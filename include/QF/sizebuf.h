/*
	sizebuf.h

	(description)

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
#ifndef __QF_sizebuf_h
#define __QF_sizebuf_h

/** \defgroup sizebuf Fixed Size Buffers
	\ingroup utils
	Fixed size buffer management
*/
///@{

#include "QF/qtypes.h"

typedef struct sizebuf_s
{
	bool        allowoverflow;	// if false, do a Sys_Error
	bool        overflowed;		// set to true if the buffer size failed
	byte       *data;
	unsigned    maxsize;
	unsigned    cursize;
} sizebuf_t;

void SZ_Alloc (sizebuf_t *buf, unsigned maxsize);
void SZ_Free (sizebuf_t *buf);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, unsigned length);
void SZ_Write (sizebuf_t *buf, const void *data, unsigned length);
void SZ_Print (sizebuf_t *buf, const char *data);	// strcats onto the sizebuf
void SZ_Dump (sizebuf_t *buf);

///@}

#endif//__QF_sizebuf_h
