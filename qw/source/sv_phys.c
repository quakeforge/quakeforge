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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "QF/sys.h"

#include "qw/include/server.h"
#include "qw/include/sv_progs.h"
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

	solid_edge items clip against only bsp models.
*/

cvar_t     *sv_friction;
cvar_t     *sv_gravity;
cvar_t     *sv_jump_any;
cvar_t     *sv_maxvelocity;
cvar_t     *sv_stopspeed;

#define	MOVE_EPSILON	0.01
#if 0
static void
SV_CheckAllEnts (void)
{
	edict_t    *check;
	int         e;

	// see if any solid entities are inside the final position
	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts;
		 e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)
			continue;
		if (SVfloat (check, movetype) == MOVETYPE_PUSH
			|| SVfloat (check, movetype) == MOVETYPE_NONE
			|| SVfloat (check, movetype) == MOVETYPE_NOCLIP)
			continue;

		if (SV_TestEntityPosition (check))
			Sys_Printf ("entity in invalid position\n");
	}
}
#endif
void
SV_CheckVelocity (edict_t *ent)
{
	float       wishspeed;
//	int         i;

	// bound velocity
#if 0
	for (i = 0; i < 3; i++) {
		if (IS_NAN (SVvector (ent, velocity)[i])) {
			Sys_Printf ("Got a NaN velocity on %s\n",
						PR_GetString (&sv_pr_state, SVstring (ent,
															  classname)));
			SVvector (ent, velocity)[i] = 0;
		}
		if (IS_NAN (SVvector (ent, origin)[i])) {
			Sys_Printf ("Got a NaN origin on %s\n",
						PR_GetString (&sv_pr_state, SVstring (ent,
															  classname)));
			SVvector (ent, origin)[i] = 0;
		}
	}
#endif
	wishspeed = VectorLength (SVvector (ent, velocity));
	if (wishspeed > sv_maxvelocity->value) {
		VectorScale (SVvector (ent, velocity), sv_maxvelocity->value /
					 wishspeed, SVvector (ent, velocity));
	}
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
		thinktime = SVfloat (ent, nextthink);
		if (thinktime <= 0 || thinktime > sv.time + sv_frametime)
			return true;

		if (thinktime < sv.time)
			thinktime = sv.time;		// don't let things stay in the past.
										// it is possible to start that way
										// by a trigger with a local time.
		SVfloat (ent, nextthink) = 0;
		*sv_globals.time = thinktime;
		sv_pr_think (ent);

		if (ent->free)
			return false;
	} while (SVfloat (ent, nextthink) > thinktime);

	return true;
}

/*
	SV_Impact

	Two entities have touched, so run their touch functions
*/
static void
SV_Impact (edict_t *e1, edict_t *e2)
{
	int         old_self, old_other;

	old_self = *sv_globals.self;
	old_other = *sv_globals.other;

	*sv_globals.time = sv.time;
	if (SVfunc (e1, touch) && SVfloat (e1, solid) != SOLID_NOT) {
		sv_pr_touch (e1, e2);
	}

	if (SVfunc (e2, touch) && SVfloat (e2, solid) != SOLID_NOT) {
		sv_pr_touch (e2, e1);
	}

	*sv_globals.self = old_self;
	*sv_globals.other = old_other;
}

/*
	ClipVelocity

	Slide off of the impacting object
	returns the blocked flags (1 = floor, 2 = step / wall)
*/
static int
ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float       backoff, change;
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

int
SV_EntCanSupportJump (edict_t *ent)
{
	int         solid = SVfloat (ent, solid);
	if (solid == SOLID_BSP)
		return 1;
	if (!sv_jump_any->int_val)
		return 0;
	if (solid == SOLID_NOT || solid == SOLID_SLIDEBOX)
		return 0;
	return 1;
}

#define	MAX_CLIP_PLANES	5

