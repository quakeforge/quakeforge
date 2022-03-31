/*
	qf_sprite.h

	GLSL specific sprite model stuff

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/7/22

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
#ifndef __QF_GLSL_qf_sprite_h
#define __QF_GLSL_qf_sprite_h

struct entity_s;
void glsl_R_DrawSprite (struct entity_s *ent);
void glsl_R_SpriteBegin (void);
void glsl_R_SpriteEnd (void);
void glsl_R_InitSprites (void);

#endif//__QF_GLSL_qf_sprite_h
