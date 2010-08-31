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

/**	Create a very large four-point winding with all point on the plane.

	The winding will be a box with aligned with the axes of the plane.

	\param p		The plane for which to create the winding.
	\return			The new winding.
	\note It is the caller's responsibiltiy to free the new winding.
*/
winding_t *BaseWindingForPlane (struct plane_s *p);

/**	Create a new, empty winding with.

	\param points	The number of points for which to leave space.
	\return			The new winding.
	\note It is the caller's responsibiltiy to free the new winding.
*/
winding_t *NewWinding (int points);

/**	Free the winding.

	\param w		The winding to be freed.
*/
void FreeWinding (winding_t *w);

/**	Create a new winding with the same points as the given winding.

	\param w		The winding to copy.
	\return			The new winding.
	\note It is the caller's responsibiltiy to free the new winding.
*/
winding_t *CopyWinding (winding_t *w);

/**	Create a new winding with the reverse points of the given winding.

	\param w		The winding to copy.
	\return			The new winding.
	\note It is the caller's responsibiltiy to free the new winding.
*/
winding_t *CopyWindingReverse (winding_t *w);

/**	Clip the winding to the plain.

	The new winding will be the part of the input winding that is on the
	front side of the plane.

	\note It is the caller's responsibiltiy to free the new winding.

	\note The input winding will be freed.

	\param in		The winding to be clipped.
	\param split	The plane by which the winding will be clipped.
	\param keepon	If true, an exactly on-plane winding will be saved,
					otherwise it will be clipped away.
	\return			The new winding representing the part of the input winding
					on the font side of the plane, or NULL if the winding has
					been clipped away.
*/
winding_t *ClipWinding (winding_t *in, struct plane_s *split, qboolean keepon);

/**	Divide a winding by a plane, producing one or two windings.

	\note The original winding is not damaged or freed.

	\note If one of \a front or \a back is NULL, the other will point to
	the input winding.

	\note If neither \a front nor \a back are NULL, then both will be new
	windings and the caller will be responsible for freeing them.

	\param in		The winding to be divided.
	\param split	The plane by which the winding will be divided.
	\param front	Set to the part of the input winding that is in front of
					the plane, or NULL if no part of the winding is in front
					of the plane.
	\param back		Set to the part of the input winding that is behind the
					plane, or NULL if no part of the winding is behind the
					plane.
*/
void DivideWinding (winding_t *in, struct plane_s *split,
					winding_t **front, winding_t **back);

#endif//qfbsp_winding_h
