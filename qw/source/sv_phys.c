/*
	sv_phys.c

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cvar.h"
#include "pmove.h"
#include "server.h"
#include "sv_progs.h"
#include "world.h"

/*

	pushmove objects do not obey gravity, and do not interact with each
	other or trigger fields, but block normal movement and push normal
	objects when they move.

	onground is set for toss objects when they come to a complete rest.  it
	is set for steping or walking objects

	doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
	bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
	corpses are SOLID_NOT and MOVETYPE_TOSS
	crates are SOLID_BBOX and MOVETYPE_TOSS
	walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
	flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

	solid_edge items only clip against bsp models.

*/

cvar_t     *sv_maxvelocity;

cvar_t     *sv_gravity;
cvar_t     *sv_stopspeed;
cvar_t     *sv_maxspeed;
cvar_t     *sv_spectatormaxspeed;
cvar_t     *sv_accelerate;
cvar_t     *sv_airaccelerate;
cvar_t     *sv_wateraccelerate;
cvar_t     *sv_friction;
cvar_t     *sv_waterfriction;


#define	MOVE_EPSILON	0.01

void        SV_Physics_Toss (edict_t *ent);

/*
	SV_CheckAllEnts
*/
void
SV_CheckAllEnts (void)
{
	int         e;
	edict_t    *check;

// see if any solid entities are inside the final position
	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)
			continue;
		if (SVFIELD (check, movetype, float) == MOVETYPE_PUSH
			|| SVFIELD (check, movetype, float) == MOVETYPE_NONE
			|| SVFIELD (check, movetype, float) == MOVETYPE_NOCLIP) continue;

		if (SV_TestEntityPosition (check))
			Con_Printf ("entity in invalid position\n");
	}
}

/*
	SV_CheckVelocity
*/
void
SV_CheckVelocity (edict_t *ent)
{
	int         i;
	float       wishspeed;				// 1999-10-18 SV_MAXVELOCITY fix by Maddes

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
	}

// 1999-10-18 SV_MAXVELOCITY fix by Maddes  start
	wishspeed = Length (SVFIELD (ent, velocity, vector));
	if (wishspeed > sv_maxvelocity->value) {
		VectorScale (SVFIELD (ent, velocity, vector), sv_maxvelocity->value / wishspeed,
					 SVFIELD (ent, velocity, vector));
	}
// 1999-10-18 SV_MAXVELOCITY fix by Maddes  end
}

/*
	SV_RunThink

	Runs thinking code if time.  There is some play in the exact time the think
	function will be called, because it is called before any movement is done
	in a frame.  Not used for pushmove objects, because they must be exact.
	Returns false if the entity removed itself.
 */
qboolean
SV_RunThink (edict_t *ent)
{
	float       thinktime;

	do {
		thinktime = SVFIELD (ent, nextthink, float);
		if (thinktime <= 0)
			return true;
		if (thinktime > sv.time + sv_frametime)
			return true;

		if (thinktime < sv.time)
			thinktime = sv.time;		// don't let things stay in the past.
		// it is possible to start that way
		// by a trigger with a local time.
		SVFIELD (ent, nextthink, float) = 0;
		*sv_globals.time = thinktime;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		PR_ExecuteProgram (&sv_pr_state, SVFIELD (ent, think, func));

		if (ent->free)
			return false;
	} while (1);

	return true;
}

