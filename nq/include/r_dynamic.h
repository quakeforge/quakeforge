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

	$Id$
*/

#ifndef _R_DYNAMIC_H
#define _R_DYNAMIC_H

#include "QF/mathlib.h"

void R_ParseParticleEffect (void);
struct entity_s;
void R_RocketTrail (int type, struct entity_s *ent);
void R_RunParticleEffect (vec3_t org, int color, int count);
void R_RunPuffEffect (vec3_t org, byte type, byte count);
void R_RunSpikeEffect (vec3_t org, byte type);

#ifdef QUAKE2
void R_DarkFieldParticles (entity_t *ent);
#endif
void R_EntityParticles (entity_t *ent);
void R_BlobExplosion (vec3_t org);
void R_ParticleExplosion (vec3_t org);
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void R_LavaSplash (vec3_t org);
void R_TeleportSplash (vec3_t org);
void R_PushDlights (vec3_t entorigin);

void R_InitParticles (void);
void R_ClearParticles (void);
void R_DrawParticles (void);

#endif // _R_DYNAMIC_H
