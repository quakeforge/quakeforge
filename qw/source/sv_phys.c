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
		if (((entvars_t*)&check->v)->movetype == MOVETYPE_PUSH
			|| ((entvars_t*)&check->v)->movetype == MOVETYPE_NONE
			|| ((entvars_t*)&check->v)->movetype == MOVETYPE_NOCLIP) continue;

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
		if (IS_NAN (((entvars_t*)&ent->v)->velocity[i])) {
			Con_Printf ("Got a NaN velocity on %s\n",
						PR_GetString (&sv_pr_state, ((entvars_t*)&ent->v)->classname));
			((entvars_t*)&ent->v)->velocity[i] = 0;
		}
		if (IS_NAN (((entvars_t*)&ent->v)->origin[i])) {
			Con_Printf ("Got a NaN origin on %s\n",
						PR_GetString (&sv_pr_state, ((entvars_t*)&ent->v)->classname));
			((entvars_t*)&ent->v)->origin[i] = 0;
		}
	}

// 1999-10-18 SV_MAXVELOCITY fix by Maddes  start
	wishspeed = Length (((entvars_t*)&ent->v)->velocity);
	if (wishspeed > sv_maxvelocity->value) {
		VectorScale (((entvars_t*)&ent->v)->velocity, sv_maxvelocity->value / wishspeed,
					 ((entvars_t*)&ent->v)->velocity);
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
		thinktime = ((entvars_t*)&ent->v)->nextthink;
		if (thinktime <= 0)
			return true;
		if (thinktime > sv.time + sv_frametime)
			return true;

		if (thinktime < sv.time)
			thinktime = sv.time;		// don't let things stay in the past.
		// it is possible to start that way
		// by a trigger with a local time.
		((entvars_t*)&ent->v)->nextthink = 0;
		*sv_globals.time = thinktime;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		PR_ExecuteProgram (&sv_pr_state, ((entvars_t*)&ent->v)->think);

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
	if (((entvars_t*)&e1->v)->touch && ((entvars_t*)&e1->v)->solid != SOLID_NOT) {
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, e1);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, e2);
		PR_ExecuteProgram (&sv_pr_state, ((entvars_t*)&e1->v)->touch);
	}

	if (((entvars_t*)&e2->v)->touch && ((entvars_t*)&e2->v)->solid != SOLID_NOT) {
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, e2);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, e1);
		PR_ExecuteProgram (&sv_pr_state, ((entvars_t*)&e2->v)->touch);
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
	VectorCopy (((entvars_t*)&ent->v)->velocity, original_velocity);
	VectorCopy (((entvars_t*)&ent->v)->velocity, primal_velocity);
	numplanes = 0;

	time_left = time;

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
		for (i = 0; i < 3; i++)
			end[i] = ((entvars_t*)&ent->v)->origin[i] + time_left * ((entvars_t*)&ent->v)->velocity[i];

		trace =
			SV_Move (((entvars_t*)&ent->v)->origin, ((entvars_t*)&ent->v)->mins, ((entvars_t*)&ent->v)->maxs, end, false, ent);

		if (trace.allsolid) {			// entity is trapped in another solid
			VectorCopy (vec3_origin, ((entvars_t*)&ent->v)->velocity);
			return 3;
		}

		if (trace.fraction > 0) {		// actually covered some distance
			VectorCopy (trace.endpos, ((entvars_t*)&ent->v)->origin);
			VectorCopy (((entvars_t*)&ent->v)->velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break;						// moved the entire distance

		if (!trace.ent)
			SV_Error ("SV_FlyMove: !trace.ent");

		if (trace.plane.normal[2] > 0.7) {
			blocked |= 1;				// floor
			if ((((entvars_t*)&trace.ent->v)->solid == SOLID_BSP)
				|| (((entvars_t*)&trace.ent->v)->movetype == MOVETYPE_PPUSH)) {
				((entvars_t*)&ent->v)->flags = (int) ((entvars_t*)&ent->v)->flags | FL_ONGROUND;
				((entvars_t*)&ent->v)->groundentity = EDICT_TO_PROG (&sv_pr_state, trace.ent);
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
			VectorCopy (vec3_origin, ((entvars_t*)&ent->v)->velocity);
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
			VectorCopy (new_velocity, ((entvars_t*)&ent->v)->velocity);
		} else {						// go along the crease
			if (numplanes != 2) {
//              Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
				VectorCopy (vec3_origin, ((entvars_t*)&ent->v)->velocity);
				return 7;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, ((entvars_t*)&ent->v)->velocity);
			VectorScale (dir, d, ((entvars_t*)&ent->v)->velocity);
		}

//
// if original velocity is against the original velocity, stop dead
// to avoid tiny occilations in sloping corners
//
		if (DotProduct (((entvars_t*)&ent->v)->velocity, primal_velocity) <= 0) {
			VectorCopy (vec3_origin, ((entvars_t*)&ent->v)->velocity);
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
	((entvars_t*)&ent->v)->velocity[2] -= scale * movevars.gravity * sv_frametime;
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

	VectorAdd (((entvars_t*)&ent->v)->origin, push, end);

	if (((entvars_t*)&ent->v)->movetype == MOVETYPE_FLYMISSILE)
		trace =
			SV_Move (((entvars_t*)&ent->v)->origin, ((entvars_t*)&ent->v)->mins, ((entvars_t*)&ent->v)->maxs, end, MOVE_MISSILE,
					 ent);
	else if (((entvars_t*)&ent->v)->solid == SOLID_TRIGGER || ((entvars_t*)&ent->v)->solid == SOLID_NOT)
		// only clip against bmodels
		trace =
			SV_Move (((entvars_t*)&ent->v)->origin, ((entvars_t*)&ent->v)->mins, ((entvars_t*)&ent->v)->maxs, end,
					 MOVE_NOMONSTERS, ent);
	else
		trace =
			SV_Move (((entvars_t*)&ent->v)->origin, ((entvars_t*)&ent->v)->mins, ((entvars_t*)&ent->v)->maxs, end, MOVE_NORMAL,
					 ent);

	VectorCopy (trace.endpos, ((entvars_t*)&ent->v)->origin);
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
		mins[i] = ((entvars_t*)&pusher->v)->absmin[i] + move[i];
		maxs[i] = ((entvars_t*)&pusher->v)->absmax[i] + move[i];
	}

	VectorCopy (((entvars_t*)&pusher->v)->origin, pushorig);

// move the pusher to it's final position

	VectorAdd (((entvars_t*)&pusher->v)->origin, move, ((entvars_t*)&pusher->v)->origin);
	SV_LinkEdict (pusher, false);

// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)
			continue;
		if (((entvars_t*)&check->v)->movetype == MOVETYPE_PUSH
			|| ((entvars_t*)&check->v)->movetype == MOVETYPE_NONE
			|| ((entvars_t*)&check->v)->movetype == MOVETYPE_PPUSH
			|| ((entvars_t*)&check->v)->movetype == MOVETYPE_NOCLIP) continue;

		// Don't assume SOLID_BSP !  --KB
		solid_save = ((entvars_t*)&pusher->v)->solid;
		((entvars_t*)&pusher->v)->solid = SOLID_NOT;
		block = SV_TestEntityPosition (check);
		// ((entvars_t*)&pusher->v)->solid = SOLID_BSP;
		((entvars_t*)&pusher->v)->solid = solid_save;
		if (block)
			continue;

		// if the entity is standing on the pusher, it will definately be
		// moved
		if (!(((int) ((entvars_t*)&check->v)->flags & FL_ONGROUND)
			  && PROG_TO_EDICT (&sv_pr_state, ((entvars_t*)&check->v)->groundentity) == pusher)) {
			if (((entvars_t*)&check->v)->absmin[0] >= maxs[0]
				|| ((entvars_t*)&check->v)->absmin[1] >= maxs[1]
				|| ((entvars_t*)&check->v)->absmin[2] >= maxs[2]
				|| ((entvars_t*)&check->v)->absmax[0] <= mins[0]
				|| ((entvars_t*)&check->v)->absmax[1] <= mins[1]
				|| ((entvars_t*)&check->v)->absmax[2] <= mins[2])
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition (check))
				continue;
		}

		VectorCopy (((entvars_t*)&check->v)->origin, moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		// try moving the contacted entity
		VectorAdd (((entvars_t*)&check->v)->origin, move, ((entvars_t*)&check->v)->origin);
		block = SV_TestEntityPosition (check);
		if (!block) {					// pushed ok
			SV_LinkEdict (check, false);
			continue;
		}
		// if it is ok to leave in the old position, do it
		VectorSubtract (((entvars_t*)&check->v)->origin, move, ((entvars_t*)&check->v)->origin);
		block = SV_TestEntityPosition (check);
		if (!block) {
			num_moved--;
			continue;
		}
		// if it is still inside the pusher, block
		if (((entvars_t*)&check->v)->mins[0] == ((entvars_t*)&check->v)->maxs[0]) {
			SV_LinkEdict (check, false);
			continue;
		}
		if (((entvars_t*)&check->v)->solid == SOLID_NOT || ((entvars_t*)&check->v)->solid == SOLID_TRIGGER) {	// corpse
			((entvars_t*)&check->v)->mins[0] = ((entvars_t*)&check->v)->mins[1] = 0;
			VectorCopy (((entvars_t*)&check->v)->mins, ((entvars_t*)&check->v)->maxs);
			SV_LinkEdict (check, false);
			continue;
		}

		VectorCopy (pushorig, ((entvars_t*)&pusher->v)->origin);
		SV_LinkEdict (pusher, false);

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if (((entvars_t*)&pusher->v)->blocked) {
			*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, pusher);
			*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, check);
			PR_ExecuteProgram (&sv_pr_state, ((entvars_t*)&pusher->v)->blocked);
		}
		// move back any entities we already moved
		for (i = 0; i < num_moved; i++) {
			VectorCopy (moved_from[i], ((entvars_t*)&moved_edict[i]->v)->origin);
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

	if (!((entvars_t*)&pusher->v)->velocity[0] && !((entvars_t*)&pusher->v)->velocity[1]
		&& !((entvars_t*)&pusher->v)->velocity[2]) {
		((entvars_t*)&pusher->v)->ltime += movetime;
		return;
	}

	for (i = 0; i < 3; i++)
		move[i] = ((entvars_t*)&pusher->v)->velocity[i] * movetime;

	if (SV_Push (pusher, move))
		((entvars_t*)&pusher->v)->ltime += movetime;
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

	oldltime = ((entvars_t*)&ent->v)->ltime;

	thinktime = ((entvars_t*)&ent->v)->nextthink;
	if (thinktime < ((entvars_t*)&ent->v)->ltime + sv_frametime) {
		movetime = thinktime - ((entvars_t*)&ent->v)->ltime;
		if (movetime < 0)
			movetime = 0;
	} else
		movetime = sv_frametime;

	if (movetime) {
		SV_PushMove (ent, movetime);	// advances ((entvars_t*)&ent->v)->ltime if not
										// blocked
	}

	if (thinktime > oldltime && thinktime <= ((entvars_t*)&ent->v)->ltime) {
		VectorCopy (((entvars_t*)&ent->v)->origin, oldorg);
		((entvars_t*)&ent->v)->nextthink = 0;
		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		PR_ExecuteProgram (&sv_pr_state, ((entvars_t*)&ent->v)->think);
		if (ent->free)
			return;
		VectorSubtract (((entvars_t*)&ent->v)->origin, oldorg, move);

		l = Length (move);
		if (l > 1.0 / 64) {
//          Con_Printf ("**** snap: %f\n", Length (l));
			VectorCopy (oldorg, ((entvars_t*)&ent->v)->origin);
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

	VectorMA (((entvars_t*)&ent->v)->angles, sv_frametime, ((entvars_t*)&ent->v)->avelocity, ((entvars_t*)&ent->v)->angles);
	VectorMA (((entvars_t*)&ent->v)->origin, sv_frametime, ((entvars_t*)&ent->v)->velocity, ((entvars_t*)&ent->v)->origin);

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

	cont = SV_PointContents (((entvars_t*)&ent->v)->origin);
	if (!((entvars_t*)&ent->v)->watertype) {			// just spawned here
		((entvars_t*)&ent->v)->watertype = cont;
		((entvars_t*)&ent->v)->waterlevel = 1;
		return;
	}

	if (cont <= CONTENTS_WATER) {
		if (((entvars_t*)&ent->v)->watertype == CONTENTS_EMPTY) {	// just crossed into
													// water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}
		((entvars_t*)&ent->v)->watertype = cont;
		((entvars_t*)&ent->v)->waterlevel = 1;
	} else {
		if (((entvars_t*)&ent->v)->watertype != CONTENTS_EMPTY) {	// just crossed into
													// water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}
		((entvars_t*)&ent->v)->watertype = CONTENTS_EMPTY;
		((entvars_t*)&ent->v)->waterlevel = cont;
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

	if (((entvars_t*)&ent->v)->velocity[2] > 0)
		((entvars_t*)&ent->v)->flags = (int) ((entvars_t*)&ent->v)->flags & ~FL_ONGROUND;

// if onground, return without moving
	if (((int) ((entvars_t*)&ent->v)->flags & FL_ONGROUND))
		return;

	SV_CheckVelocity (ent);

// add gravity
	if (((entvars_t*)&ent->v)->movetype != MOVETYPE_FLY
		&& ((entvars_t*)&ent->v)->movetype != MOVETYPE_FLYMISSILE) SV_AddGravity (ent, 1.0);

// move angles
	VectorMA (((entvars_t*)&ent->v)->angles, sv_frametime, ((entvars_t*)&ent->v)->avelocity, ((entvars_t*)&ent->v)->angles);

// move origin
	VectorScale (((entvars_t*)&ent->v)->velocity, sv_frametime, move);
	trace = SV_PushEntity (ent, move);
	if (trace.fraction == 1)
		return;
	if (ent->free)
		return;

	if (((entvars_t*)&ent->v)->movetype == MOVETYPE_BOUNCE)
		backoff = 1.5;
	else
		backoff = 1;

	ClipVelocity (((entvars_t*)&ent->v)->velocity, trace.plane.normal, ((entvars_t*)&ent->v)->velocity,
				  backoff);

// stop if on ground
	if (trace.plane.normal[2] > 0.7) {
		if (((entvars_t*)&ent->v)->velocity[2] < 60 || ((entvars_t*)&ent->v)->movetype != MOVETYPE_BOUNCE) {
			((entvars_t*)&ent->v)->flags = (int) ((entvars_t*)&ent->v)->flags | FL_ONGROUND;
			((entvars_t*)&ent->v)->groundentity = EDICT_TO_PROG (&sv_pr_state, trace.ent);
			VectorCopy (vec3_origin, ((entvars_t*)&ent->v)->velocity);
			VectorCopy (vec3_origin, ((entvars_t*)&ent->v)->avelocity);
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
	if (!((int) ((entvars_t*)&ent->v)->flags & (FL_ONGROUND | FL_FLY | FL_SWIM))) {
		if (((entvars_t*)&ent->v)->velocity[2] < movevars.gravity * -0.1)
			hitsound = true;
		else
			hitsound = false;

		SV_AddGravity (ent, 1.0);
		SV_CheckVelocity (ent);
		SV_FlyMove (ent, sv_frametime, NULL);
		SV_LinkEdict (ent, true);

		if ((int) ((entvars_t*)&ent->v)->flags & FL_ONGROUND)	// just hit ground
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
		move[i] = ((entvars_t*)&pusher->v)->velocity[i] * movetime;
		mins[i] = ((entvars_t*)&pusher->v)->absmin[i] + move[i];
		maxs[i] = ((entvars_t*)&pusher->v)->absmax[i] + move[i];
	}

	VectorCopy (((entvars_t*)&pusher->v)->origin, ((entvars_t*)&pusher->v)->oldorigin);	// Backup origin
	trace =
		SV_Move (((entvars_t*)&pusher->v)->origin, ((entvars_t*)&pusher->v)->mins, ((entvars_t*)&pusher->v)->maxs, move,
				 MOVE_NOMONSTERS, pusher);

	if (trace.fraction == 1) {
		VectorCopy (((entvars_t*)&pusher->v)->origin, ((entvars_t*)&pusher->v)->oldorigin);	// Revert
		return;
	}


	VectorAdd (((entvars_t*)&pusher->v)->origin, move, ((entvars_t*)&pusher->v)->origin);	// Move
	SV_LinkEdict (pusher, false);
	((entvars_t*)&pusher->v)->ltime += movetime;

	oldsolid = ((entvars_t*)&pusher->v)->solid;

	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)				// What entity?
			continue;

		// Stage 1: Is it in contact with me?
		if (!SV_TestEntityPosition (check))	// Nope
			continue;

		// Stage 2: Is it a player we can push?
		if (((entvars_t*)&check->v)->movetype == MOVETYPE_WALK) {
			Con_Printf ("Pusher encountered a player\n");	// Yes!@#!@
			((entvars_t*)&pusher->v)->solid = SOLID_NOT;
			SV_PushEntity (check, move);
			((entvars_t*)&pusher->v)->solid = oldsolid;
			continue;
		}
		// Stage 3: No.. Is it something that blocks us?
		if (((entvars_t*)&check->v)->mins[0] == ((entvars_t*)&check->v)->maxs[0])
			continue;
		if (((entvars_t*)&check->v)->solid == SOLID_NOT || ((entvars_t*)&check->v)->solid == SOLID_TRIGGER)
			continue;

		// Stage 4: Yes, it must be. Fail the move.
		VectorCopy (((entvars_t*)&pusher->v)->origin, ((entvars_t*)&pusher->v)->oldorigin);	// Revert
		if (((entvars_t*)&pusher->v)->blocked) {		// Blocked func?
			*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, pusher);
			*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, check);
			PR_ExecuteProgram (&sv_pr_state, ((entvars_t*)&pusher->v)->blocked);
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

	oldltime = ((entvars_t*)&ent->v)->ltime;

	thinktime = ((entvars_t*)&ent->v)->nextthink;
	if (thinktime < ((entvars_t*)&ent->v)->ltime + sv_frametime) {
		movetime = thinktime - ((entvars_t*)&ent->v)->ltime;
		if (movetime < 0)
			movetime = 0;
	} else
		movetime = sv_frametime;

//  if (movetime)
//  {
	SV_PPushMove (ent, 0.0009);			// advances ((entvars_t*)&ent->v)->ltime if not
										// blocked
//  }

	if (thinktime > oldltime && thinktime <= ((entvars_t*)&ent->v)->ltime) {
		((entvars_t*)&ent->v)->nextthink = 0;
		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		PR_ExecuteProgram (&sv_pr_state, ((entvars_t*)&ent->v)->think);
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
	if (((entvars_t*)&ent->v)->lastruntime == (float) realtime)
		return;
	((entvars_t*)&ent->v)->lastruntime = (float) realtime;

	switch ((int) ((entvars_t*)&ent->v)->movetype) {
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
			SV_Error ("SV_Physics: bad movetype %i", (int) ((entvars_t*)&ent->v)->movetype);
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