/*
	SV_Impact

	Two entities have touched, so run their touch functions
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
	ClipVelocity

	Slide off of the impacting object
	returns the blocked flags (1 = floor, 2 = step / wall)
 */
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
	SV_FlyMove

	The basic solid body movement clip that slides along multiple planes
	Returns the clipflags if the velocity was modified (hit something solid)
	1 = floor
	2 = wall / step
	4 = dead stop
	If steptrace is not NULL, the trace of any vertical wall hit will be stored
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
		for (i = 0; i < 3; i++)
			end[i] = SVFIELD (ent, origin, vector)[i] + time_left * SVFIELD (ent, velocity, vector)[i];

		trace =
			SV_Move (SVFIELD (ent, origin, vector), SVFIELD (ent, mins, vector), SVFIELD (ent, maxs, vector), end, false, ent);

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
			SV_Error ("SV_FlyMove: !trace.ent");

		if (trace.plane.normal[2] > 0.7) {
			blocked |= 1;				// floor
			if ((SVFIELD (trace.ent, solid, float) == SOLID_BSP)
				|| (SVFIELD (trace.ent, movetype, float) == MOVETYPE_PPUSH)) {
				SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) | FL_ONGROUND;
				SVFIELD (ent, groundentity, int) = EDICT_TO_PROG (&sv_pr_state, trace.ent);
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
	SV_AddGravity
*/
void
SV_AddGravity (edict_t *ent, float scale)
{
	SVFIELD (ent, velocity, vector)[2] -= scale * movevars.gravity * sv_frametime;
}

/*
	PUSHMOVE
*/

/*
	SV_PushEntity

	Does not change the entities velocity at all
*/
trace_t
SV_PushEntity (edict_t *ent, vec3_t push)
{
	trace_t     trace;
	vec3_t      end;

	VectorAdd (SVFIELD (ent, origin, vector), push, end);

	if (SVFIELD (ent, movetype, float) == MOVETYPE_FLYMISSILE)
		trace =
			SV_Move (SVFIELD (ent, origin, vector), SVFIELD (ent, mins, vector), SVFIELD (ent, maxs, vector), end, MOVE_MISSILE,
					 ent);
	else if (SVFIELD (ent, solid, float) == SOLID_TRIGGER || SVFIELD (ent, solid, float) == SOLID_NOT)
		// only clip against bmodels
		trace =
			SV_Move (SVFIELD (ent, origin, vector), SVFIELD (ent, mins, vector), SVFIELD (ent, maxs, vector), end,
					 MOVE_NOMONSTERS, ent);
	else
		trace =
			SV_Move (SVFIELD (ent, origin, vector), SVFIELD (ent, mins, vector), SVFIELD (ent, maxs, vector), end, MOVE_NORMAL,
					 ent);

	VectorCopy (trace.endpos, SVFIELD (ent, origin, vector));
	SV_LinkEdict (ent, true);

	if (trace.ent)
		SV_Impact (ent, trace.ent);

	return trace;
}


/*
	SV_Push
*/
qboolean
SV_Push (edict_t *pusher, vec3_t move)
{
	int         i, e;
	edict_t    *check, *block;
	vec3_t      mins, maxs;
	vec3_t      pushorig;
	int         num_moved;
	edict_t    *moved_edict[MAX_EDICTS];
	vec3_t      moved_from[MAX_EDICTS];
	float       solid_save;				// for Lord Havoc's SOLID_BSP fix

										// --KB

	for (i = 0; i < 3; i++) {
		mins[i] = SVFIELD (pusher, absmin, vector)[i] + move[i];
		maxs[i] = SVFIELD (pusher, absmax, vector)[i] + move[i];
	}

	VectorCopy (SVFIELD (pusher, origin, vector), pushorig);

// move the pusher to it's final position

	VectorAdd (SVFIELD (pusher, origin, vector), move, SVFIELD (pusher, origin, vector));
	SV_LinkEdict (pusher, false);

// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)
			continue;
		if (SVFIELD (check, movetype, float) == MOVETYPE_PUSH
			|| SVFIELD (check, movetype, float) == MOVETYPE_NONE
			|| SVFIELD (check, movetype, float) == MOVETYPE_PPUSH
			|| SVFIELD (check, movetype, float) == MOVETYPE_NOCLIP) continue;

		// Don't assume SOLID_BSP !  --KB
		solid_save = SVFIELD (pusher, solid, float);
		SVFIELD (pusher, solid, float) = SOLID_NOT;
		block = SV_TestEntityPosition (check);
		// SVFIELD (pusher, solid, float) = SOLID_BSP;
		SVFIELD (pusher, solid, float) = solid_save;
		if (block)
			continue;

		// if the entity is standing on the pusher, it will definately be
		// moved
		if (!(((int) SVFIELD (check, flags, float) & FL_ONGROUND)
			  && PROG_TO_EDICT (&sv_pr_state, SVFIELD (check, groundentity, int)) == pusher)) {
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

		VectorCopy (SVFIELD (check, origin, vector), moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		// try moving the contacted entity
		VectorAdd (SVFIELD (check, origin, vector), move, SVFIELD (check, origin, vector));
		block = SV_TestEntityPosition (check);
		if (!block) {					// pushed ok
			SV_LinkEdict (check, false);
			continue;
		}
		// if it is ok to leave in the old position, do it
		VectorSubtract (SVFIELD (check, origin, vector), move, SVFIELD (check, origin, vector));
		block = SV_TestEntityPosition (check);
		if (!block) {
			num_moved--;
			continue;
		}
		// if it is still inside the pusher, block
		if (SVFIELD (check, mins, vector)[0] == SVFIELD (check, maxs, vector)[0]) {
			SV_LinkEdict (check, false);
			continue;
		}
		if (SVFIELD (check, solid, float) == SOLID_NOT || SVFIELD (check, solid, float) == SOLID_TRIGGER) {	// corpse
			SVFIELD (check, mins, vector)[0] = SVFIELD (check, mins, vector)[1] = 0;
			VectorCopy (SVFIELD (check, mins, vector), SVFIELD (check, maxs, vector));
			SV_LinkEdict (check, false);
			continue;
		}

		VectorCopy (pushorig, SVFIELD (pusher, origin, vector));
		SV_LinkEdict (pusher, false);

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if (SVFIELD (pusher, blocked, func)) {
			*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, pusher);
			*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, check);
			PR_ExecuteProgram (&sv_pr_state, SVFIELD (pusher, blocked, func));
		}
		// move back any entities we already moved
		for (i = 0; i < num_moved; i++) {
			VectorCopy (moved_from[i], SVFIELD (moved_edict[i], origin, vector));
			SV_LinkEdict (moved_edict[i], false);
		}
		return false;
	}

	return true;
}

