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

	$Id$
*/

#ifndef _INPUT_H
#define _INPUT_H

#include "QF/cvar.h"

struct {
	vec3_t angles;
	vec3_t position;
} viewdelta;

#define freelook (in_mlook.state&1 || in_freelook->int_val)

void IN_Init (void);
void IN_Init_Cvars (void);

void IN_Shutdown (void);

void IN_Commands (void);
// oportunity for devices to stick commands on the script buffer

void IN_SendKeyEvents (void);
// Perform Key_Event () callbacks until the input que is empty

void IN_Move (void); // FIXME: was cmduser_t?
// add additional movement on top of the keyboard move cmd

void IN_ModeChanged (void);
// called whenever screen dimensions change

void IN_HandlePause (qboolean paused);

extern cvar_t		*_windowed_mouse;
extern cvar_t		*m_filter;
extern cvar_t		*in_freelook;
extern cvar_t		*sensitivity;
extern cvar_t		*lookstrafe;

extern qboolean    in_mouse_avail;
extern float       in_mouse_x, in_mouse_y;


void IN_LL_Init_Cvars ();
void IN_LL_Init ();
void IN_LL_Shutdown ();
void IN_LL_Commands ();
void IN_LL_SendKeyEvents ();
void IN_LL_ClearStates ();

#endif // _INPUT_H
