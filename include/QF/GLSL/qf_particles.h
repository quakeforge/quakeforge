/*
	qf_particles.h

	GLSL specific particles stuff

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/15

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
#ifndef __QF_GLSL_qf_particles_h
#define __QF_GLSL_qf_particles_h

#include "QF/GLSL/types.h"

typedef struct {
	float       texcoord[2];
	float       vertex[3];
	byte        color[4];
} partvert_t;

struct psystem_s;
void glsl_R_DrawParticles (struct psystem_s *psystem);
void glsl_R_Particles_Init_Cvars (void);
void glsl_R_InitParticles (void);
void glsl_R_ShutdownParticles (void);

void glsl_R_InitTrails (void);
void glsl_R_ShutdownTrails (void);
void glsl_R_DrawTrails (struct psystem_s *psystem);

#endif//__QF_GLSL_qf_particles_h
