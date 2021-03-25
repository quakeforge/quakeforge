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

#ifndef qfbsp_csg4_h
#define qfbsp_csg4_h

#include "QF/bspfile.h"

/**	\defgroup qfbsp_csg4 CSG Functions
	\ingroup qfbsp
*/
///@{

struct plane_s;
struct visfacet_s;
struct brushset_s;
struct surface_s;

extern struct visfacet_s *validfaces[MAX_MAP_PLANES];

/**	Build the surface lists for the brushset.

	Each plane still with faces after clipping will have one surface, and
	each surface will list the faces on that plane.

	\return			Chain of all the external surfaces with one or more
					visible faces.
*/
struct surface_s *BuildSurfaces (void);

/**	Create a duplicate face.

	Duplicates the non point information of a face.

	\param in		The face to duplicate.
	\return			The new face.

*/
struct visfacet_s *NewFaceFromFace (const struct visfacet_s *in);

/**	Convert a brush-set to a list of clipped faces.

	The brushes of the brush set will be clipped against each other.

	\param bs		The brush-set to convert.
	\return			list of surfaces containing all of the faces
*/
struct surface_s *CSGFaces (struct brushset_s *bs);

/**	Split a face by a plane.

	\param in		The face to split.
	\param split	The plane by which to split the face.
	\param front	Set to the new face repesenting the part of the input
					face that is in front of the plane, or NULL if there is
					no front portion.
	\param back		Set to the new face repesenting the part of the input
					face that is behind the plane, or NULL if there is
					no back portion.
	\note A face entirely on the plane is considered to be behind the plane.

	\note The face represented by \a in becomes invalid. If either \a front
	or \a back is NULL, then no split has occurred and the other will be set
	to \a in.
*/
void SplitFace (struct visfacet_s *in, struct plane_s *split,
				struct visfacet_s **front, struct visfacet_s **back);

///@}

#endif//qfbsp_csg4_h
