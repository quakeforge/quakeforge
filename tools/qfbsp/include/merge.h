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

#ifndef qfbsp_merge_h
#define qfbsp_merge_h

/**	\defgroup qfbsp_merge Merge Functions
	\ingroup qfbsp
*/
///@{

/**	Add a face to the list of faces, doing any possible merging.

	\param face		The face to add to the list.
	\param list		The list to which the face will be added.
	\return			The list with the plane added or merged to other faces in
					the list.
*/
face_t *MergeFaceToList (face_t *face, face_t *list);

/**	Remove merged out faces from the list.

	Removes faces that MergeFaceToList() wanted to remove but couldn't due to
	its implementation.

	\param merged	The list of merged faces.
	\return			The cleaned list.

	\note			The list is reversed in the process.

	\todo Reimplement MergeFaceToList() such that this function is no longer
	needed.
*/
face_t *FreeMergeListScraps (face_t *merged);

/**	Process the faces on a surface, searching for mergable faces.

	\param plane	The surface of which the faces will be processed.
*/
void MergePlaneFaces (surface_t *plane);

/**	Process the faces on a list of suraces.

	\param surfhead	The list of surfaces.
*/
void MergeAll (surface_t *surfhead);

///@}

#endif//qfbsp_merge_h