/*
	SV_PushMove
*/
void
SV_PushMove (edict_t *pusher, float movetime)
{
	int         i;
	vec3_t      move;

	if (!SVFIELD (pusher, velocity, vector)[0] && !SVFIELD (pusher, velocity, vector)[1]
		&& !SVFIELD (pusher, velocity, vector)[2]) {
		SVFIELD (pusher, ltime, float) += movetime;
		return;
	}

	for (i = 0; i < 3; i++)
		move[i] = SVFIELD (pusher, velocity, vector)[i] * movetime;

	if (SV_Push (pusher, move))
		SVFIELD (pusher, ltime, float) += movetime;
}


/*
	SV_Physics_Pusher
*/
void
SV_Physics_Pusher (edict_t *ent)
{
	float       thinktime;
	float       oldltime;
	float       movetime;
	vec3_t      oldorg, move;
	float       l;

	oldltime = SVFIELD (ent, ltime, float);

	thinktime = SVFIELD (ent, nextthink, float);
	if (thinktime < SVFIELD (ent, ltime, float) + sv_frametime) {
		movetime = thinktime - SVFIELD (ent, ltime, float);
		if (movetime < 0)
			movetime = 0;
	} else
		movetime = sv_frametime;

	if (movetime) {
		SV_PushMove (ent, movetime);	// advances SVFIELD (ent, ltime, float) if not
										// blocked
	}

	if (thinktime > oldltime && thinktime <= SVFIELD (ent, ltime, float)) {
		VectorCopy (SVFIELD (ent, origin, vector), oldorg);
		SVFIELD (ent, nextthink, float) = 0;
		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		PR_ExecuteProgram (&sv_pr_state, SVFIELD (ent, think, func));
		if (ent->free)
			return;
		VectorSubtract (SVFIELD (ent, origin, vector), oldorg, move);

		l = Length (move);
		if (l > 1.0 / 64) {
//          Con_Printf ("**** snap: %f\n", Length (l));
			VectorCopy (oldorg, SVFIELD (ent, origin, vector));
			SV_Push (ent, move);
		}
	}
}