/*
	SV_FlyMove

	The basic solid body movement clip that slides along multiple planes
	Returns the clipflags if the velocity was modified (hit something solid)
	1 = floor
	2 = wall / step
	4 = dead stop
	If steptrace is not NULL, the trace of any vertical wall hit will be stored
*/
int
SV_FlyMove (edict_t *ent, float time, trace_t *steptrace)
{
	float       d, time_left;
	int         blocked, bumpcount, numbumps, numplanes, i, j;
	trace_t     trace;
	vec3_t      dir, end;
	vec3_t      planes[MAX_CLIP_PLANES];
	vec3_t      primal_velocity, original_velocity, new_velocity;

	numbumps = 4;

	blocked = 0;
	VectorCopy (SVvector (ent, velocity), original_velocity);
	VectorCopy (SVvector (ent, velocity), primal_velocity);
	numplanes = 0;

	time_left = time;

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
		if (VectorIsZero (SVvector (ent, velocity)))
			break;

		VectorMultAdd (SVvector (ent, origin), time_left,
					   SVvector (ent, velocity), end);
		if (SVdata (ent)->add_grav) {
			SVdata (ent)->add_grav = false;
			SV_FinishGravity (ent, end);
		}

		trace = SV_Move (SVvector (ent, origin), SVvector (ent, mins),
						 SVvector (ent, maxs), end, false, ent);

		if (trace.allsolid) {			// entity is trapped in another solid
			VectorZero (SVvector (ent, velocity));
			return 3;
		}

		if (trace.fraction > 0) {		// actually covered some distance
			VectorCopy (trace.endpos, SVvector (ent, origin));
			VectorCopy (SVvector (ent, velocity), original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break;						// moved the entire distance

		if (!trace.ent)
			Sys_Error ("SV_FlyMove: !trace.ent");

		if (trace.plane.normal[2] > 0.7) {
			blocked |= 1;				// floor
			if (SV_EntCanSupportJump (trace.ent)) {
				SVfloat (ent, flags) = (int) SVfloat (ent, flags) |
					FL_ONGROUND;
				SVentity (ent, groundentity) = EDICT_TO_PROG (&sv_pr_state,
															  trace.ent);
			}
		}
		if (!trace.plane.normal[2]) {
			blocked |= 2;				// step
			if (steptrace)
				*steptrace = trace;		// save for player extrafriction
		}

		// run the impact function
		SV_Impact (ent, trace.ent);
		if (ent->free)
			break;						// removed by the impact function

		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES) {	// this shouldn't really happen
			VectorZero (SVvector (ent, velocity));
			return 3;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
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
			VectorCopy (new_velocity, SVvector (ent, velocity));
		} else {						// go along the crease
			if (numplanes != 2) {
				VectorZero (SVvector (ent, velocity));
				return 7;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, SVvector (ent, velocity));
			VectorScale (dir, d, SVvector (ent, velocity));
		}

		// if original velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if (DotProduct (SVvector (ent, velocity), primal_velocity) <= 0) {
			VectorZero (SVvector (ent, velocity));
			return blocked;
		}
	}

	return blocked;
}

void
SV_AddGravity (edict_t *ent)
{
	float       ent_grav;

	if (sv_fields.gravity != -1 && SVfloat (ent, gravity))
		ent_grav = SVfloat (ent, gravity);
	else
		ent_grav = 1.0;
	SVvector (ent, velocity)[2] -= ent_grav * sv_gravity->value * sv_frametime;
	SVdata (ent)->add_grav = true;
}

void
SV_FinishGravity (edict_t *ent, vec3_t move)
{
	float       ent_grav;

	if (sv_fields.gravity != -1 && SVfloat (ent, gravity))
		ent_grav = SVfloat (ent, gravity);
	else
		ent_grav = 1.0;
	ent_grav *= sv_gravity->value;
	move[2] += ent_grav * sv_frametime * sv_frametime / 2;
}

/* PUSHMOVE */

