/*
	sv_phys.c

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "server.h"
#include "host.h"
#include "world.h"
#include "console.h"
#include "sv_progs.h"
#include "sys.h"

/*


pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects 

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/

cvar_t     *sv_friction;
cvar_t     *sv_stopspeed;
cvar_t     *sv_gravity;
cvar_t     *sv_maxvelocity;
cvar_t     *sv_nostep;

#ifdef QUAKE2
static vec3_t vec_origin = { 0.0, 0.0, 0.0 };
#endif

#define	MOVE_EPSILON	0.01

void        SV_Physics_Toss (edict_t *ent);

/*
================
SV_CheckAllEnts
================
*/
void
SV_CheckAllEnts (void)
{
	int         e;
	edict_t    *check;

// see if any solid entities are inside the final position
	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts;
		 e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)
			continue;
		if (SVFIELD (check, movetype, float) == MOVETYPE_PUSH
			|| SVFIELD (check, movetype, float) == MOVETYPE_NONE
#ifdef QUAKE2
			|| SVFIELD (check, movetype, float) == MOVETYPE_FOLLOW
#endif
			|| SVFIELD (check, movetype, float) == MOVETYPE_NOCLIP)
			continue;

		if (SV_TestEntityPosition (check))
			Con_Printf ("entity in invalid position\n");
	}
}

/*
================
SV_CheckVelocity
================
*/
void
SV_CheckVelocity (edict_t *ent)
{
	int         i;

//
// bound velocity
//
	for (i = 0; i < 3; i++) {
		if (IS_NAN (SVFIELD (ent, velocity, vector)[i])) {
			Con_Printf ("Got a NaN velocity on %s\n",
						PR_GetString (&sv_pr_state, SVFIELD (ent, classname, string)));
			SVFIELD (ent, velocity, vector)[i] = 0;
		}
		if (IS_NAN (SVFIELD (ent, origin, vector)[i])) {
			Con_Printf ("Got a NaN origin on %s\n",
						PR_GetString (&sv_pr_state, SVFIELD (ent, classname, string)));
			SVFIELD (ent, origin, vector)[i] = 0;
		}
		if (SVFIELD (ent, velocity, vector)[i] > sv_maxvelocity->value)
			SVFIELD (ent, velocity, vector)[i] = sv_maxvelocity->value;
		else if (SVFIELD (ent, velocity, vector)[i] < -sv_maxvelocity->value)
			SVFIELD (ent, velocity, vector)[i] = -sv_maxvelocity->value;
	}
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
qboolean
SV_RunThink (edict_t *ent)
{
	float       thinktime;

	thinktime = SVFIELD (ent, nextthink, float);
	if (thinktime <= 0 || thinktime > sv.time + host_frametime)
		return true;

	if (thinktime < sv.time)
		thinktime = sv.time;			// don't let things stay in the past.
	// it is possible to start that way
	// by a trigger with a local time.
	SVFIELD (ent, nextthink, float) = 0;
	*sv_globals.time = thinktime;
	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
	*sv_globals.other =
		EDICT_TO_PROG (&sv_pr_state, sv.edicts);
	PR_ExecuteProgram (&sv_pr_state, SVFIELD (ent, think, func));
	return !ent->free;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void
SV_Impact (edict_t *e1, edict_t *e2)
{
	int         old_self, old_other;

	old_self = *sv_globals.self;
	old_other = *sv_globals.other;

	*sv_globals.time = sv.time;
	if (SVFIELD (e1, touch, func) && SVFIELD (e1, solid, float) != SOLID_NOT) {
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, e1);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, e2);
		PR_ExecuteProgram (&sv_pr_state, SVFIELD (e1, touch, func));
	}

	if (SVFIELD (e2, touch, func) && SVFIELD (e2, solid, float) != SOLID_NOT) {
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, e2);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, e1);
		PR_ExecuteProgram (&sv_pr_state, SVFIELD (e2, touch, func));
	}

	*sv_globals.self = old_self;
	*sv_globals.other = old_other;
}


/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
#define	STOP_EPSILON	0.1

int
ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float       backoff;
	float       change;
	int         i, blocked;

	blocked = 0;
	if (normal[2] > 0)
		blocked |= 1;					// floor
	if (!normal[2])
		blocked |= 2;					// step

	backoff = DotProduct (in, normal) * overbounce;

	for (i = 0; i < 3; i++) {
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	return blocked;
}


