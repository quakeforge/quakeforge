/*
	temp_entities.h

	Temporary entity management

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/3/10

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
#ifndef __client_temp_entities_h
#define __client_temp_entities_h

#include "QF/simd/vec4f.h"

typedef enum TE_Effect {
	TE_NoEffect,		// for invalid nq/qw -> qf mapping
	TE_Beam,			// grappling hook beam
	TE_Blood,			// bullet hitting body
	TE_Explosion,		// rocket explosion
	TE_Explosion2,		// color mapped explosion
	TE_Explosion3,		// Nehahra colored light explosion
	TE_Gunshot1,		// NQ gunshot (20 particles)
	TE_Gunshot2,		// QW gunshot (has particle count)
	TE_KnightSpike,		// spike hitting wall
	TE_LavaSplash,
	TE_Lightning1,		// lightning bolts
	TE_Lightning2,		// lightning bolts
	TE_Lightning3,		// lightning bolts
	TE_Lightning4,		// Nehahra lightning
	TE_LightningBlood,	// lightning hitting body
	TE_Spike,			// spike hitting wall
	TE_SuperSpike,		// super spike hitting wall
	TE_TarExplosion,	// tarbaby explosion
	TE_Teleport,
	TE_WizSpike,		// spike hitting wall
} TE_Effect;

typedef enum TE_nqEffect {
	TE_nqSpike,
	TE_nqSuperSpike,
	TE_nqGunshot,
	TE_nqExplosion,
	TE_nqTarExplosion,
	TE_nqLightning1,
	TE_nqLightning2,
	TE_nqWizSpike,
	TE_nqKnightSpike,
	TE_nqLightning3,
	TE_nqLavaSplash,
	TE_nqTeleport,
	TE_nqExplosion2,
	TE_nqBeam,
	TE_nqExplosion3 = 16,
	TE_nqLightning4,
} TE_nqEffect;

typedef enum TE_qwEffect {
	TE_qwSpike,
	TE_qwSuperSpike,
	TE_qwGunshot,
	TE_qwExplosion,
	TE_qwTarExplosion,
	TE_qwLightning1,
	TE_qwLightning2,
	TE_qwWizSpike,
	TE_qwKnightSpike,
	TE_qwLightning3,
	TE_qwLavaSplash,
	TE_qwTeleport,
	TE_qwBlood,
	TE_qwLightningBlood,
	TE_qwExplosion2 = 16,
	TE_qwBeam,
} TE_qwEffect;

//FIXME find a better way to get this info from the parser
typedef struct TEntContext_s {
	vec4f_t     simorg;
	int         playerEntity;
} TEntContext_t;

struct msg_s;
struct entity_s;

void CL_TEnts_Init (void);
void CL_TEnts_Precache (void);
void CL_Init_Entity (struct entity_s ent);
void CL_ClearTEnts (void);
void CL_UpdateTEnts (double time, TEntContext_t *ctx);
void CL_ParseTEnt_nq (struct msg_s *net_message, double time,
					  TEntContext_t *ctx);
void CL_ParseTEnt_qw (struct msg_s *net_message, double time,
					  TEntContext_t *ctx);
void CL_ParseParticleEffect (struct msg_s *net_message);
void CL_ClearProjectiles (void);
void CL_ParseProjectiles (struct msg_s *net_message, qboolean nail2,
						  TEntContext_t *ctx);

#endif//__client_temp_entities_h
