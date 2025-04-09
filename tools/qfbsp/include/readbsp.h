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

#ifndef qfbsp_readbsp_h
#define qfbsp_readbsp_h

/**	\defgroup qfbsp_readbsp BSP Reading Functions
	\ingroup qfbsp
*/
///@{

/**	Load the bspfile into memory.
*/
void LoadBSP (void);

/**	Recreate the map's portals and write the portal file.
*/
void bsp2prt (void);

/**	Write a wad file containing the textures in the bsp file.
*/
void extract_textures (void);

/**	Write the map's entities string to a file.
*/
void extract_entities (void);

/**	Write a hull from the map's bsp to a C file.
*/
void extract_hull (void);

/**	Write a brush from the map's bsp to a C file.
*/
void extract_model (void);

///@}

#endif//qfbsp_readbsp_h