/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If steptrace is not NULL, the trace of any vertical wall hit will be stored
============
*/
#define	MAX_CLIP_PLANES	5
int
SV_FlyMove (edict_t *ent, float time, trace_t *steptrace)
{
	int         bumpcount, numbumps;
	vec3_t      dir;
	float       d;
	int         numplanes;
	vec3_t      planes[MAX_CLIP_PLANES];
	vec3_t      primal_velocity, original_velocity, new_velocity;
	int         i, j;
	trace_t     trace;
	vec3_t      end;
	float       time_left;
	int         blocked;

	numbumps = 4;

	blocked = 0;
	VectorCopy (SVFIELD (ent, velocity, vector), original_velocity);
	VectorCopy (SVFIELD (ent, velocity, vector), primal_velocity);
	numplanes = 0;

	time_left = time;

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
		if (!SVFIELD (ent, velocity, vector)[0] && !SVFIELD (ent, velocity, vector)[1]
			&& !SVFIELD (ent, velocity, vector)[2])
			break;

		for (i = 0; i < 3; i++)
			end[i] = SVFIELD (ent, origin, vector)[i] + time_left * SVFIELD (ent, velocity, vector)[i];

		trace =
			SV_Move (SVFIELD (ent, origin, vector), SVFIELD (ent, mins, vector), SVFIELD (ent, maxs, vector), end, false,
					 ent);

		if (trace.allsolid) {			// entity is trapped in another solid
			VectorCopy (vec3_origin, SVFIELD (ent, velocity, vector));
			return 3;
		}

		if (trace.fraction > 0) {		// actually covered some distance
			VectorCopy (trace.endpos, SVFIELD (ent, origin, vector));
			VectorCopy (SVFIELD (ent, velocity, vector), original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break;						// moved the entire distance

		if (!trace.ent)
			Sys_Error ("SV_FlyMove: !trace.ent");

		if (trace.plane.normal[2] > 0.7) {
			blocked |= 1;				// floor
			if (SVFIELD (trace.ent, solid, float) == SOLID_BSP) {
				SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) | FL_ONGROUND;
				SVFIELD (ent, groundentity, entity) = EDICT_TO_PROG (&sv_pr_state, trace.ent);
			}
		}
		if (!trace.plane.normal[2]) {
			blocked |= 2;				// step
			if (steptrace)
				*steptrace = trace;		// save for player extrafriction
		}
//
// run the impact function
//
		SV_Impact (ent, trace.ent);
		if (ent->free)
			break;						// removed by the impact function


		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES) {	// this shouldn't really happen
			VectorCopy (vec3_origin, SVFIELD (ent, velocity, vector));
			return 3;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

//
// modify original_velocity so it parallels all of the clip planes
//
		for (i = 0; i < numplanes; i++) {
			ClipVelocity (original_velocity, planes[i], new_velocity, 1);
			for (j = 0; j < numplanes; j++)
				if (j != i) {
					if (DotProduct (new_velocity, planes[j]) < 0)
						break;			// not ok
				}
			if (j == numplanes)
				break;
		}

		if (i != numplanes) {			// go along this plane
			VectorCopy (new_velocity, SVFIELD (ent, velocity, vector));
		} else {						// go along the crease
			if (numplanes != 2) {
//              Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
				VectorCopy (vec3_origin, SVFIELD (ent, velocity, vector));
				return 7;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, SVFIELD (ent, velocity, vector));
			VectorScale (dir, d, SVFIELD (ent, velocity, vector));
		}

//
// if original velocity is against the original velocity, stop dead
// to avoid tiny occilations in sloping corners
//
		if (DotProduct (SVFIELD (ent, velocity, vector), primal_velocity) <= 0) {
			VectorCopy (vec3_origin, SVFIELD (ent, velocity, vector));
			return blocked;
		}
	}

	return blocked;
}


/*
============
SV_AddGravity

============
*/
void
SV_AddGravity (edict_t *ent)
{
	float       ent_gravity;

#ifdef QUAKE2
	if (SVFIELD (ent, gravity, float))
		ent_gravity = SVFIELD (ent, gravity, float);
	else
		ent_gravity = 1.0;
#else
	eval_t     *val;

	val = GetEdictFieldValue (&sv_pr_state, ent, "gravity");
	if (val && val->_float)
		ent_gravity = val->_float;
	else
		ent_gravity = 1.0;
#endif
	SVFIELD (ent, velocity, vector)[2] -= ent_gravity * sv_gravity->value * host_frametime;
}


/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t
SV_PushEntity (edict_t *ent, vec3_t push)
{
	trace_t     trace;
	vec3_t      end;

	VectorAdd (SVFIELD (ent, origin, vector), push, end);

	if (SVFIELD (ent, movetype, float) == MOVETYPE_FLYMISSILE)
		trace =
			SV_Move (SVFIELD (ent, origin, vector), SVFIELD (ent, mins, vector), SVFIELD (ent, maxs, vector), end,
					 MOVE_MISSILE, ent);
	else if (SVFIELD (ent, solid, float) == SOLID_TRIGGER || SVFIELD (ent, solid, float) == SOLID_NOT)
		// only clip against bmodels
		trace =
			SV_Move (SVFIELD (ent, origin, vector), SVFIELD (ent, mins, vector), SVFIELD (ent, maxs, vector), end,
					 MOVE_NOMONSTERS, ent);
	else
		trace =
			SV_Move (SVFIELD (ent, origin, vector), SVFIELD (ent, mins, vector), SVFIELD (ent, maxs, vector), end,
					 MOVE_NORMAL, ent);

	VectorCopy (trace.endpos, SVFIELD (ent, origin, vector));
	SV_LinkEdict (ent, true);

	if (trace.ent)
		SV_Impact (ent, trace.ent);

	return trace;
}


