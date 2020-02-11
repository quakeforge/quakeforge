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

#ifndef qfbsp_brush_h
#define qfbsp_brush_h

#include "QF/mathlib.h"

#include "bsp5.h"
#include "map.h"

/**	\defgroup qfbsp_brush Brush Functions
	\ingroup qfbsp
*/
///@{

#define	NUM_HULLS		2				// normal and +16

#define	NUM_CONTENTS	2				// solid and water

typedef struct brush_s {
	struct brush_s *next;
	vec3_t      mins, maxs;
	struct visfacet_s *faces;
	int         contents;
} brush_t;

typedef struct brushset_s {
	vec3_t      mins, maxs;
	brush_t    *brushes;			// NULL terminated list
} brushset_t;

extern int      numbrushplanes;
extern plane_t  planes[MAX_MAP_PLANES];

/**	Allocate a new brush.

	\return			Pointer to the new brush.
*/
brush_t *AllocBrush (void);

/**	Load the brushes from the entity.

	\param ent		The entity from which to load the brushes.
	\param hullnum	The number of the hull for which to load the brushes.
	\return			The brush set holding the brushes loaded from the entity.
*/
brushset_t *Brush_LoadEntity (entity_t *ent, int hullnum);

/**	Determine the primary axis of the normal.

	\param normal	Must be canonical.
*/
int	PlaneTypeForNormal (const vec3_t normal) __attribute__((pure));

/**	Make the plane canonical.

	A cononical plane is one whose normal points towards +inf on its primary
	axis. The primary axis is that which has the largest magnitude of the
	vector's components.

	\param dp		The plane to make canonical.
	\return			1 if the plane was flipped, otherwise 0.
*/
int NormalizePlane (plane_t *dp);

/**	Add a plane to the global list of planes.

	Make the plane canonical, and add it to the global list of planes if it
	does not duplicate a plane that is already in the list. If the plane is
	flipped while being made canonical, side will be set to 1, otherwise side
	will be 0.

	\param dplane	The plane to add.
	\param side		The side of the plane that will be front.
	\return			global plane number.
*/
int	FindPlane (const plane_t *dplane, int *side);

///@}

#endif//qfbsp_brush_h
