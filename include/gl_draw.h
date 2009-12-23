/*
	gl_draw.h

	gl specific draw function

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

	$Id: draw.h 10910 2004-05-07 03:54:35Z taniwha $
*/

#ifndef __gl_draw_h
#define __gl_draw_h

void GL_Set2D (void);
void GL_Set2DScaled (void);
void GL_DrawReset (void);
void GL_FlushText (void);

#endif//__gl_draw_h
