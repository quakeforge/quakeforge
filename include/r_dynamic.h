/*
	render.h

	public interface to refresh functions

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

#ifndef _R_DYNAMIC_H
#define _R_DYNAMIC_H

#include "QF/mathlib.h"

#include "d_iface.h"

typedef enum {
    PE_UNKNOWN,
    PE_GUNSHOT,
    PE_BLOOD,
    PE_LIGHTNINGBLOOD,
    PE_SPIKE,
    PE_SUPERSPIKE,
    PE_KNIGHTSPIKE,
    PE_WIZSPIKE,
} particle_effect_t;

struct entity_s;

void R_PushDlights (const vec3_t entorigin);
void R_MaxDlightsCheck (int max_dlights);
void R_Particles_Init_Cvars (void);
void R_Particle_New (const char *type, int texnum, const vec3_t org,
					 float scale, const vec3_t vel, float die, int color,
					 float alpha, float ramp);
void R_Particle_NewRandom (const char *type, int texnum, const vec3_t org,
						   int org_fuzz, float scale, int vel_fuzz, float die,
						   int color, float alpha, float ramp);
void R_InitBubble (void);

void R_InitParticles (void);
void R_ClearParticles (void);
void R_InitSprites (void);

#endif // _R_DYNAMIC_H
