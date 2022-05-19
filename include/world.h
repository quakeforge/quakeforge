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

*/

#ifndef __world_h
#define __world_h

#include "QF/link.h"
#include "QF/mathlib.h"
#include "QF/model.h"

typedef struct {
	qboolean    present;
	vec3_t      laggedpos;
} laggedentinfo_t;

typedef enum {
	tr_point,
	tr_box,
	tr_ellipsoid,
} trace_e;

typedef struct trace_s {
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	qboolean	inopen, inwater;
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		extents;	// 1/2 size of traced box
	trace_e		type;		// type of trace to perform
	vec3_t		endpos;		// final position
	plane_t		plane;		// surface normal at impact
	struct edict_s *ent;	// entity the surface is on
	unsigned    contents;	// contents of leafs touched by trace
} trace_t;


#define	MOVE_NORMAL		0
#define	MOVE_NOMONSTERS	1
#define	MOVE_MISSILE	2
#define MOVE_HITMODEL   4
#define MOVE_RESERVED   8
#define MOVE_TRIGGERS	16 //triggers must be marked with FINDABLE_NONSOLID    (an alternative to solid-corpse)
#define MOVE_EVERYTHING 32 //can return triggers and non-solid items if they're marked with FINDABLE_NONSOLID (works even if the items are not properly linked)
#define MOVE_LAGGED     64 //trace touches current last-known-state, instead of actual ents (just affects players for now)
#define MOVE_ENTCHAIN	128 //chain of impacted ents, otherwise result shows only world

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

void SV_FreeAllEdictLeafs (void);

void SV_InitHull (hull_t *hull, dclipnode_t *clipnodes, plane_t *planes);

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

int SV_PointContents (const vec3_t p) __attribute__((pure));
int SV_TruePointContents (const vec3_t p) __attribute__((pure));
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

int SV_HullPointContents (hull_t *hull, int num, const vec3_t p) __attribute__((pure));
hull_t *SV_HullForEntity (struct edict_s *ent, const vec3_t mins,
						  const vec3_t maxs, vec3_t extents, vec3_t offset);
void MOD_TraceLine (hull_t *hull, int num,
					const vec3_t start, const vec3_t end, trace_t *trace);
int MOD_HullContents (hull_t *hull, int num, const vec3_t origin,
					  trace_t *trace);

typedef struct clipport_s {
	int         planenum;
	struct clipport_s *next[2];		///< front, back
	struct clipleaf_s *leafs[2];	///< front, back
	struct winding_s *winding;
	struct winding_s *edges;		///< unit vectors along edges
} clipport_t;

typedef struct clipleaf_s {
	clipport_t *portals;
	int         contents;
	int         test_count;
} clipleaf_t;

typedef struct nodeleaf_s {
	clipleaf_t *leafs[2];	///< front, back. If null, node's child is a node
} nodeleaf_t;

nodeleaf_t *MOD_BuildBrushes (hull_t *hull);
void MOD_FreeBrushes (hull_t *hull);

#endif // __world_h
