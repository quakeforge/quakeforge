/*
	r_screen.h

	shared screen declarations

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/12/11

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

#ifndef __r_screen_h
#define __r_screen_h

struct tex_s *SCR_CaptureBGR (void);
struct tex_s *SCR_ScreenShot (unsigned width, unsigned height);
void SCR_DrawStringToSnap (const char *s, struct tex_s *tex, int x, int y);
void SCR_ScreenShot_f (void);
int MipColor (int r, int g, int b);

extern int         scr_copytop;
extern qboolean    scr_skipupdate;

#endif//__r_screen_h