/*
============
SV_PushMove

============
*/
void
SV_PushMove (edict_t *pusher, float movetime)
{
	int         i, e;
	edict_t    *check, *block;
	vec3_t      mins, maxs, move;
	vec3_t      entorig, pushorig;
	int         num_moved;
	edict_t    *moved_edict[MAX_EDICTS];
	vec3_t      moved_from[MAX_EDICTS];

	if (!SVFIELD (pusher, velocity, vector)[0] && !SVFIELD (pusher, velocity, vector)[1]
		&& !SVFIELD (pusher, velocity, vector)[2]) {
		SVFIELD (pusher, ltime, float) += movetime;
		return;
	}

	for (i = 0; i < 3; i++) {
		move[i] = SVFIELD (pusher, velocity, vector)[i] * movetime;
		mins[i] = SVFIELD (pusher, absmin, vector)[i] + move[i];
		maxs[i] = SVFIELD (pusher, absmax, vector)[i] + move[i];
	}

	VectorCopy (SVFIELD (pusher, origin, vector), pushorig);

// move the pusher to it's final position

	VectorAdd (SVFIELD (pusher, origin, vector), move, SVFIELD (pusher, origin, vector));
	SVFIELD (pusher, ltime, float) += movetime;
	SV_LinkEdict (pusher, false);


// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts;
		 e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)
			continue;
		if (SVFIELD (check, movetype, float) == MOVETYPE_PUSH
			|| SVFIELD (check, movetype, float) == MOVETYPE_NONE
#ifdef QUAKE2
			|| SVFIELD (check, movetype, float) == MOVETYPE_FOLLOW
#endif
			|| SVFIELD (check, movetype, float) == MOVETYPE_NOCLIP)
			continue;

		// if the entity is standing on the pusher, it will definately be
		// moved
		if (!(((int) SVFIELD (check, flags, float) & FL_ONGROUND)
			  && PROG_TO_EDICT (&sv_pr_state,
								SVFIELD (check, groundentity, entity)) == pusher)) {
			if (SVFIELD (check, absmin, vector)[0] >= maxs[0]
				|| SVFIELD (check, absmin, vector)[1] >= maxs[1]
				|| SVFIELD (check, absmin, vector)[2] >= maxs[2]
				|| SVFIELD (check, absmax, vector)[0] <= mins[0]
				|| SVFIELD (check, absmax, vector)[1] <= mins[1]
				|| SVFIELD (check, absmax, vector)[2] <= mins[2])
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition (check))
				continue;
		}
		// remove the onground flag for non-players
		if (SVFIELD (check, movetype, float) != MOVETYPE_WALK)
			SVFIELD (check, flags, float) = (int) SVFIELD (check, flags, float) & ~FL_ONGROUND;

		VectorCopy (SVFIELD (check, origin, vector), entorig);
		VectorCopy (SVFIELD (check, origin, vector), moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		// try moving the contacted entity 
		SVFIELD (pusher, solid, float) = SOLID_NOT;
		SV_PushEntity (check, move);
		SVFIELD (pusher, solid, float) = SOLID_BSP;

		// if it is still inside the pusher, block
		block = SV_TestEntityPosition (check);
		if (block) {					// fail the move
			if (SVFIELD (check, mins, vector)[0] == SVFIELD (check, maxs, vector)[0])
				continue;
			if (SVFIELD (check, solid, float) == SOLID_NOT || SVFIELD (check, solid, float) == SOLID_TRIGGER) {	// corpse
				SVFIELD (check, mins, vector)[0] = SVFIELD (check, mins, vector)[1] = 0;
				VectorCopy (SVFIELD (check, mins, vector), SVFIELD (check, maxs, vector));
				continue;
			}

			VectorCopy (entorig, SVFIELD (check, origin, vector));
			SV_LinkEdict (check, true);

			VectorCopy (pushorig, SVFIELD (pusher, origin, vector));
			SV_LinkEdict (pusher, false);
			SVFIELD (pusher, ltime, float) -= movetime;

			// if the pusher has a "blocked" function, call it
			// otherwise, just stay in place until the obstacle is gone
			if (SVFIELD (pusher, blocked, func)) {
				*sv_globals.self =
					EDICT_TO_PROG (&sv_pr_state, pusher);
				*sv_globals.other =
					EDICT_TO_PROG (&sv_pr_state, check);
				PR_ExecuteProgram (&sv_pr_state, SVFIELD (pusher, blocked, func));
			}
			// move back any entities we already moved
			for (i = 0; i < num_moved; i++) {
				VectorCopy (moved_from[i], SVFIELD (moved_edict[i], origin, vector));
				SV_LinkEdict (moved_edict[i], false);
			}
			return;
		}
	}


}

#ifdef QUAKE2
/*
============
SV_PushRotate

============
*/
void
SV_PushRotate (edict_t *pusher, float movetime)
{
	int         i, e;
	edict_t    *check, *block;
	vec3_t      move, a, amove;
	vec3_t      entorig, pushorig;
	int         num_moved;
	edict_t    *moved_edict[MAX_EDICTS];
	vec3_t      moved_from[MAX_EDICTS];
	vec3_t      org, org2;
	vec3_t      forward, right, up;

	if (!SVFIELD (pusher, avelocity, float)[0] && !SVFIELD (pusher, avelocity, float)[1]
		&& !SVFIELD (pusher, avelocity, float)[2]) {
		SVFIELD (pusher, ltime, float) += movetime;
		return;
	}

	for (i = 0; i < 3; i++)
		amove[i] = SVFIELD (pusher, avelocity, float)[i] * movetime;

	VectorSubtract (vec3_origin, amove, a);
	AngleVectors (a, forward, right, up);

	VectorCopy (SVFIELD (pusher, angles, float), pushorig);

// move the pusher to it's final position

	VectorAdd (SVFIELD (pusher, angles, float), amove, SVFIELD (pusher, angles, float));
	SVFIELD (pusher, ltime, float) += movetime;
	SV_LinkEdict (pusher, false);


// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts;
		 e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)
			continue;
		if (SVFIELD (check, movetype, float) == MOVETYPE_PUSH
			|| SVFIELD (check, movetype, float) == MOVETYPE_NONE
			|| SVFIELD (check, movetype, float) == MOVETYPE_FOLLOW
			|| SVFIELD (check, movetype, float) == MOVETYPE_NOCLIP) continue;

		// if the entity is standing on the pusher, it will definately be
		// moved
		if (!(((int) SVFIELD (check, flags, float) & FL_ONGROUND)
			  && PROG_TO_EDICT (&sv_pr_state,
								SVFIELD (check, groundentity, float)) == pusher)) {
			if (SVFIELD (check, absmin, float)[0] >= SVFIELD (pusher, absmax, float)[0]
				|| SVFIELD (check, absmin, float)[1] >= SVFIELD (pusher, absmax, float)[1]
				|| SVFIELD (check, absmin, float)[2] >= SVFIELD (pusher, absmax, float)[2]
				|| SVFIELD (check, absmax, float)[0] <= SVFIELD (pusher, absmin, float)[0]
				|| SVFIELD (check, absmax, float)[1] <= SVFIELD (pusher, absmin, float)[1]
				|| SVFIELD (check, absmax, float)[2] <= SVFIELD (pusher, absmin, float)[2])
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition (check))
				continue;
		}
		// remove the onground flag for non-players
		if (SVFIELD (check, movetype, float) != MOVETYPE_WALK)
			SVFIELD (check, flags, float) = (int) SVFIELD (check, flags, float) & ~FL_ONGROUND;

		VectorCopy (SVFIELD (check, origin, float), entorig);
		VectorCopy (SVFIELD (check, origin, float), moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		// calculate destination position
		VectorSubtract (SVFIELD (check, origin, float), SVFIELD (pusher, origin, float), org);
		org2[0] = DotProduct (org, forward);
		org2[1] = -DotProduct (org, right);
		org2[2] = DotProduct (org, up);
		VectorSubtract (org2, org, move);

		// try moving the contacted entity 
		SVFIELD (pusher, solid, float) = SOLID_NOT;
		SV_PushEntity (check, move);
		SVFIELD (pusher, solid, float) = SOLID_BSP;

		// if it is still inside the pusher, block
		block = SV_TestEntityPosition (check);
		if (block) {					// fail the move
			if (SVFIELD (check, mins, float)[0] == SVFIELD (check, maxs, float)[0])
				continue;
			if (SVFIELD (check, solid, float) == SOLID_NOT || SVFIELD (check, solid, float) == SOLID_TRIGGER) {	// corpse
				SVFIELD (check, mins, float)[0] = SVFIELD (check, mins, float)[1] = 0;
				VectorCopy (SVFIELD (check, mins, float), SVFIELD (check, maxs, float));
				continue;
			}

			VectorCopy (entorig, SVFIELD (check, origin, float));
			SV_LinkEdict (check, true);

			VectorCopy (pushorig, SVFIELD (pusher, angles, float));
			SV_LinkEdict (pusher, false);
			SVFIELD (pusher, ltime, float) -= movetime;

			// if the pusher has a "blocked" function, call it
			// otherwise, just stay in place until the obstacle is gone
			if (SVFIELD (pusher, blocked, float)) {
				*sv_globals.self =
					EDICT_TO_PROG (&sv_pr_state, pusher);
				*sv_globals.other =
					EDICT_TO_PROG (&sv_pr_state, check);
				PR_ExecuteProgram (&sv_pr_state, SVFIELD (pusher, blocked, float));
			}
			// move back any entities we already moved
			for (i = 0; i < num_moved; i++) {
				VectorCopy (moved_from[i], moved_edict[i]->v.v.origin);
				VectorSubtract (moved_edict[i]->v.v.angles, amove,
								moved_edict[i]->v.v.angles);
				SV_LinkEdict (moved_edict[i], false);
			}
			return;
		} else {
			VectorAdd (SVFIELD (check, angles, float), amove, SVFIELD (check, angles, float));
		}
	}


}
#endif

