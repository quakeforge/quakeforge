/*
	effects.h

	Effect management

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/3/11

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
#ifndef __client_effects_h
#define __client_effects_h

struct entity_s;
struct entity_state_s;

void CL_NewDlight (int key, vec3_t org, int effects, byte glow_size,
				   byte glow_color, double time);
void CL_ModelEffects (struct entity_s *ent, int num, int glow_color,
					  double time);
void CL_EntityEffects (int num, struct entity_s *ent,
					   struct entity_state_s *state, double time);

#endif//__client_effects_h
