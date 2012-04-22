/*
	hl_bsp.h

	Half Life file definitions

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/


#ifndef _HL_BSP_H
#define _HL_BSP_H

#include "QF/qtypes.h"

extern void CL_ParseEntityLump(const char *entdata);
extern void HL_Mod_LoadLighting (lump_t *l);
extern void HL_Mod_LoadTextures (lump_t *l);
extern byte *W_GetTexture(const char *name, int matchwidth, int matchheight);
extern void W_LoadTextureWadFile (const char *filename, int complain);

#endif	// _HL_BSP_H