/*
================
SV_Physics_Pusher

================
*/
void
SV_Physics_Pusher (edict_t *ent)
{
	float       thinktime;
	float       oldltime;
	float       movetime;

	oldltime = SVFIELD (ent, ltime, float);

	thinktime = SVFIELD (ent, nextthink, float);
	if (thinktime < SVFIELD (ent, ltime, float) + host_frametime) {
		movetime = thinktime - SVFIELD (ent, ltime, float);
		if (movetime < 0)
			movetime = 0;
	} else
		movetime = host_frametime;

	if (movetime) {
#ifdef QUAKE2
		if (SVFIELD (ent, avelocity, float)[0] || SVFIELD (ent, avelocity, float)[1]
			|| SVFIELD (ent, avelocity, float)[2])
			SV_PushRotate (ent, movetime);
		else
#endif
			SV_PushMove (ent, movetime);	// advances SVFIELD (ent, ltime, float) if not 
											// 
		// blocked
	}

	if (thinktime > oldltime && thinktime <= SVFIELD (ent, ltime, float)) {
		SVFIELD (ent, nextthink, float) = 0;
		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
		*sv_globals.other =
			EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		PR_ExecuteProgram (&sv_pr_state, SVFIELD (ent, think, func));
		if (ent->free)
			return;
	}

}


/*
===============================================================================

CLIENT MOVEMENT

===============================================================================
*/

/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
void
SV_CheckStuck (edict_t *ent)
{
	int         i, j;
	int         z;
	vec3_t      org;

	if (!SV_TestEntityPosition (ent)) {
		VectorCopy (SVFIELD (ent, origin, vector), SVFIELD (ent, oldorigin, vector));
		return;
	}

	VectorCopy (SVFIELD (ent, origin, vector), org);
	VectorCopy (SVFIELD (ent, oldorigin, vector), SVFIELD (ent, origin, vector));
	if (!SV_TestEntityPosition (ent)) {
		Con_DPrintf ("Unstuck.\n");
		SV_LinkEdict (ent, true);
		return;
	}

	for (z = 0; z < 18; z++)
		for (i = -1; i <= 1; i++)
			for (j = -1; j <= 1; j++) {
				SVFIELD (ent, origin, vector)[0] = org[0] + i;
				SVFIELD (ent, origin, vector)[1] = org[1] + j;
				SVFIELD (ent, origin, vector)[2] = org[2] + z;
				if (!SV_TestEntityPosition (ent)) {
					Con_DPrintf ("Unstuck.\n");
					SV_LinkEdict (ent, true);
					return;
				}
			}

	VectorCopy (org, SVFIELD (ent, origin, vector));
	Con_DPrintf ("player is stuck.\n");
}


/*
=============
SV_CheckWater
=============
*/
qboolean
SV_CheckWater (edict_t *ent)
{
	vec3_t      point;
	int         cont;

#ifdef QUAKE2
	int         truecont;
#endif

	point[0] = SVFIELD (ent, origin, vector)[0];
	point[1] = SVFIELD (ent, origin, vector)[1];
	point[2] = SVFIELD (ent, origin, vector)[2] + SVFIELD (ent, mins, vector)[2] + 1;

	SVFIELD (ent, waterlevel, float) = 0;
	SVFIELD (ent, watertype, float) = CONTENTS_EMPTY;
	cont = SV_PointContents (point);
	if (cont <= CONTENTS_WATER) {
#ifdef QUAKE2
		truecont = SV_TruePointContents (point);
#endif
		SVFIELD (ent, watertype, float) = cont;
		SVFIELD (ent, waterlevel, float) = 1;
		point[2] =
			SVFIELD (ent, origin, vector)[2] + (SVFIELD (ent, mins, vector)[2] + SVFIELD (ent, maxs, vector)[2]) * 0.5;
		cont = SV_PointContents (point);
		if (cont <= CONTENTS_WATER) {
			SVFIELD (ent, waterlevel, float) = 2;
			point[2] = SVFIELD (ent, origin, vector)[2] + SVFIELD (ent, view_ofs, vector)[2];
			cont = SV_PointContents (point);
			if (cont <= CONTENTS_WATER)
				SVFIELD (ent, waterlevel, float) = 3;
		}
#ifdef QUAKE2
		if (truecont <= CONTENTS_CURRENT_0 && truecont >= CONTENTS_CURRENT_DOWN) {
			static vec3_t current_table[] = {
				{1, 0, 0},
				{0, 1, 0},
				{-1, 0, 0},
				{0, -1, 0},
				{0, 0, 1},
				{0, 0, -1}
			};

			VectorMA (SVFIELD (ent, basevelocity, float), 150.0 * SVFIELD (ent, waterlevel, float) / 3.0,
					  current_table[CONTENTS_CURRENT_0 - truecont],
					  SVFIELD (ent, basevelocity, float));
		}
#endif
	}

	return SVFIELD (ent, waterlevel, float) > 1;
}

