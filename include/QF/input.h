/*
	input.h

	External (non-keyboard) input devices

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

#ifndef __QF_input_h_
#define __QF_input_h_

#include "QF/keys.h"

typedef struct {
	vec3_t angles;
	vec3_t position;
} viewdelta_t;

extern viewdelta_t viewdelta;

#define freelook (in_mlook.state & 1 || in_freelook->int_val)

struct cvar_s;

void IN_Init (struct cbuf_s *cbuf);
void IN_Init_Cvars (void);

void IN_ProcessEvents (void);

void IN_UpdateGrab (struct cvar_s *);

void IN_ClearStates (void);

void IN_Move (void); // FIXME: was cmduser_t?
// add additional movement on top of the keyboard move cmd

extern struct cvar_s		*in_grab;
extern struct cvar_s		*in_amp;
extern struct cvar_s		*in_pre_amp;
extern struct cvar_s		*m_filter;
extern struct cvar_s		*in_mouse_accel;
extern struct cvar_s		*in_freelook;
extern struct cvar_s		*lookstrafe;

extern qboolean 	in_mouse_avail;
extern float		in_mouse_x, in_mouse_y;

void IN_LL_Init_Cvars (void);
void IN_LL_Init (void);
void IN_LL_Shutdown (void);
void IN_LL_ProcessEvents (void);
void IN_LL_ClearStates (void);
void IN_LL_Grab_Input (int grab);

extern kbutton_t   in_strafe, in_klook, in_speed, in_mlook;

#endif // __QF_input_h_
