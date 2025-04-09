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

*/

#ifndef __QF_winding_h
#define __QF_winding_h

#include "QF/mathlib.h"

/**	\defgroup winding Winding Manipulation
*/
///@{

struct plane_s;

#define ON_EPSILON 0.05

typedef struct winding_s {
	int         numpoints;			///< The number of points in the winding
	vec3_t      points[4];			///< variable sized, never less than 3
} winding_t;

/**	Create a very large four-point winding with all point on the plane.

	The winding will be a box with aligned with the axes of the plane. The
	order of yhe points is clockwise when viewed from the front side of
	the plane.

	In terms of s and t, the axes of the plane will be such that t (up) is
	the projection of either the z-axis or the x-axis (whichever is
	"closer"), and s is to the right (n = s cross t).

	\param p		The plane on which to create the winding.
	\return			The new winding.
	\note It is the caller's responsibiltiy to free the new winding.
*/
winding_t *BaseWindingForPlane (const struct plane_s *p);

/**	Create a new, empty winding with space for the specified number of points.

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
winding_t *CopyWinding (const winding_t *w);

/**	Create a new winding with the reverse points of the given winding.

	This is useful when a winding for the back side of a plane is required.

	\param w		The winding to copy.
	\return			The new winding.
	\note It is the caller's responsibiltiy to free the new winding.
*/
winding_t *CopyWindingReverse (const winding_t *w);

/** Create a new "winding" that holds the unit vectors of the edges of the
	given winding.

	\param w		The winding to convert.
	\param unit		If true, normalize the vectors.
	\return			The "winding" holding the (unit) vectors.
	\note It is the caller's responsibiltiy to free the new winding.
*/
winding_t *WindingVectors (const winding_t *w, int unit);

/**	Clip the winding to the plain.

	The new winding will be the part of the input winding that is on the
	front side of the plane.

	The direction of the winding is preserved.

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
winding_t *ClipWinding (winding_t *in, struct plane_s *split, bool keepon);

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

///@}

#endif//__QF_winding_h
