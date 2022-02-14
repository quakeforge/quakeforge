/*
	sv_move.c

	monster movement

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

#include "world.h"

#include "nq/include/server.h"
#include "nq/include/sv_progs.h"

#define	STEPSIZE	18

int         c_yes, c_no;


/*
	SV_CheckBottom

	Returns false if any part of the bottom of the entity is off an edge that
	is not a staircase.
*/
qboolean
SV_CheckBottom (edict_t *ent)
{
	float       mid, bottom;
	int         x, y;
	trace_t     trace;
	vec3_t      mins, maxs, start, stop;

	VectorAdd (SVvector (ent, origin), SVvector (ent, mins), mins);
	VectorAdd (SVvector (ent, origin), SVvector (ent, maxs), maxs);

	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks

	// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for (x = 0; x <= 1; x++)
		for (y = 0; y <= 1; y++) {
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if (SV_PointContents (start) != CONTENTS_SOLID)
				goto realcheck;
		}

	c_yes++;
	return true;						// we got out easy

  realcheck:
	c_no++;

	// check it for real...
	start[2] = mins[2];

	// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0]) * 0.5;
	start[1] = stop[1] = (mins[1] + maxs[1]) * 0.5;
	stop[2] = start[2] - 2 * STEPSIZE;
	trace = SV_Move (start, vec3_origin, vec3_origin, stop, true, ent);

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos[2];

	// the corners must be within 16 of the midpoint
	for (x = 0; x <= 1; x++)
		for (y = 0; y <= 1; y++) {
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			trace = SV_Move (start, vec3_origin, vec3_origin, stop, true, ent);

			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > STEPSIZE)
				return false;
		}

	c_yes++;
	return true;
}

/*
	SV_movestep

	Called by monster program code.
	The move will be adjusted for slopes and stairs, but if the move isn't
	possible, no move is done, false is returned, and
	pr_global_struct->trace_normal is set to the normal of the blocking wall
*/
qboolean
SV_movestep (edict_t *ent, const vec3_t move, qboolean relink)
{
	edict_t    *enemy;
	float       dz;
	int         i;
	trace_t     trace;
	vec3_t      oldorg, neworg, end;

	// try the move
	VectorCopy (SVvector (ent, origin), oldorg);
	VectorAdd (SVvector (ent, origin), move, neworg);

	// flying monsters don't step up
	if ((int) SVfloat (ent, flags) & (FL_SWIM | FL_FLY)) {
		// try one move with vertical motion, then one without
		for (i = 0; i < 2; i++) {
			VectorAdd (SVvector (ent, origin), move, neworg);
			enemy = PROG_TO_EDICT (&sv_pr_state, SVentity (ent, enemy));
			if (i == 0 && enemy != sv.edicts) {
				dz = SVvector (ent, origin)[2] - SVvector (enemy, origin)[2];
				if (dz > 40)
					neworg[2] -= 8;
				if (dz < 30)
					neworg[2] += 8;
			}
			trace = SV_Move (SVvector (ent, origin), SVvector (ent, mins),
							 SVvector (ent, maxs), neworg, false, ent);

			if (trace.fraction == 1) {
				if (((int) SVfloat (ent, flags) & FL_SWIM)
					&& SV_PointContents (trace.endpos) == CONTENTS_EMPTY)
					return false;		// swim monster left water

				VectorCopy (trace.endpos, SVvector (ent, origin));
				if (relink)
					SV_LinkEdict (ent, true);
				return true;
			}

			if (enemy == sv.edicts)
				break;
		}

		return false;
	}

	// push down from a step height above the wished position
	neworg[2] += STEPSIZE;
	VectorCopy (neworg, end);
	end[2] -= STEPSIZE * 2;

	trace = SV_Move (neworg, SVvector (ent, mins), SVvector (ent, maxs), end,
					 false, ent);

	if (trace.allsolid)
		return false;

	if (trace.startsolid) {
		neworg[2] -= STEPSIZE;
		trace = SV_Move (neworg, SVvector (ent, mins), SVvector (ent, maxs),
						 end, false, ent);
		if (trace.allsolid || trace.startsolid)
			return false;
	}
	if (trace.fraction == 1) {
		// if monster had the ground pulled out, go ahead and fall
		if ((int) SVfloat (ent, flags) & FL_PARTIALGROUND) {
			VectorAdd (SVvector (ent, origin), move, SVvector (ent, origin));
			if (relink)
				SV_LinkEdict (ent, true);
			SVfloat (ent, flags) = (int) SVfloat (ent, flags) & ~FL_ONGROUND;
			return true;
		}

		return false;					// walked off an edge
	}
	// check point traces down for dangling corners
	VectorCopy (trace.endpos, SVvector (ent, origin));

	if (!SV_CheckBottom (ent)) {
		if ((int) SVfloat (ent, flags) & FL_PARTIALGROUND) {
			// entity had floor mostly pulled out from underneath it and is
			// trying to correct
			if (relink)
				SV_LinkEdict (ent, true);
			return true;
		}
		VectorCopy (oldorg, SVvector (ent, origin));
		return false;
	}

	if ((int) SVfloat (ent, flags) & FL_PARTIALGROUND) {
		SVfloat (ent, flags) = (int) SVfloat (ent, flags) & ~FL_PARTIALGROUND;
	}
	SVentity (ent, groundentity) = EDICT_TO_PROG (&sv_pr_state, trace.ent);

	// the move is ok
	if (relink)
		SV_LinkEdict (ent, true);
	return true;
}