/*
============
SV_WallFriction

============
*/
void
SV_WallFriction (edict_t *ent, trace_t *trace)
{
	vec3_t      forward, right, up;
	float       d, i;
	vec3_t      into, side;

	AngleVectors (SVFIELD (ent, v_angle, vector), forward, right, up);
	d = DotProduct (trace->plane.normal, forward);

	d += 0.5;
	if (d >= 0)
		return;

// cut the tangential velocity
	i = DotProduct (trace->plane.normal, SVFIELD (ent, velocity, vector));
	VectorScale (trace->plane.normal, i, into);
	VectorSubtract (SVFIELD (ent, velocity, vector), into, side);

	SVFIELD (ent, velocity, vector)[0] = side[0] * (1 + d);
	SVFIELD (ent, velocity, vector)[1] = side[1] * (1 + d);
}

/*
=====================
SV_TryUnstick

Player has come to a dead stop, possibly due to the problem with limited
float precision at some angle joins in the BSP hull.

Try fixing by pushing one pixel in each direction.

This is a hack, but in the interest of good gameplay...
======================
*/
int
SV_TryUnstick (edict_t *ent, vec3_t oldvel)
{
	int         i;
	vec3_t      oldorg;
	vec3_t      dir;
	int         clip;
	trace_t     steptrace;

	VectorCopy (SVFIELD (ent, origin, vector), oldorg);
	VectorCopy (vec3_origin, dir);

	for (i = 0; i < 8; i++) {
// try pushing a little in an axial direction
		switch (i) {
			case 0:
			dir[0] = 2;
			dir[1] = 0;
			break;
			case 1:
			dir[0] = 0;
			dir[1] = 2;
			break;
			case 2:
			dir[0] = -2;
			dir[1] = 0;
			break;
			case 3:
			dir[0] = 0;
			dir[1] = -2;
			break;
			case 4:
			dir[0] = 2;
			dir[1] = 2;
			break;
			case 5:
			dir[0] = -2;
			dir[1] = 2;
			break;
			case 6:
			dir[0] = 2;
			dir[1] = -2;
			break;
			case 7:
			dir[0] = -2;
			dir[1] = -2;
			break;
		}

		SV_PushEntity (ent, dir);

// retry the original move
		SVFIELD (ent, velocity, vector)[0] = oldvel[0];
		SVFIELD (ent, velocity, vector)[1] = oldvel[1];
		SVFIELD (ent, velocity, vector)[2] = 0;
		clip = SV_FlyMove (ent, 0.1, &steptrace);

		if (fabs (oldorg[1] - SVFIELD (ent, origin, vector)[1]) > 4
			|| fabs (oldorg[0] - SVFIELD (ent, origin, vector)[0]) > 4) {
//Con_DPrintf ("unstuck!\n");
			return clip;
		}
// go back to the original pos and try again
		VectorCopy (oldorg, SVFIELD (ent, origin, vector));
	}

	VectorCopy (vec3_origin, SVFIELD (ent, velocity, vector));
	return 7;							// still not moving
}

/*
=====================
SV_WalkMove

Only used by players
======================
*/
#define	STEPSIZE	18
void
SV_WalkMove (edict_t *ent)
{
	vec3_t      upmove, downmove;
	vec3_t      oldorg, oldvel;
	vec3_t      nosteporg, nostepvel;
	int         clip;
	int         oldonground;
	trace_t     steptrace, downtrace;

//
// do a regular slide move unless it looks like you ran into a step
//
	oldonground = (int) SVFIELD (ent, flags, float) & FL_ONGROUND;
	SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) & ~FL_ONGROUND;

	VectorCopy (SVFIELD (ent, origin, vector), oldorg);
	VectorCopy (SVFIELD (ent, velocity, vector), oldvel);

	clip = SV_FlyMove (ent, host_frametime, &steptrace);

	if (!(clip & 2))
		return;							// move didn't block on a step

	if (!oldonground && SVFIELD (ent, waterlevel, float) == 0)
		return;							// don't stair up while jumping

	if (SVFIELD (ent, movetype, float) != MOVETYPE_WALK)
		return;							// gibbed by a trigger

	if (sv_nostep->int_val)
		return;

	if ((int) SVFIELD (sv_player, flags, float) & FL_WATERJUMP)
		return;

	VectorCopy (SVFIELD (ent, origin, vector), nosteporg);
	VectorCopy (SVFIELD (ent, velocity, vector), nostepvel);

//
// try moving up and forward to go up a step
//
	VectorCopy (oldorg, SVFIELD (ent, origin, vector));	// back to start pos

	VectorCopy (vec3_origin, upmove);
	VectorCopy (vec3_origin, downmove);
	upmove[2] = STEPSIZE;
	downmove[2] = -STEPSIZE + oldvel[2] * host_frametime;

// move up
	SV_PushEntity (ent, upmove);		// FIXME: don't link?

// move forward
	SVFIELD (ent, velocity, vector)[0] = oldvel[0];
	SVFIELD (ent, velocity, vector)[1] = oldvel[1];
	SVFIELD (ent, velocity, vector)[2] = 0;
	clip = SV_FlyMove (ent, host_frametime, &steptrace);

// check for stuckness, possibly due to the limited precision of floats
// in the clipping hulls
	if (clip) {
		if (fabs (oldorg[1] - SVFIELD (ent, origin, vector)[1]) < 0.03125
			&& fabs (oldorg[0] - SVFIELD (ent, origin, vector)[0]) < 0.03125) {	// stepping 
																	// 
			// up
			// didn't 
			// make
			// any
			// progress
			clip = SV_TryUnstick (ent, oldvel);
		}
	}
// extra friction based on view angle
	if (clip & 2)
		SV_WallFriction (ent, &steptrace);

