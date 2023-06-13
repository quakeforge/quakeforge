/*
	in_win.h

	Win32 input prototypes

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

#ifndef _IN_WIN_H
#define _IN_WIN_H

#include "QF/qtypes.h"

extern bool          mouseactive;
extern float         mouse_x, mouse_y;

void IN_UpdateClipCursor (void);
void IN_ShowMouse (void);
void IN_HideMouse (void);
void IN_ActivateMouse (void);
void IN_DeactivateMouse (void);

#endif // _IN_WIN_H
