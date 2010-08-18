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

#ifndef qfbsp_csg4_h
#define qfbsp_csg4_h

#include "QF/bspfile.h"

struct plane_s;
struct visfacet_s;
struct brushset_s;
struct surface_s;

// build surfaces is also used by GatherNodeFaces
extern struct visfacet_s *validfaces[MAX_MAP_PLANES];
struct surface_s *BuildSurfaces (void);

struct visfacet_s *NewFaceFromFace (struct visfacet_s *in);
struct surface_s *CSGFaces (struct brushset_s *bs);
void SplitFace (struct visfacet_s *in, struct plane_s *split,
				struct visfacet_s **front, struct visfacet_s **back);

#endif//qfbsp_csg4_h
