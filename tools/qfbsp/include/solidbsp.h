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

#ifndef qfbsp_solidbsp_h
#define qfbsp_solidbsp_h

#include "QF/qtypes.h"

struct visfacet_s;
struct plane_s;
struct surface_s;
struct node_s;

void DivideFacet (struct visfacet_s *in, struct plane_s *split,
				  struct visfacet_s **front, struct visfacet_s **back);
void CalcSurfaceInfo (struct surface_s *surf);
struct node_s *SolidBSP (struct surface_s *surfhead, qboolean midsplit);

#endif//qfbsp_solidbsp_h
