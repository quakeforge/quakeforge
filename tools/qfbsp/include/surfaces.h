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

#ifndef surfaces_h
#define surfaces_h

#include "QF/bspfile.h"

struct visfacet_s;
struct node_s;
struct surface_s;

extern int c_cornerverts;
extern int c_tryedges;
extern struct visfacet_s *edgefaces[MAX_MAP_EDGES][2];

extern int firstmodeledge;
extern int firstmodelface;

void SubdivideFace (struct visfacet_s *f, struct visfacet_s **prevptr);

struct surface_s *GatherNodeFaces (struct node_s *headnode);

void MakeFaceEdges (struct node_s *headnode);

#endif//surfaces_h
