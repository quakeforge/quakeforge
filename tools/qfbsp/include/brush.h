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

#ifndef qfbsp_brush_h
#define qfbsp_brush_h

#include "QF/mathlib.h"

#include "bsp5.h"
#include "map.h"

#define	NUM_HULLS		2				// normal and +16

#define	NUM_CONTENTS	2				// solid and water

typedef struct brush_s {
	struct brush_s	*next;
	vec3_t			mins, maxs;
	struct visfacet_s *faces;
	int				contents;
} brush_t;

typedef struct brushset_s {
	vec3_t		mins, maxs;
	brush_t		*brushes;		// NULL terminated list
} brushset_t;

extern	int			numbrushplanes;
extern	plane_t		planes[MAX_MAP_PLANES];

brushset_t *Brush_LoadEntity (entity_t *ent, int hullnum);
int	PlaneTypeForNormal (const vec3_t normal);
int	FindPlane (plane_t *dplane, int *side);

#endif//qfbsp_brush_h
