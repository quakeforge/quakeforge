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

#ifndef qfbsp_tjunc_h
#define qfbsp_tjunc_h

/**	\defgroup qfbsp_tjunc T-Junction Repair
	\ingroup qfbsp
*/
///@{

struct node_s;

/**	Add the edges from the faces in the bsp tree.

	\param headnode	The current node in the bsp tree.
*/
void tjunc (struct node_s *headnode);

///@}

#endif//qfbsp_tjunc_h
