/*
	pmove.h

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

#ifndef _PMOVE_H
#define _PMOVE_H

#include "qw/protocol.h"
#include "QF/mathlib.h"
#include "QF/model.h"

#include "world.h"

//FIXME location
#define STOP_EPSILON 0.1

#define	MAX_PHYSENTS	(MAX_CLIENTS + MAX_PACKET_ENTITIES)
typedef struct {
	vec3_t	origin;
	vec3_t	angles;
	model_t	*model;		// only for bsp models
	vec3_t	mins, maxs;	// only for non-bsp models
	hull_t	*hull;		// hey, magic :)
	int		info;		// for client or server to identify
} physent_t;


typedef struct {
	int         sequence;	// just for debugging prints

	// player state
	vec3_t      origin;
	vec3_t      angles;
	vec3_t      velocity;
	int         oldbuttons;
	int         oldonground;
	float       waterjumptime;
	bool        dead;
	bool        flying;
	bool        add_grav;
	int         spectator;

	// world state
	int         numphysent;
	physent_t   physents[MAX_PHYSENTS];	// 0 should be the world

	// input
	usercmd_t   cmd;

	// results
	int         numtouch;
	physent_t  *touchindex[MAX_PHYSENTS];
} playermove_t;

typedef struct {
	float	gravity;
	float	stopspeed;
	float	maxspeed;
	float	spectatormaxspeed;
	float	accelerate;
	float	airaccelerate;
	float	wateraccelerate;
	float	friction;
	float	waterfriction;
	float	entgravity;
} movevars_t;

extern int no_pogo_stick;
extern	movevars_t		movevars;
extern	playermove_t	pmove;
extern	int		onground;
extern	int		waterlevel;
extern	int		watertype;

extern vec3_t	player_mins;
extern vec3_t	player_maxs;

void PM_Accelerate (vec3_t wishdir, float wishspeed, float accel);
void PM_CategorizePosition (void);
void PM_InitBoxHull (void);

void PlayerMove (void);
void Pmove_Init (void);
void Pmove_Init_Cvars (void);

int PM_HullPointContents (hull_t *hull, int num, const vec3_t p) __attribute__((pure));

int PM_PointContents (const vec3_t point) __attribute__((pure));
bool PM_TestPlayerPosition (const vec3_t point);
trace_t PM_PlayerMove (const vec3_t start, const vec3_t stop);

#endif // _PMOVE_H
