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

extern float cl_upspeed;
extern float cl_forwardspeed;
extern float cl_backspeed;
extern float cl_sidespeed;

extern float cl_movespeedkey;

extern float cl_yawspeed;
extern float cl_pitchspeed;

extern float cl_anglespeedkey;

extern float m_pitch;
extern float m_yaw;
extern float m_forward;
extern float m_side;

#define FORWARD 0
#define SIDE 1
#define UP 2

typedef struct movestate_s {
	vec4f_t     move;
	vec4f_t     angles;
} movestate_t;

#define freelook (in_mlook.state & 1 || in_freelook)

struct viewstate_s;

void CL_OnFocusChange (void (*func) (int game));
void CL_Input_BuildMove (float frametime, movestate_t *state,
						 struct viewstate_s *vs);
void CL_Input_Init (struct cbuf_s *cbuf);
void CL_Input_Init_Cvars (void);
void CL_Input_Activate (int in_game);

extern in_axis_t in_move_forward, in_move_side, in_move_up;
extern in_axis_t in_move_pitch, in_move_yaw, in_move_roll;
extern in_axis_t in_cam_forward, in_cam_side, in_cam_up;
extern in_button_t  in_left, in_right, in_forward, in_back;
extern in_button_t  in_lookup, in_lookdown, in_moveleft, in_moveright;
extern in_button_t  in_use, in_jump, in_attack;
extern in_button_t  in_up, in_down;
extern in_button_t  in_strafe, in_klook, in_speed, in_mlook;

#endif // __client_input_h_