/*
	SV_Physics_None

	Non moving objects can only think
*/
void
SV_Physics_None (edict_t *ent)
{
// regular thinking
	SV_RunThink (ent);
	SV_LinkEdict (ent, false);
}

/*
	SV_Physics_Noclip

	A moving object that doesn't obey physics
*/
void
SV_Physics_Noclip (edict_t *ent)
{
// regular thinking
	if (!SV_RunThink (ent))
		return;

	VectorMA (SVFIELD (ent, angles, vector), sv_frametime, SVFIELD (ent, avelocity, vector), SVFIELD (ent, angles, vector));
	VectorMA (SVFIELD (ent, origin, vector), sv_frametime, SVFIELD (ent, velocity, vector), SVFIELD (ent, origin, vector));

	SV_LinkEdict (ent, false);
}

/*
	TOSS / BOUNCE
*/

/*
	SV_CheckWaterTransition
*/
void
SV_CheckWaterTransition (edict_t *ent)
{
	int         cont;

	cont = SV_PointContents (SVFIELD (ent, origin, vector));
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
	SV_Physics_Toss

	Toss, bounce, and fly movement.  When onground, do nothing.
*/
void
SV_Physics_Toss (edict_t *ent)
{
	trace_t     trace;
	vec3_t      move;
	float       backoff;

// regular thinking
	if (!SV_RunThink (ent))
		return;

	if (SVFIELD (ent, velocity, vector)[2] > 0)
		SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) & ~FL_ONGROUND;

// if onground, return without moving
	if (((int) SVFIELD (ent, flags, float) & FL_ONGROUND))
		return;

	SV_CheckVelocity (ent);

// add gravity
	if (SVFIELD (ent, movetype, float) != MOVETYPE_FLY
		&& SVFIELD (ent, movetype, float) != MOVETYPE_FLYMISSILE) SV_AddGravity (ent, 1.0);

// move angles
	VectorMA (SVFIELD (ent, angles, vector), sv_frametime, SVFIELD (ent, avelocity, vector), SVFIELD (ent, angles, vector));

// move origin
	VectorScale (SVFIELD (ent, velocity, vector), sv_frametime, move);
	trace = SV_PushEntity (ent, move);
	if (trace.fraction == 1)
		return;
	if (ent->free)
		return;

	if (SVFIELD (ent, movetype, float) == MOVETYPE_BOUNCE)
		backoff = 1.5;
	else
		backoff = 1;

	ClipVelocity (SVFIELD (ent, velocity, vector), trace.plane.normal, SVFIELD (ent, velocity, vector),
				  backoff);

// stop if on ground
	if (trace.plane.normal[2] > 0.7) {
		if (SVFIELD (ent, velocity, vector)[2] < 60 || SVFIELD (ent, movetype, float) != MOVETYPE_BOUNCE) {
			SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) | FL_ONGROUND;
			SVFIELD (ent, groundentity, int) = EDICT_TO_PROG (&sv_pr_state, trace.ent);
			VectorCopy (vec3_origin, SVFIELD (ent, velocity, vector));
			VectorCopy (vec3_origin, SVFIELD (ent, avelocity, vector));
		}
	}
// check for in water
	SV_CheckWaterTransition (ent);
}

/*
	STEPPING MOVEMENT
*/

