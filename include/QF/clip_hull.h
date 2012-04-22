/*
	clip_hull.h

	dynamic bsp clipping hull management

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/7/28

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

#ifndef __QF_clip_hull_h
#define __QF_clip_hull_h

#include "QF/bspfile.h"

typedef struct clip_hull_s {
	vec3_t      mins;
	vec3_t      maxs;
	vec3_t      axis[3];
	struct hull_s *hulls[MAX_MAP_HULLS];
} clip_hull_t;

clip_hull_t *MOD_Alloc_Hull (int nodes, int planes);
void MOD_Free_Hull (clip_hull_t *ch);

#endif//__QF_clip_hull_h
