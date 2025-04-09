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
#include "QF/simd/types.h"
#include "QF/scene/entity.h"

typedef struct {
	int     destcolor[3];
	int     percent;        // 0-255
	double  time;
	int     initialpct;
} cshift_t;

#define CSHIFT_CONTENTS 0
#define CSHIFT_DAMAGE   1
#define CSHIFT_BONUS    2
#define CSHIFT_POWERUP  3
#define NUM_CSHIFTS     4

#define INFO_CSHIFT_BONUS		(1 << 0)
#define INFO_CSHIFT_CONTENTS	(1 << 1)
#define INFO_CSHIFT_DAMAGE		(1 << 2)
#define INFO_CSHIFT_POWERUP		(1 << 3)

typedef struct viewstate_s {
	vec4f_t     player_origin;
	vec3_t      player_angles;
	int         chase;
	vec4f_t     movecmd;
	vec4f_t     velocity;
	vec4f_t     punchangle;
	transform_t camera_transform;
	double      time;
	double      realtime;
	double      last_servermessage;
	float       frametime;
	float       height;
	int         weaponframe;
	int         onground;		// -1 when in air
	unsigned    active:1;
	unsigned    loading:1;
	unsigned    watervis:1;
	unsigned    demoplayback:1;
	unsigned    drift_enabled:1;
	unsigned    voffs_enabled:1;
	unsigned    bob_enabled:1;
	unsigned    intermission:1;
	unsigned    decay_punchangle:1;
	int         force_cshifts;	// bitfield of server enforced cshifts
	uint32_t    flags;

	int         powerup_index;
	cshift_t    cshifts[NUM_CSHIFTS];	// Color shifts for damage, powerups
	cshift_t    prev_cshifts[NUM_CSHIFTS];	// and content types
	quat_t      cshift_color;

// pitch drifting vars
	float       idealpitch;
	float       pitchvel;
	bool        nodrift;
	float       driftmove;
	double      laststop;

	struct model_s *weapon_model;
	entity_t    weapon_entity;
	entity_t    player_entity;

	struct chasestate_s *chasestate;
} viewstate_t;

#define VF_DEAD 1
#define VF_GIB 2

struct msg_s;
struct scene_s;

void V_NewScene (viewstate_t *vs, struct scene_s *scene);
void V_Init (viewstate_t *vs);
void V_Init_Cvars (void);
void V_RenderView (viewstate_t *vs);
float V_CalcRoll (const vec3_t angles, vec4f_t velocity) __attribute__((pure));
void V_StartPitchDrift (viewstate_t *vs);
void V_StopPitchDrift (viewstate_t *vs);
void V_SetContentsColor (viewstate_t *vs, int contents);
void V_ParseDamage (struct msg_s *net_message, viewstate_t *vs);
void V_PrepBlend (viewstate_t *vs);

extern bool noclip_anglehack;

#endif // __client_view_h
