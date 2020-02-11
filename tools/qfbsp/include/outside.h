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

#ifndef qfbsp_outside_h
#define qfbsp_outside_h

/**	\defgroup qfbsp_outside Outside Functions
	\ingroup qfbsp
*/
///@{

struct node_s;

/**	Make the outside of the map solid.

	Removes any faces from filled nodes.

	\pre outside_node must be initilized.

	\param node		The root of the map's bsp.
	\return			\c true if the outside has been set solid, otherwise
					\c false.
*/
qboolean FillOutside (struct node_s *node);

///@}

#endif//qfbsp_outside_h