/*
	SV_Physics_Step

	Monsters freefall when they don't have a ground entity, otherwise
	all movement is done with discrete steps.

	This is also used for objects that have become still on the ground, but
	will fall if the floor is pulled out from under them.
	FIXME: is this true?
*/
void
SV_Physics_Step (edict_t *ent)
{
	qboolean    hitsound;

// freefall if not on ground
	if (!((int) SVFIELD (ent, flags, float) & (FL_ONGROUND | FL_FLY | FL_SWIM))) {
		if (SVFIELD (ent, velocity, vector)[2] < movevars.gravity * -0.1)
			hitsound = true;
		else
			hitsound = false;

		SV_AddGravity (ent, 1.0);
		SV_CheckVelocity (ent);
		SV_FlyMove (ent, sv_frametime, NULL);
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

void
SV_PPushMove (edict_t *pusher, float movetime)	// player push
{
	int         i, e;
	edict_t    *check;
	vec3_t      mins, maxs, move;
	int         oldsolid;
	trace_t     trace;

	SV_CheckVelocity (pusher);
	for (i = 0; i < 3; i++) {
		move[i] = SVFIELD (pusher, velocity, vector)[i] * movetime;
		mins[i] = SVFIELD (pusher, absmin, vector)[i] + move[i];
		maxs[i] = SVFIELD (pusher, absmax, vector)[i] + move[i];
	}

	VectorCopy (SVFIELD (pusher, origin, vector), SVFIELD (pusher, oldorigin, vector));	// Backup origin
	trace =
		SV_Move (SVFIELD (pusher, origin, vector), SVFIELD (pusher, mins, vector), SVFIELD (pusher, maxs, vector), move,
				 MOVE_NOMONSTERS, pusher);

	if (trace.fraction == 1) {
		VectorCopy (SVFIELD (pusher, origin, vector), SVFIELD (pusher, oldorigin, vector));	// Revert
		return;
	}


	VectorAdd (SVFIELD (pusher, origin, vector), move, SVFIELD (pusher, origin, vector));	// Move
	SV_LinkEdict (pusher, false);
	SVFIELD (pusher, ltime, float) += movetime;

	oldsolid = SVFIELD (pusher, solid, float);

	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)				// What entity?
			continue;

		// Stage 1: Is it in contact with me?
		if (!SV_TestEntityPosition (check))	// Nope
			continue;

		// Stage 2: Is it a player we can push?
		if (SVFIELD (check, movetype, float) == MOVETYPE_WALK) {
			Con_Printf ("Pusher encountered a player\n");	// Yes!@#!@
			SVFIELD (pusher, solid, float) = SOLID_NOT;
			SV_PushEntity (check, move);
			SVFIELD (pusher, solid, float) = oldsolid;
			continue;
		}
		// Stage 3: No.. Is it something that blocks us?
		if (SVFIELD (check, mins, vector)[0] == SVFIELD (check, maxs, vector)[0])
			continue;
		if (SVFIELD (check, solid, float) == SOLID_NOT || SVFIELD (check, solid, float) == SOLID_TRIGGER)
			continue;

		// Stage 4: Yes, it must be. Fail the move.
		VectorCopy (SVFIELD (pusher, origin, vector), SVFIELD (pusher, oldorigin, vector));	// Revert
		if (SVFIELD (pusher, blocked, func)) {		// Blocked func?
			*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, pusher);
			*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, check);
			PR_ExecuteProgram (&sv_pr_state, SVFIELD (pusher, blocked, func));
		}

		return;
	}
}

void
SV_Physics_PPusher (edict_t *ent)
{
	float       thinktime;
	float       oldltime;
	float       movetime;

//  float   l;

	oldltime = SVFIELD (ent, ltime, float);

	thinktime = SVFIELD (ent, nextthink, float);
	if (thinktime < SVFIELD (ent, ltime, float) + sv_frametime) {
		movetime = thinktime - SVFIELD (ent, ltime, float);
		if (movetime < 0)
			movetime = 0;
	} else
		movetime = sv_frametime;

//  if (movetime)
//  {
	SV_PPushMove (ent, 0.0009);			// advances SVFIELD (ent, ltime, float) if not
										// blocked
//  }

	if (thinktime > oldltime && thinktime <= SVFIELD (ent, ltime, float)) {
		SVFIELD (ent, nextthink, float) = 0;
		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		PR_ExecuteProgram (&sv_pr_state, SVFIELD (ent, think, func));
		if (ent->free)
			return;
	}
}

