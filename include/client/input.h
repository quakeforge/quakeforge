/*
	input.h

	Client input handling

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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
#ifndef __client_input_h_
#define __client_input_h_

#include "QF/simd/vec4f.h"

#include "QF/input.h"

struct cbuf_s;

extern struct cvar_s *cl_upspeed;
extern struct cvar_s *cl_forwardspeed;
extern struct cvar_s *cl_backspeed;
extern struct cvar_s *cl_sidespeed;

extern struct cvar_s *cl_movespeedkey;

extern struct cvar_s *cl_yawspeed;
extern struct cvar_s *cl_pitchspeed;

extern struct cvar_s *cl_anglespeedkey;


#define FORWARD 0
#define SIDE 1
#define UP 2

typedef struct movestate_s {
	vec4f_t     move;
	vec4f_t     angles;
} movestate_t;

#define freelook (in_mlook.state & 1 || in_freelook->int_val)

void CL_OnFocusChange (void (*func) (int game));
void CL_Input_BuildMove (float frametime, movestate_t *state);
void CL_Input_Init (struct cbuf_s *cbuf);
void CL_Input_Init_Cvars (void);
void CL_Input_Activate (int in_game);

extern in_button_t  in_left, in_right, in_forward, in_back;
extern in_button_t  in_lookup, in_lookdown, in_moveleft, in_moveright;
extern in_button_t  in_use, in_jump, in_attack;
extern in_button_t  in_up, in_down;
extern in_button_t  in_strafe, in_klook, in_speed, in_mlook;

#endif // __client_input_h_