// move down
	downtrace = SV_PushEntity (ent, downmove);	// FIXME: don't link?

	if (downtrace.plane.normal[2] > 0.7) {
		if (SVFIELD (ent, solid, float) == SOLID_BSP) {
			SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) | FL_ONGROUND;
			SVFIELD (ent, groundentity, float) = EDICT_TO_PROG (&sv_pr_state, downtrace.ent);
		}
	} else {
// if the push down didn't end up on good ground, use the move without
// the step up.  This happens near wall / slope combinations, and can
// cause the player to hop up higher on a slope too steep to climb  
		VectorCopy (nosteporg, SVFIELD (ent, origin, vector));
		VectorCopy (nostepvel, SVFIELD (ent, velocity, vector));
	}
}


/*
================
SV_Physics_Client

Player character actions
================
*/
void
SV_Physics_Client (edict_t *ent, int num)
{
	if (!svs.clients[num - 1].active)
		return;							// unconnected slot

//
// call standard client pre-think
//  
	*sv_globals.time = sv.time;
	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
	PR_ExecuteProgram (&sv_pr_state,
					   sv_funcs.PlayerPreThink);

//
// do a move
//
	SV_CheckVelocity (ent);

//
// decide which move function to call
//
	switch ((int) SVFIELD (ent, movetype, float)) {
		case MOVETYPE_NONE:
		if (!SV_RunThink (ent))
			return;
		break;

		case MOVETYPE_WALK:
		if (!SV_RunThink (ent))
			return;
		if (!SV_CheckWater (ent) && !((int) SVFIELD (ent, flags, float) & FL_WATERJUMP))
			SV_AddGravity (ent);
		SV_CheckStuck (ent);
#ifdef QUAKE2
		VectorAdd (SVFIELD (ent, velocity, float), SVFIELD (ent, basevelocity, float), SVFIELD (ent, velocity, float));
#endif
		SV_WalkMove (ent);

#ifdef QUAKE2
		VectorSubtract (SVFIELD (ent, velocity, float), SVFIELD (ent, basevelocity, float),
						SVFIELD (ent, velocity, float));
#endif
		break;

		case MOVETYPE_TOSS:
		case MOVETYPE_BOUNCE:
		SV_Physics_Toss (ent);
		break;

		case MOVETYPE_FLY:
		if (!SV_RunThink (ent))
			return;
		SV_FlyMove (ent, host_frametime, NULL);
		break;

		case MOVETYPE_NOCLIP:
		if (!SV_RunThink (ent))
			return;
		VectorMA (SVFIELD (ent, origin, vector), host_frametime, SVFIELD (ent, velocity, vector),
				  SVFIELD (ent, origin, vector));
		break;

		default:
		Sys_Error ("SV_Physics_client: bad movetype %i",
				   (int) SVFIELD (ent, movetype, float));
	}

//
// call standard player post-think
//      
	SV_LinkEdict (ent, true);

	*sv_globals.time = sv.time;
	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
	PR_ExecuteProgram (&sv_pr_state,
					   sv_funcs.PlayerPostThink);
}

//============================================================================

/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
void
SV_Physics_None (edict_t *ent)
{
// regular thinking
	SV_RunThink (ent);
}

#ifdef QUAKE2
/*
=============
SV_Physics_Follow

Entities that are "stuck" to another entity
=============
*/
void
SV_Physics_Follow (edict_t *ent)
{
// regular thinking
	SV_RunThink (ent);
	VectorAdd (PROG_TO_EDICT (&sv_pr_state, SVFIELD (ent, aiment, float))->v.v.origin,
			   SVFIELD (ent, v_angle, float), SVFIELD (ent, origin, float));
	SV_LinkEdict (ent, true);
}