//============================================================================

void
SV_ProgStartFrame (void)
{
// let the progs know that a new frame has started
	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
	*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
	*sv_globals.time = sv.time;
	PR_ExecuteProgram (&sv_pr_state, sv_funcs.StartFrame);
}

/*
	SV_RunEntity
*/
void
SV_RunEntity (edict_t *ent)
{
	if (SVFIELD (ent, lastruntime, float) == (float) realtime)
		return;
	SVFIELD (ent, lastruntime, float) = (float) realtime;

	switch ((int) SVFIELD (ent, movetype, float)) {
		case MOVETYPE_PUSH:
			SV_Physics_Pusher (ent);
			break;
		case MOVETYPE_PPUSH:
			SV_Physics_PPusher (ent);
			break;
		case MOVETYPE_NONE:
			SV_Physics_None (ent);
			break;
		case MOVETYPE_NOCLIP:
			SV_Physics_Noclip (ent);
			break;
		case MOVETYPE_STEP:
			SV_Physics_Step (ent);
			break;
		case MOVETYPE_TOSS:
		case MOVETYPE_BOUNCE:
		case MOVETYPE_FLY:
		case MOVETYPE_FLYMISSILE:
			SV_Physics_Toss (ent);
			break;
		default:
			SV_Error ("SV_Physics: bad movetype %i", (int) SVFIELD (ent, movetype, float));
	}
}

/*
	SV_RunNewmis
*/
void
SV_RunNewmis (void)
{
	edict_t    *ent;

	if (!*sv_globals.newmis)
		return;
	ent = PROG_TO_EDICT (&sv_pr_state, *sv_globals.newmis);
	sv_frametime = 0.05;
	*sv_globals.newmis = 0;

	SV_RunEntity (ent);
}

/*
	SV_Physics
*/
void
SV_Physics (void)
{
	int         i;
	edict_t    *ent;
	static double old_time;

// don't bother running a frame if sys_ticrate seconds haven't passed
	sv_frametime = realtime - old_time;
	if (sv_frametime < sv_mintic->value)
		return;
	if (sv_frametime > sv_maxtic->value)
		sv_frametime = sv_maxtic->value;
	old_time = realtime;

	*sv_globals.frametime = sv_frametime;

	SV_ProgStartFrame ();

//
// treat each object in turn
// even the world gets a chance to think
//
	ent = sv.edicts;
	for (i = 0; i < sv.num_edicts; i++, ent = NEXT_EDICT (&sv_pr_state, ent)) {
		if (ent->free)
			continue;

		if (*sv_globals.force_retouch)
			SV_LinkEdict (ent, true);	// force retouch even for stationary

		if (i > 0 && i <= MAX_CLIENTS)
			continue;					// clients are run directly from
										// packets

		SV_RunEntity (ent);
		SV_RunNewmis ();
	}

	if (*sv_globals.force_retouch)
		(*sv_globals.force_retouch)--;

// 2000-01-02 EndFrame function by Maddes/FrikaC  start
	if (EndFrame) {
		// let the progs know that the frame has ended
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		*sv_globals.time = sv.time;
		PR_ExecuteProgram (&sv_pr_state, EndFrame);
	}
// 2000-01-02 EndFrame function by Maddes/FrikaC  end
}

void
SV_SetMoveVars (void)
{
	movevars.gravity = sv_gravity->value;
	movevars.stopspeed = sv_stopspeed->value;
	movevars.maxspeed = sv_maxspeed->value;
	movevars.spectatormaxspeed = sv_spectatormaxspeed->value;
	movevars.accelerate = sv_accelerate->value;
	movevars.airaccelerate = sv_airaccelerate->value;
	movevars.wateraccelerate = sv_wateraccelerate->value;
	movevars.friction = sv_friction->value;
	movevars.waterfriction = sv_waterfriction->value;
	movevars.entgravity = 1.0;
}
