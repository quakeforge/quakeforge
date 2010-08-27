/*
	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	See file, 'COPYING', for details.

	$Id$
*/

#ifndef qfbsp_winding_h
#define qfbsp_winding_h

#include "QF/mathlib.h"

struct plane_s;

typedef struct winding_s {
	int         numpoints;
	vec3_t      points[8];			// variable sized
} winding_t;

winding_t *BaseWindingForPlane (struct plane_s *p);
winding_t *NewWinding (int points);
void FreeWinding (winding_t *w);
winding_t *CopyWinding (winding_t *w);
winding_t *CopyWindingReverse (winding_t *w);
winding_t *ClipWinding (winding_t *in, struct plane_s *split, qboolean keepon);
void DivideWinding (winding_t *in, struct plane_s *split,
					winding_t **front, winding_t **back);

#endif//qfbsp_winding_h
