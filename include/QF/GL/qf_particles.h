/*
	qf_particles.h

	GL specific particles stuff

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
#ifndef __QF_GL_qf_particles_h
#define __QF_GL_qf_particles_h

#include "QF/GL/types.h"

typedef struct {
	float       texcoord[2];
	float       vertex[3];
	byte        color[4];
} partvert_t;

struct psystem_s;
void gl_R_DrawParticles (struct psystem_s *pssystem);
void gl_R_Particles_Init_Cvars (void);
void gl_R_InitParticles (void);

#endif//__QF_GL_qf_particles_h