/*
	SV_PushEntity

	Does not change the entities velocity at all
*/
trace_t
SV_PushEntity (edict_t *ent, vec3_t push, unsigned traceflags)
{
	trace_t     trace;
	vec3_t      end;
	vec_t      *e_origin, *e_mins, *e_maxs;
	int         e_movetype, e_solid;

	e_origin = SVvector (ent, origin);
	e_mins = SVvector (ent, mins);
	e_maxs = SVvector (ent, maxs);
	e_movetype = SVfloat (ent, movetype);
	e_solid = SVfloat (ent, solid);

	VectorAdd (e_origin, push, end);

	if ((int) SVfloat (ent, flags) & FLQW_LAGGEDMOVE)
		traceflags |= MOVE_LAGGED;

	if (e_movetype == MOVETYPE_FLYMISSILE)
		traceflags |= MOVE_MISSILE;
	else if (e_solid == SOLID_TRIGGER || e_solid == SOLID_NOT)
		// clip against only bmodels
		traceflags |= MOVE_NOMONSTERS;
	else
		traceflags |= MOVE_NORMAL;
	trace = SV_Move (e_origin, e_mins, e_maxs, end, traceflags, ent);

	VectorCopy (trace.endpos, e_origin);
	SV_LinkEdict (ent, true);

	if (trace.ent)
		SV_Impact (ent, trace.ent);

	return trace;
}

static qboolean
SV_Push (edict_t *pusher, const vec3_t tmove, const vec3_t amove)
{
	float       solid_save;
	int         num_moved, i, e;
	edict_t    *check, *block;
	edict_t   **moved_edict;
	vec3_t      move, org, org2;
	vec3_t      mins, maxs, pushtorig, pushaorig;
	vec3_t     *moved_from;
	vec3_t      forward = {1, 0, 0};
	vec3_t      left    = {0, 1, 0};
	vec3_t      up      = {0, 0, 1};
	size_t      mark;
	int         c_flags, c_movetype, c_groundentity, c_solid;
	vec_t      *c_absmin, *c_absmax, *c_origin, *c_angles, *c_mins, *c_maxs;
	vec_t      *p_origin, *p_angles;

	VectorAdd (SVvector (pusher, absmin), tmove, mins);
	VectorAdd (SVvector (pusher, absmax), tmove, maxs);

	if (!VectorIsZero (amove)) {
		vec3_t      a;
		VectorSubtract (vec3_origin, amove, a);
		AngleVectors (a, forward, left, up);
		VectorNegate (left, left);	// AngleVectors is right-handed
	}

	p_origin = SVvector (pusher, origin);
	p_angles = SVvector (pusher, angles);
	VectorCopy (p_origin, pushtorig);
	VectorCopy (p_angles, pushaorig);

	// move the pusher to it's final position
	VectorAdd (p_origin, tmove, p_origin);
	VectorAdd (p_angles, amove, p_angles);
	SV_LinkEdict (pusher, false);

	mark = Hunk_LowMark (0);
	moved_edict = Hunk_Alloc (0, sv.num_edicts * sizeof (edict_t *));
	moved_from = Hunk_Alloc (0, sv.num_edicts * sizeof (vec_t));

	// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts;
		 e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)
			continue;
		c_movetype = SVfloat (check, movetype);
		if (c_movetype == MOVETYPE_PUSH || c_movetype == MOVETYPE_NONE
			|| c_movetype == MOVETYPE_NOCLIP)
			continue;

		// If the entity is in another solid, it's not free to move. Make the
		// pusher non-solid to ensure it doesn't interfere with the check.
		solid_save = SVfloat (pusher, solid);
		SVfloat (pusher, solid) = SOLID_NOT;
		block = SV_TestEntityPosition (check);
		SVfloat (pusher, solid) = solid_save;
		if (block)
			continue;

		// if the entity is standing on the pusher, it will definately be moved
		c_flags = SVfloat (check, flags);
		c_groundentity = SVentity (check, groundentity);
		if (!(c_flags & FL_ONGROUND)
			  || PROG_TO_EDICT (&sv_pr_state, c_groundentity) != pusher) {
			// The entity is NOT standing on pusher, so check whether the
			// entity is inside the pusher's final position.
			// FIXME what if the pusher is moving so fast it skips past the
			// entity?
			c_absmin = SVvector (check, absmin);
			c_absmax = SVvector (check, absmax);
			if (VectorCompCompareAll (c_absmin, >=, maxs)
				|| VectorCompCompareAll (c_absmax, <=, mins))
				continue;

			if (!SV_TestEntityPosition (check))
				continue;
			// The pusher and entity collide, so push the entity.
		}
		// remove the onground flag for non-players
		if (c_movetype != MOVETYPE_WALK)
			SVfloat (check, flags) = c_flags & ~FL_ONGROUND;

		c_origin = SVvector (check, origin);
		VectorCopy (c_origin, moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		// calculate destination position
		VectorSubtract (c_origin, p_origin, org);
		org2[0] = DotProduct (org, forward);
		org2[1] = DotProduct (org, left);
		org2[2] = DotProduct (org, up);
		VectorSubtract (org2, org, move);
		VectorAdd (move, tmove, move);

		// try moving the contacted entity
		solid_save = SVfloat (pusher, solid);
		SVfloat (pusher, solid) = SOLID_NOT;
		SV_PushEntity (check, move, MOVE_NORMAL);
		SVfloat (pusher, solid) = solid_save;

		block = SV_TestEntityPosition (check);
		if (!block) {
			c_angles = SVvector (check, angles);
			VectorAdd (c_angles, amove, c_angles);
			continue;
		}
		// if it is still inside the pusher, block
		c_mins = SVvector (check, mins);
		c_maxs = SVvector (check, maxs);
		c_solid = SVfloat (check, solid);
		if (c_mins[0] == c_maxs[0]
			|| c_solid == SOLID_NOT || c_solid == SOLID_TRIGGER) {	// corpse
			c_mins[0] = c_mins[1] = 0;
			VectorCopy (c_mins, c_maxs);
			SV_LinkEdict (check, false);
			continue;
		}

		VectorCopy (pushtorig, p_origin);
		VectorCopy (pushaorig, p_angles);
		SV_LinkEdict (pusher, false);

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if (SVfunc (pusher, blocked))
			sv_pr_blocked (pusher, check);

		// move back any entities we already moved
		for (i = 0; i < num_moved; i++) {
			vec_t      *m_origin = SVvector (moved_edict[i], origin);
			vec_t      *m_angles = SVvector (moved_edict[i], angles);
			VectorCopy (moved_from[i], m_origin);
			VectorSubtract (m_angles, amove, m_angles);
			SV_LinkEdict (moved_edict[i], false);
		}
		Hunk_FreeToLowMark (0, mark);
		return false;
	}
	Hunk_FreeToLowMark (0, mark);
	return true;
}