/*
	SV_StepDirection

	Turns to the movement direction, and walks the current distance if
	facing it.
*/
static qboolean
SV_StepDirection (edict_t *ent, float yaw, float dist)
{
	float       delta;
	vec3_t      move, oldorigin;

	SVfloat (ent, ideal_yaw) = yaw;
	PF_changeyaw (&sv_pr_state, 0);

	yaw = yaw * M_PI * 2 / 360;
	move[0] = cos (yaw) * dist;
	move[1] = sin (yaw) * dist;
	move[2] = 0;

	VectorCopy (SVvector (ent, origin), oldorigin);
	if (SV_movestep (ent, move, false)) {
		delta = SVvector (ent, angles)[YAW] - SVfloat (ent, ideal_yaw);
		if (delta > 45 && delta < 315) {	// not turned far enough, so
											// don't take the step
			VectorCopy (oldorigin, SVvector (ent, origin));
		}
		SV_LinkEdict (ent, true);
		return true;
	}
	SV_LinkEdict (ent, true);

	return false;
}

static void
SV_FixCheckBottom (edict_t *ent)
{
	SVfloat (ent, flags) = (int) SVfloat (ent, flags) | FL_PARTIALGROUND;
}

#define	DI_NODIR	-1

static void
SV_NewChaseDir (edict_t *actor, edict_t *enemy, float dist)
{
	float       deltax, deltay, olddir, tdir, turnaround;
	float       d[3];

	olddir = anglemod ((int) (SVfloat (actor, ideal_yaw) / 45) * 45);
	turnaround = anglemod (olddir - 180);

	deltax = SVvector (enemy, origin)[0] - SVvector (actor, origin)[0];
	deltay = SVvector (enemy, origin)[1] - SVvector (actor, origin)[1];
	if (deltax > 10)
		d[1] = 0;
	else if (deltax < -10)
		d[1] = 180;
	else
		d[1] = DI_NODIR;
	if (deltay < -10)
		d[2] = 270;
	else if (deltay > 10)
		d[2] = 90;
	else
		d[2] = DI_NODIR;

	// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR) {
		if (d[1] == 0)
			tdir = d[2] == 90 ? 45 : 315;
		else
			tdir = d[2] == 90 ? 135 : 215;

		if (tdir != turnaround && SV_StepDirection (actor, tdir, dist))
			return;
	}
	// try other directions
	if (((rand () & 3) & 1) || abs (deltay) > abs (deltax)) {
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}

	if (d[1] != DI_NODIR && d[1] != turnaround
		&& SV_StepDirection (actor, d[1], dist)) return;

	if (d[2] != DI_NODIR && d[2] != turnaround
		&& SV_StepDirection (actor, d[2], dist)) return;

	/* there is no direct path to the player, so pick another direction */
	if (olddir != DI_NODIR && SV_StepDirection (actor, olddir, dist))
		return;

	if (rand () & 1) {				// randomly determine direction of search
		for (tdir = 0; tdir <= 315; tdir += 45)
			if (tdir != turnaround && SV_StepDirection (actor, tdir, dist))
				return;
	} else {
		for (tdir = 315; tdir >= 0; tdir -= 45)
			if (tdir != turnaround && SV_StepDirection (actor, tdir, dist))
				return;
	}

	if (turnaround != DI_NODIR && SV_StepDirection (actor, turnaround, dist))
		return;

	SVfloat (actor, ideal_yaw) = olddir;		// can't move

	// if a bridge was pulled out from underneath a monster, it may not have
	// a valid standing position at all
	if (!SV_CheckBottom (actor))
		SV_FixCheckBottom (actor);
}

static qboolean
SV_CloseEnough (edict_t *ent, edict_t *goal, float dist)
{
	int         i;

	for (i = 0; i < 3; i++) {
		if (SVvector (goal, absmin)[i] > SVvector (ent, absmax)[i] + dist)
			return false;
		if (SVvector (goal, absmax)[i] < SVvector (ent, absmin)[i] - dist)
			return false;
	}
	return true;
}

void
SV_MoveToGoal (progs_t *pr, void *data)
{
	edict_t    *ent, *goal;
	float       dist;

	ent = PROG_TO_EDICT (pr, *pr->globals.self);
	goal = PROG_TO_EDICT (pr, SVentity (ent, goalentity));
	dist = P_FLOAT (pr, 0);

	if (!((int) SVfloat (ent, flags) & (FL_ONGROUND | FL_FLY | FL_SWIM))) {
		R_FLOAT (pr) = 0;
		return;
	}
	// if the next step hits the enemy, return immediately
	if (PROG_TO_EDICT (pr, SVentity (ent, enemy)) != sv.edicts
		&& SV_CloseEnough (ent, goal, dist))
		return;

	// bump around...
	if ((rand () & 3) == 1
		|| !SV_StepDirection (ent, SVfloat (ent, ideal_yaw), dist)) {
		SV_NewChaseDir (ent, goal, dist);
	}
}
