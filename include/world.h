/*
	world.h

	@description@

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

#ifndef __world_h
#define __world_h

#include "QF/link.h"
#include "QF/mathlib.h"
#include "QF/model.h"

typedef struct
{
	vec3_t	normal;
	vec_t	dist;
} plane_t;

typedef struct trace_s {
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	qboolean	inopen, inwater;
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		extents;	// 1/2 size of traced box
	qboolean	isbox;		// box or point
	vec3_t		endpos;		// final position
	plane_t		plane;		// surface normal at impact
	struct edict_s *ent;	// entity the surface is on
} trace_t;

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON    (0.03125)

#define	MOVE_NORMAL		0
#define	MOVE_NOMONSTERS	1
#define	MOVE_MISSILE	2

typedef struct areanode_s {
	int		axis;		// -1 = leaf node
	float	dist;
	struct areanode_s	*children[2];
	link_t	trigger_edicts;
	link_t	solid_edicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32

extern	areanode_t	sv_areanodes[AREA_NODES];

void SV_InitHull (hull_t *hull, dclipnode_t *clipnodes, mplane_t *planes);

void SV_ClearWorld (void);
// called after the world model has been loaded, before linking any entities

void SV_UnlinkEdict (struct edict_s *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself
// flags ent->v.modified

void SV_LinkEdict (struct edict_s *ent, qboolean touch_triggers);
// Needs to be called any time an entity changes origin, mins, maxs, or solid
// flags ent->v.modified
// sets ent->v.absmin and ent->v.absmax
// if touchtriggers, calls prog functions for the intersected triggers

int SV_PointContents (const vec3_t p);
int SV_TruePointContents (const vec3_t p);
// returns the CONTENTS_* value from the world at the given point.
// does not check any entities at all
// the non-true version remaps the water current contents to content_water

struct edict_s	*SV_TestEntityPosition (struct edict_s *ent);

trace_t SV_Move (const vec3_t start, const vec3_t mins, const vec3_t maxs,
				 const vec3_t end, int type, struct edict_s *passedict);
// mins and maxs are reletive

// if the entire move stays in a solid volume, trace.allsolid will be set

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// nomonsters is used for line of sight or edge testing, where mosnters
// shouldn't be considered solid objects

// passedict is explicitly excluded from clipping checks (normally NULL)

struct edict_s	*SV_TestPlayerPosition (struct edict_s *ent,
										const vec3_t origin);

int SV_HullPointContents (hull_t *hull, int num, const vec3_t p);
hull_t *SV_HullForEntity (struct edict_s *ent, const vec3_t mins,
						  const vec3_t maxs, vec3_t extents, vec3_t offset);
void MOD_TraceLine (hull_t *hull, int num,
					const vec3_t start, const vec3_t end, trace_t *trace);

#endif // __world_h