static void
SV_PushMove (edict_t *pusher, float movetime)
{
	vec3_t      move;
	vec3_t      amove;

	if (VectorIsZero (SVvector (pusher, velocity))
		&& VectorIsZero (SVvector (pusher, avelocity))) {
		SVfloat (pusher, ltime) += movetime;
		return;
	}

	VectorScale (SVvector (pusher, velocity), movetime, move);
	//FIXME finish gravity
	VectorScale (SVvector (pusher, avelocity), movetime, amove);

	if (SV_Push (pusher, move, amove))
		SVfloat (pusher, ltime) += movetime;
}

static void
SV_Physics_Pusher (edict_t *ent)
{
	float       movetime, oldltime, thinktime;
	float       l;
	vec3_t      oldorg, move;

	oldltime = SVfloat (ent, ltime);

	thinktime = SVfloat (ent, nextthink);
	if (thinktime < SVfloat (ent, ltime) + sv_frametime) {
		movetime = thinktime - SVfloat (ent, ltime);
		if (movetime < 0)
			movetime = 0;
	} else
		movetime = sv_frametime;

	if (movetime) {
		SV_PushMove (ent, movetime);	// advances SVfloat (ent, ltime) if not
										// blocked
	}

	if (thinktime > oldltime && thinktime <= SVfloat (ent, ltime)) {
		VectorCopy (SVvector (ent, origin), oldorg);
		SVfloat (ent, nextthink) = 0;
		*sv_globals.time = sv.time;
		sv_pr_think (ent);
		if (ent->free)
			return;
		VectorSubtract (SVvector (ent, origin), oldorg, move);

		l = VectorLength (move);
		if (l > (1.0 / 64.0)) {
			VectorCopy (oldorg, SVvector (ent, origin));
			SV_Push (ent, move, vec3_origin);	//FIXME angle
		}
	}
}