#endif

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
void
SV_Physics_Noclip (edict_t *ent)
{
// regular thinking
	if (!SV_RunThink (ent))
		return;

	VectorMA (SVFIELD (ent, angles, vector), host_frametime, SVFIELD (ent, avelocity, vector),
			  SVFIELD (ent, angles, vector));
	VectorMA (SVFIELD (ent, origin, vector), host_frametime, SVFIELD (ent, velocity, vector),
			  SVFIELD (ent, origin, vector));

	SV_LinkEdict (ent, false);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_CheckWaterTransition

=============
*/
void
SV_CheckWaterTransition (edict_t *ent)
{
	int         cont;

#ifdef QUAKE2
	vec3_t      point;

	point[0] = SVFIELD (ent, origin, vector)[0];
	point[1] = SVFIELD (ent, origin, vector)[1];
	point[2] = SVFIELD (ent, origin, vector)[2] + SVFIELD (ent, mins, vector)[2] + 1;
	cont = SV_PointContents (point);
#else
	cont = SV_PointContents (SVFIELD (ent, origin, vector));
#endif
	if (!SVFIELD (ent, watertype, float)) {			// just spawned here
		SVFIELD (ent, watertype, float) = cont;
		SVFIELD (ent, waterlevel, float) = 1;
		return;
	}

	if (cont <= CONTENTS_WATER) {
		if (SVFIELD (ent, watertype, float) == CONTENTS_EMPTY) {	// just crossed into
			// water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}
		SVFIELD (ent, watertype, float) = cont;
		SVFIELD (ent, waterlevel, float) = 1;
	} else {
		if (SVFIELD (ent, watertype, float) != CONTENTS_EMPTY) {	// just crossed into
			// water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}
		SVFIELD (ent, watertype, float) = CONTENTS_EMPTY;
		SVFIELD (ent, waterlevel, float) = cont;
	}
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void
SV_Physics_Toss (edict_t *ent)
{
	trace_t     trace;
	vec3_t      move;
	float       backoff;

#ifdef QUAKE2
	edict_t    *groundentity;

	groundentity = PROG_TO_EDICT (&sv_pr_state, SVFIELD (ent, groundentity, entity));
	if ((int) SVFIELD (groundentity, flags, float) & FL_CONVEYOR)
		VectorScale (SVFIELD (groundentity, movedir, float), SVFIELD (groundentity, speed, float),
					 SVFIELD (ent, basevelocity, float));
	else
		VectorCopy (vec_origin, SVFIELD (ent, basevelocity, float));
	SV_CheckWater (ent);
#endif
	// regular thinking
	if (!SV_RunThink (ent))
		return;

#ifdef QUAKE2
	if (SVFIELD (ent, velocity, float)[2] > 0)
		SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) & ~FL_ONGROUND;

	if (((int) SVFIELD (ent, flags, float) & FL_ONGROUND))
//@@
		if (VectorCompare (SVFIELD (ent, basevelocity, float), vec_origin))
			return;

	SV_CheckVelocity (ent);

// add gravity
	if (!((int) SVFIELD (ent, flags, float) & FL_ONGROUND)
		&& SVFIELD (ent, movetype, float) != MOVETYPE_FLY
		&& SVFIELD (ent, movetype, float) != MOVETYPE_BOUNCEMISSILE
		&& SVFIELD (ent, movetype, float) != MOVETYPE_FLYMISSILE) SV_AddGravity (ent);

#else
// if onground, return without moving
	if (((int) SVFIELD (ent, flags, float) & FL_ONGROUND))
		return;

	SV_CheckVelocity (ent);

// add gravity
	if (SVFIELD (ent, movetype, float) != MOVETYPE_FLY
		&& SVFIELD (ent, movetype, float) != MOVETYPE_FLYMISSILE) SV_AddGravity (ent);
#endif

// move angles
	VectorMA (SVFIELD (ent, angles, vector), host_frametime, SVFIELD (ent, avelocity, vector),
			  SVFIELD (ent, angles, vector));

// move origin
#ifdef QUAKE2
	VectorAdd (SVFIELD (ent, velocity, vector), SVFIELD (ent, basevelocity, vector), SVFIELD (ent, velocity, vector));
#endif
	VectorScale (SVFIELD (ent, velocity, vector), host_frametime, move);
	trace = SV_PushEntity (ent, move);
#ifdef QUAKE2
	VectorSubtract (SVFIELD (ent, velocity, vector), SVFIELD (ent, basevelocity, vector),
					SVFIELD (ent, velocity, vector));
#endif
	if (trace.fraction == 1)
		return;
	if (ent->free)
		return;

	if (SVFIELD (ent, movetype, float) == MOVETYPE_BOUNCE)
		backoff = 1.5;
#ifdef QUAKE2
	else if (SVFIELD (ent, movetype, float) == MOVETYPE_BOUNCEMISSILE)
		backoff = 2.0;
#endif
	else
		backoff = 1;

	ClipVelocity (SVFIELD (ent, velocity, vector), trace.plane.normal, SVFIELD (ent, velocity, vector),
				  backoff);

// stop if on ground
	if (trace.plane.normal[2] > 0.7) {
#ifdef QUAKE2
		if (SVFIELD (ent, velocity, vector)[2] < 60
			|| (SVFIELD (ent, movetype, float) != MOVETYPE_BOUNCE
				&& SVFIELD (ent, movetype, float) != MOVETYPE_BOUNCEMISSILE))
#else
		if (SVFIELD (ent, velocity, vector)[2] < 60 || SVFIELD (ent, movetype, float) != MOVETYPE_BOUNCE)
#endif
		{
			SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) | FL_ONGROUND;
			SVFIELD (ent, groundentity, entity) = EDICT_TO_PROG (&sv_pr_state, trace.ent);
			VectorCopy (vec3_origin, SVFIELD (ent, velocity, vector));
			VectorCopy (vec3_origin, SVFIELD (ent, avelocity, vector));
		}
	}
// check for in water
	SV_CheckWaterTransition (ent);
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/
#ifdef QUAKE2
void
SV_Physics_Step (edict_t *ent)
{
	qboolean    wasonground;
	qboolean    inwater;
	qboolean    hitsound = false;
	float      *vel;
	float       speed, newspeed, control;
	float       friction;
	edict_t    *groundentity;

	groundentity = PROG_TO_EDICT (&sv_pr_state, SVFIELD (ent, groundentity, float));
	if ((int) SVFIELD (groundentity, flags, float) & FL_CONVEYOR)
		VectorScale (SVFIELD (groundentity, movedir, float), SVFIELD (groundentity, speed, float),
					 SVFIELD (ent, basevelocity, float));
	else
		VectorCopy (vec_origin, SVFIELD (ent, basevelocity, float));
//@@
	*sv_globals.time = sv.time;
	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
	PF_WaterMove (&sv_pr_state);

	SV_CheckVelocity (ent);

	wasonground = (int) SVFIELD (ent, flags, float) & FL_ONGROUND;
//  SVFIELD (ent, flags, float) = (int)SVFIELD (ent, flags, float) & ~FL_ONGROUND;

	// add gravity except:
	// flying monsters
	// swimming monsters who are in the water
	inwater = SV_CheckWater (ent);
	if (!wasonground)
		if (!((int) SVFIELD (ent, flags, float) & FL_FLY))
			if (!(((int) SVFIELD (ent, flags, float) & FL_SWIM)
				  && (SVFIELD (ent, waterlevel, float) > 0))) {
				if (SVFIELD (ent, velocity, float)[2] < sv_gravity->value * -0.1)
					hitsound = true;
				if (!inwater)
					SV_AddGravity (ent);
			}

	if (!VectorCompare (SVFIELD (ent, velocity, float), vec_origin)
		|| !VectorCompare (SVFIELD (ent, basevelocity, float), vec_origin)) {
		SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) & ~FL_ONGROUND;
		// apply friction
		// let dead monsters who aren't completely onground slide
		if (wasonground)
			if (!(SVFIELD (ent, health, float) <= 0.0 && !SV_CheckBottom (ent))) {
				vel = SVFIELD (ent, velocity, float);
				speed = sqrt (vel[0] * vel[0] + vel[1] * vel[1]);
				if (speed) {
					friction = sv_friction->value;

					control =
						speed <
						sv_stopspeed->value ? sv_stopspeed->value : speed;
					newspeed = speed - host_frametime * control * friction;

					if (newspeed < 0)
						newspeed = 0;
					newspeed /= speed;

					vel[0] = vel[0] * newspeed;
					vel[1] = vel[1] * newspeed;
				}
			}

		VectorAdd (SVFIELD (ent, velocity, float), SVFIELD (ent, basevelocity, float), SVFIELD (ent, velocity, float));
		SV_FlyMove (ent, host_frametime, NULL);
		VectorSubtract (SVFIELD (ent, velocity, float), SVFIELD (ent, basevelocity, float),
						SVFIELD (ent, velocity, float));

		// determine if it's on solid ground at all
		{
			vec3_t      mins, maxs, point;
			int         x, y;

			VectorAdd (SVFIELD (ent, origin, float), SVFIELD (ent, mins, float), mins);
			VectorAdd (SVFIELD (ent, origin, float), SVFIELD (ent, maxs, float), maxs);

			point[2] = mins[2] - 1;
			for (x = 0; x <= 1; x++)
				for (y = 0; y <= 1; y++) {
					point[0] = x ? maxs[0] : mins[0];
					point[1] = y ? maxs[1] : mins[1];
					if (SV_PointContents (point) == CONTENTS_SOLID) {
						SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) | FL_ONGROUND;
						break;
					}
				}

		}

		SV_LinkEdict (ent, true);

		if ((int) SVFIELD (ent, flags, float) & FL_ONGROUND)
			if (!wasonground)
				if (hitsound)
					SV_StartSound (ent, 0, "demon/dland2.wav", 255, 1);
	}
// regular thinking
	SV_RunThink (ent);
	SV_CheckWaterTransition (ent);
}
#else
void
SV_Physics_Step (edict_t *ent)
{
	qboolean    hitsound;

// freefall if not onground
	if (!((int) SVFIELD (ent, flags, float) & (FL_ONGROUND | FL_FLY | FL_SWIM))) {
		if (SVFIELD (ent, velocity, vector)[2] < sv_gravity->value * -0.1)
			hitsound = true;
		else
			hitsound = false;

		SV_AddGravity (ent);
		SV_CheckVelocity (ent);
		SV_FlyMove (ent, host_frametime, NULL);
		SV_LinkEdict (ent, true);

		if ((int) SVFIELD (ent, flags, float) & FL_ONGROUND)	// just hit ground
		{
			if (hitsound)
				SV_StartSound (ent, 0, "demon/dland2.wav", 255, 1);
		}
	}
// regular thinking
	SV_RunThink (ent);

	SV_CheckWaterTransition (ent);
}
#endif

//============================================================================

/*
================
SV_Physics

================
*/
void
SV_Physics (void)
{
	int         i;
	edict_t    *ent;

// let the progs know that a new frame has started
	*sv_globals.self =
		EDICT_TO_PROG (&sv_pr_state, sv.edicts);
	*sv_globals.other =
		EDICT_TO_PROG (&sv_pr_state, sv.edicts);
	*sv_globals.time = sv.time;
	PR_ExecuteProgram (&sv_pr_state, sv_funcs.StartFrame);

//SV_CheckAllEnts ();

//
// treat each object in turn
//
	ent = sv.edicts;
	for (i = 0; i < sv.num_edicts; i++, ent = NEXT_EDICT (&sv_pr_state, ent)) {
		if (ent->free)
			continue;

		if (*sv_globals.force_retouch) {
			SV_LinkEdict (ent, true);	// force retouch even for stationary
		}

		if (i > 0 && i <= svs.maxclients)
			SV_Physics_Client (ent, i);
		else if (SVFIELD (ent, movetype, float) == MOVETYPE_PUSH)
			SV_Physics_Pusher (ent);
		else if (SVFIELD (ent, movetype, float) == MOVETYPE_NONE)
			SV_Physics_None (ent);
#ifdef QUAKE2
		else if (SVFIELD (ent, movetype, float) == MOVETYPE_FOLLOW)
			SV_Physics_Follow (ent);
#endif
		else if (SVFIELD (ent, movetype, float) == MOVETYPE_NOCLIP)
			SV_Physics_Noclip (ent);
		else if (SVFIELD (ent, movetype, float) == MOVETYPE_STEP)
			SV_Physics_Step (ent);
		else if (SVFIELD (ent, movetype, float) == MOVETYPE_TOSS
				 || SVFIELD (ent, movetype, float) == MOVETYPE_BOUNCE
#ifdef QUAKE2
				 || SVFIELD (ent, movetype, float) == MOVETYPE_BOUNCEMISSILE
#endif
				 || SVFIELD (ent, movetype, float) == MOVETYPE_FLY
				 || SVFIELD (ent, movetype, float) == MOVETYPE_FLYMISSILE)
				SV_Physics_Toss (ent);
		else
			Sys_Error ("SV_Physics: bad movetype %i", (int) SVFIELD (ent, movetype, float));
	}

	if (*sv_globals.force_retouch)
		(*sv_globals.force_retouch)--;

	sv.time += host_frametime;
}


#ifdef QUAKE2
trace_t
SV_Trace_Toss (edict_t *ent, edict_t *ignore)
{
	edict_t     tempent, *tent;
	trace_t     trace;
	vec3_t      move;
	vec3_t      end;
	double      save_frametime;

//  extern particle_t   *active_particles, *free_particles;
//  particle_t  *p;


	save_frametime = host_frametime;
	host_frametime = 0.05;

	memcpy (&tempent, ent, sizeof (edict_t));

	tent = &tempent;

	while (1) {
		SV_CheckVelocity (tent);
		SV_AddGravity (tent);
		VectorMA (SVFIELD (tent, angles, float), host_frametime, SVFIELD (tent, avelocity, float),
				  SVFIELD (tent, angles, float));
		VectorScale (SVFIELD (tent, velocity, float), host_frametime, move);
		VectorAdd (SVFIELD (tent, origin, float), move, end);
		trace =
			SV_Move (SVFIELD (tent, origin, float), SVFIELD (tent, mins, float), SVFIELD (tent, maxs, float), end,
					 MOVE_NORMAL, tent);
		VectorCopy (trace.endpos, SVFIELD (tent, origin, float));

//      p = free_particles;
//      if (p)
//      {
//          free_particles = p->next;
//          p->next = active_particles;
//          active_particles = p;
//      
//          p->die = 256;
//          p->color = 15;
//          p->type = pt_static;
//          VectorCopy (vec3_origin, p->vel);
//          VectorCopy (SVFIELD (tent, origin, float), p->org);
//      }

		if (trace.ent)
			if (trace.ent != ignore)
				break;
	}
//  p->color = 224;
	host_frametime = save_frametime;
	return trace;
}
#endif
