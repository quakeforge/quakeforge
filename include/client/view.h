/*
	view.h

	(description)

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
// view.h

#ifndef __client_view_h
#define __client_view_h

#include "QF/mathlib.h"
#include "QF/render.h"
#include "QF/simd/types.h"

#define INFO_CSHIFT_BONUS		(1 << 0)
#define INFO_CSHIFT_CONTENTS	(1 << 1)
#define INFO_CSHIFT_DAMAGE		(1 << 2)
#define INFO_CSHIFT_POWERUP		(1 << 3)

typedef struct viewstate_s {
	vec4f_t     movecmd;
	vec4f_t     velocity;
	vec4f_t     origin;
	vec4f_t     punchangle;
	vec3_t      angles;
	float       frametime;
	struct transform_s *camera_transform;
	double      time;
	float       height;
	int         weaponframe;
	int         onground;		// -1 when in air
	int         active:1;
	int         drift_enabled:1;
	int         voffs_enabled:1;
	int         bob_enabled:1;
	int         intermission:1;
	int         decay_punchangle:1;
	int         force_cshifts;	// bitfield of server enforced cshifts
	uint32_t    flags;

	int         powerup_index;
	cshift_t    cshifts[NUM_CSHIFTS];	// Color shifts for damage, powerups
	cshift_t    prev_cshifts[NUM_CSHIFTS];	// and content types
	quat_t      cshift_color;
	qboolean    cshift_changed;

// pitch drifting vars
	float       idealpitch;
	float       pitchvel;
	qboolean    nodrift;
	float       driftmove;
	double      laststop;

	struct model_s *weapon_model;
	struct entity_s *weapon_entity;
	struct entity_s *player_entity;

	struct chasestate_s *chasestate;
	int         chase;
} viewstate_t;

#define VF_DEAD 1
#define VF_GIB 2

struct msg_s;

void V_Init (viewstate_t *vs);
void V_Init_Cvars (void);
void V_RenderView (viewstate_t *vs);
float V_CalcRoll (const vec3_t angles, vec4f_t velocity);
void V_StartPitchDrift (viewstate_t *vs);
void V_StopPitchDrift (viewstate_t *vs);
void V_SetContentsColor (viewstate_t *vs, int contents);
void V_ParseDamage (struct msg_s *net_message, viewstate_t *vs);
void V_PrepBlend (viewstate_t *vs);

extern qboolean noclip_anglehack;

#endif // __client_view_h
