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

#ifndef qfbsp_writebsp_h
#define qfbsp_writebsp_h

#include "QF/bspfile.h"

/**	\defgroup qfbsp_writebsp BSP Writing Functions
	\ingroup qfbsp
*/
///@{

struct node_s;

/**	Write the planes of the map bsp to the bsp file.

	\param headnode	The root of the map bsp.
*/
void WriteNodePlanes (struct node_s *headnode);

/**	Write the clip nodes to the bsp file.

	\param headnode	The root of the map bsp.
*/
void WriteClipNodes (struct node_s *headnode);

/**	Write the draw nodes and the model information to the bsp file.

	\param headnode	The root of the map bsp.
*/
void WriteDrawNodes (const struct node_s *headnode);

/**	Write the model information for the clipping hull.

	\param hullnum	The number of this clipping hull.
*/
void BumpModel (int hullnum);

/**	Add a plane to the bsp.

	If the plane already exists in the bsp, return the number of the existing
	plane rather than adding a new one.

	\param p		The plane to add to the bsp.
	\return			The plane number within the bsp.
*/
int FindFinalPlane (const dplane_t *p);

/**	Prepare the bsp file for writing.

	Write a null edge and leaf for edge 0 and leaf 0.
*/
void BeginBSPFile (void);

/**	Finalize the bsp file.

	Writes the miptex data to the bsp file and then writes the bsp file to
	the file system.
*/
void FinishBSPFile (void);

///@}

#endif//qfbsp_writebsp_h