/*
	SV_Physics_None

	Non moving objects can only think
*/
static void
SV_Physics_None (edict_t *ent)
{
	// regular thinking
	if (SV_RunThink (ent))
		SV_LinkEdict (ent, false);
}

/*
	SV_Physics_Noclip

	A moving object that doesn't obey physics
*/
static void
SV_Physics_Noclip (edict_t *ent)
{
	// regular thinking
	if (!SV_RunThink (ent))
		return;

	VectorMultAdd (SVvector (ent, angles), sv_frametime,
				   SVvector (ent, avelocity), SVvector (ent, angles));
	VectorMultAdd (SVvector (ent, origin), sv_frametime,
				   SVvector (ent, velocity), SVvector (ent, origin));

	SV_LinkEdict (ent, false);
}

/* TOSS / BOUNCE */

static void
SV_CheckWaterTransition (edict_t *ent)
{
	int         cont;

	cont = SV_PointContents (SVvector (ent, origin));

	if (!SVfloat (ent, watertype)) {			// just spawned here
		SVfloat (ent, watertype) = cont;
		SVfloat (ent, waterlevel) = 1;
		return;
	}

	if (cont <= CONTENTS_WATER) {
		if (SVfloat (ent, watertype) == CONTENTS_EMPTY) {
			// just crossed into water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}
		SVfloat (ent, watertype) = cont;
		SVfloat (ent, waterlevel) = 1;
	} else {
		if (SVfloat (ent, watertype) != CONTENTS_EMPTY) {
			// just crossed into water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}
		SVfloat (ent, watertype) = CONTENTS_EMPTY;
		SVfloat (ent, waterlevel) = cont;
	}
}

/*
	SV_Physics_Toss

	Toss, bounce, and fly movement.  When onground, do nothing.
*/
void
SV_Physics_Toss (edict_t *ent)
{
	float       backoff;
	trace_t     trace;
	vec3_t      move;
	int         fl;

	// regular thinking
	if (!SV_RunThink (ent))
		return;

	if (SVvector (ent, velocity)[2] > 0)
		SVfloat (ent, flags) = (int) SVfloat (ent, flags) & ~FL_ONGROUND;

	// if onground, return without moving
	if (((int) SVfloat (ent, flags) & FL_ONGROUND))
		return;

	SV_CheckVelocity (ent);

	// add gravity
	if (SVfloat (ent, movetype) != MOVETYPE_FLY
		&& SVfloat (ent, movetype) != MOVETYPE_FLYMISSILE)
		SV_AddGravity (ent);

	// move angles
	VectorMultAdd (SVvector (ent, angles), sv_frametime,
				   SVvector (ent, avelocity), SVvector (ent, angles));

	// move origin
	VectorScale (SVvector (ent, velocity), sv_frametime, move);
	if (SVdata (ent)->add_grav) {
		SVdata (ent)->add_grav = false;
		SV_FinishGravity (ent, move);
	}

	fl = 0;
	if (sv_antilag->int_val == 2)
		fl |= MOVE_LAGGED;
	trace = SV_PushEntity (ent, move, fl);
	if (trace.fraction == 1)
		return;
	if (ent->free)
		return;

	if (SVfloat (ent, movetype) == MOVETYPE_BOUNCE)
		backoff = 1.5;
	else
		backoff = 1;

	ClipVelocity (SVvector (ent, velocity), trace.plane.normal,
				  SVvector (ent, velocity), backoff);

	// stop if on ground
	if (trace.plane.normal[2] > 0.7) {
		if (SVvector (ent, velocity)[2] < 60
			|| SVfloat (ent, movetype) != MOVETYPE_BOUNCE) {
			SVfloat (ent, flags) = (int) SVfloat (ent, flags) | FL_ONGROUND;
			SVentity (ent, groundentity) = EDICT_TO_PROG (&sv_pr_state,
														  trace.ent);
			VectorZero (SVvector (ent, velocity));
			VectorZero (SVvector (ent, avelocity));
		}
	}
	// check for in water
	SV_CheckWaterTransition (ent);
}

/* STEPPING MOVEMENT */

/*
	SV_Physics_Step

	Monsters freefall when they don't have a ground entity, otherwise
	all movement is done with discrete steps.

	This is also used for objects that have become still on the ground, but
	will fall if the floor is pulled out from under them.
	FIXME: is this true?
*/
static void
SV_Physics_Step (edict_t *ent)
{
	qboolean    hitsound;

	// freefall if not on ground
	if (!((int) SVfloat (ent, flags) & (FL_ONGROUND | FL_FLY | FL_SWIM))) {
		if (SVvector (ent, velocity)[2] < sv_gravity->value * -0.1)
			hitsound = true;
		else
			hitsound = false;

		SV_AddGravity (ent);
		SV_CheckVelocity (ent);
		SV_FlyMove (ent, sv_frametime, NULL);
		SV_LinkEdict (ent, true);

		if ((int) SVfloat (ent, flags) & FL_ONGROUND) {	// just hit ground
			if (hitsound)
				SV_StartSound (ent, 0, "demon/dland2.wav", 255, 1);
		}
	}
	// regular thinking
	SV_RunThink (ent);

	SV_CheckWaterTransition (ent);
}

void
SV_ProgStartFrame (void)
{
	// let the progs know that a new frame has started
	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
	*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
	*sv_globals.time = sv.time;
	PR_ExecuteProgram (&sv_pr_state, sv_funcs.StartFrame);
}

static void
SV_RunEntity (edict_t *ent)
{
	if (sv_fields.lastruntime != -1) {
		if (SVfloat (ent, lastruntime) == (float) sv.time)
			return;
		SVfloat (ent, lastruntime) = (float) sv.time;
	}
	SVdata (ent)->add_grav = false;

	switch ((int) SVfloat (ent, movetype)) {
		case MOVETYPE_PUSH:
			SV_Physics_Pusher (ent);
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
			Sys_Error ("SV_Physics: bad movetype %i",
					   (int) SVfloat (ent, movetype));
	}
}

void
SV_RunNewmis (void)
{
	edict_t    *ent;

	if (sv_fields.lastruntime == -1 || !sv_globals.newmis
		|| !*sv_globals.newmis)
		return;
	ent = PROG_TO_EDICT (&sv_pr_state, *sv_globals.newmis);
	sv_frametime = 0.05;
	*sv_globals.newmis = 0;

	SV_RunEntity (ent);
}

void
SV_Physics (void)
{
	edict_t    *ent;
	int         i;

	SV_ProgStartFrame ();

	// treat each object in turn
	// even the world gets a chance to think
	ent = sv.edicts;
	for (i = 0; i < sv.num_edicts; i++, ent = NEXT_EDICT (&sv_pr_state, ent)) {
		if (ent->free)
			continue;

		if (*sv_globals.force_retouch) {
			SV_LinkEdict (ent, true);	// force retouch even for stationary
		}

		if (i > 0 && i <= svs.maxclients) {
			if (svs.phys_client)
				svs.phys_client (ent, i);
			continue;
		}

		SV_RunEntity (ent);
		SV_RunNewmis ();
	}

	if (*sv_globals.force_retouch)
		(*sv_globals.force_retouch)--;

	if (sv_funcs.EndFrame) {
		// let the progs know that the frame has ended
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv.edicts);
		*sv_globals.time = sv.time;
		PR_ExecuteProgram (&sv_pr_state, sv_funcs.EndFrame);
	}
}
