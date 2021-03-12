/*
	view.h

	(description)

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
// view.h

#ifndef __client_view_h_
#define __client_view_h_

#include "QF/mathlib.h"
#include "QF/simd/types.h"

#define INFO_CSHIFT_BONUS		(1 << 0)
#define INFO_CSHIFT_CONTENTS	(1 << 1)
#define INFO_CSHIFT_DAMAGE		(1 << 2)
#define INFO_CSHIFT_POWERUP		(1 << 3)

void V_Init (void);
void V_Init_Cvars (void);
void V_RenderView (void);
float V_CalcRoll (const vec3_t angles, vec4f_t velocity);
void V_StartPitchDrift (void);
void V_StopPitchDrift (void);

void V_SetContentsColor (int contents);

#endif // __client_view_h_
