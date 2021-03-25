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

#ifndef surfaces_h
#define surfaces_h

#include "QF/bspfile.h"

/**	\defgroup qfbsp_surface Surface Functions
	\ingroup qfbsp
*/
///@{

struct visfacet_s;
struct node_s;
struct surface_s;

typedef struct edgeface_s {
	struct visfacet_s *f[2];
} edgeface_t;

extern int c_cornerverts;
extern int c_tryedges;
extern edgeface_t *edgefaces;

extern int firstmodeledge;
extern int firstmodelface;

/**	Allocate a new face.

	Increases \c c_activefaces by one.

	\return			Pointer to the new face.
*/
face_t *AllocFace (void);

/**	Free a face.

	Only the first face will be freed. If the face is linked to another face,
	that face will have to be freed seperately.

	Reduces \c c_activefaces by one.

	\param f		The face to free.
*/
void FreeFace (face_t *f);

/**	Allocate a new surface.

	Increases \c c_activesurfaces by one.

	\return			Pointer to the new surface.
*/
surface_t *AllocSurface (void);

/**	Free a surface.

	Only the first surface will be freed. If the surface is linked to another
	surface, that surface will have to be freed seperately.

	Reduces \c c_activefaces by one.

	\param s		The face to free.
*/
void FreeSurface (surface_t *s);

/**	Split any faces that are too big.

	If the face is > options.subdivide_size in either texture direction,
	carve off a valid sized piece and insert the remainder in the next link.

	\param f		The face to subdivide.
	\param prevptr	The address of the pointer to this face. For nice linked
					list manipulation.
*/
void SubdivideFace (struct visfacet_s *f, struct visfacet_s **prevptr);

/**	Free the current node tree and return a new chain of the surfaces that
	have inside faces.
*/
struct surface_s *GatherNodeFaces (struct node_s *headnode);

/**	Give edges to all the faces in the bsp tree.

	\param headnode	The root of the bsp tree.
*/
void MakeFaceEdges (struct node_s *headnode);

///@}

#endif//surfaces_h
