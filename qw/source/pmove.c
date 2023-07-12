/*
	pmove.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include "QF/cvar.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

#include "compat.h"

#include "qw/include/client.h"
#include "qw/pmove.h"

int no_pogo_stick;
static cvar_t no_pogo_stick_cvar = {
	.name = "no_pogo_stick",
	.description =
		"disable the ability to pogo stick: 0 pogo allowed, 1 no pogo, 2 pogo "
		"but high friction, 3 high friction and no pogo",
	.default_value = "0",
	.flags = CVAR_SERVERINFO,
	.value = { .type = &cexpr_int, .value = &no_pogo_stick },
};
movevars_t  movevars;

playermove_t pmove;

int         onground;
int         waterlevel;
int         watertype;

float       frametime;

vec3_t      forward, right, up;
vec3_t      player_mins = { -16, -16, -24 };
vec3_t      player_maxs = { 16, 16, 32 };

void
Pmove_Init (void)
{
	PM_InitBoxHull ();
}

void
Pmove_Init_Cvars (void)
{
	Cvar_Register (&no_pogo_stick_cvar, Cvar_Info, &no_pogo_stick);
}

#define	STEPSIZE	18
#define	BUTTON_JUMP	2

/*
	PM_ClipVelocity

	Slide off of the impacting object
	returns the blocked flags (1 = floor, 2 = step / wall)
*/
static int
PM_ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
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

#define	MAX_CLIP_PLANES	5

/*
	PM_FlyMove

	The basic solid body movement clip that slides along multiple planes
*/
static int
PM_FlyMove (void)
{
	float       time_left, d;
	int         blocked, bumpcount, numbumps, numplanes, i, j;
	trace_t     trace;
	vec3_t      dir, end, primal_velocity, original_velocity;
	vec3_t      planes[MAX_CLIP_PLANES];

	numbumps = 4;

	blocked = 0;
	VectorCopy (pmove.velocity, original_velocity);
	VectorCopy (pmove.velocity, primal_velocity);
	numplanes = 0;

	time_left = frametime;

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
		if (VectorIsZero (pmove.velocity))
			break;

		VectorMultAdd (pmove.origin, time_left, pmove.velocity, end);
		if (pmove.add_grav) {
			pmove.add_grav = false;
			end[2] += movevars.entgravity * movevars.gravity *
				frametime * frametime / 2;
		}
		trace = PM_PlayerMove (pmove.origin, end);

		if (trace.startsolid || trace.allsolid) {	// entity is trapped in
													// another solid
			VectorZero (pmove.velocity);
			return 3;
		}

		if (trace.fraction > 0) {		// actually covered some distance
			VectorCopy (trace.endpos, pmove.origin);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break;						// moved the entire distance

		// save entity for contact
		pmove.touchindex[pmove.numtouch] = (physent_t *) trace.ent;
		pmove.numtouch++;

		if (trace.plane.normal[2] > 0.7) {
			blocked |= 1;				// floor
		}
		if (!trace.plane.normal[2]) {
			blocked |= 2;				// step
		}

		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES) {	// this shouldn't really happen
			VectorZero (pmove.velocity);
			break;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i = 0; i < numplanes; i++) {
			PM_ClipVelocity (original_velocity, planes[i], pmove.velocity, 1);
			for (j = 0; j < numplanes; j++)
				if (j != i) {
					if (DotProduct (pmove.velocity, planes[j]) < 0)
						break;			// not ok
				}
			if (j == numplanes)
				break;
		}

		if (i != numplanes) {			// go along this plane
		} else {						// go along the crease
			if (numplanes != 2) {
//				Sys_Printf ("clip velocity, numplanes == %i\n",numplanes);
				VectorZero (pmove.velocity);
				break;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, pmove.velocity);
			VectorScale (dir, d, pmove.velocity);
		}

		// if original velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if (DotProduct (pmove.velocity, primal_velocity) <= 0) {
			VectorZero (pmove.velocity);
			break;
		}
	}

	if (pmove.waterjumptime) {
		VectorCopy (primal_velocity, pmove.velocity);
	}
	return blocked;
}

/*
	PM_FlymodeMove

	Pre-PM_FlyMove function for MOVETYPE_FLY players.  Could have altered
	other physics to fit this in, but that's to easy to screw up.  --KB
*/
static void
PM_FlymodeMove (void)
{
	float       pmspeed;
	trace_t     trace;
	vec3_t      start, dest, pmvel, pmtmp;

	pmvel[0] = forward[0] * pmove.cmd.forwardmove +
		right[0] * pmove.cmd.sidemove;
	pmvel[1] = forward[1] * pmove.cmd.forwardmove +
		right[1] * pmove.cmd.sidemove;
	pmvel[2] = forward[2] * pmove.cmd.forwardmove +
		right[2] * pmove.cmd.sidemove + pmove.cmd.upmove;

	VectorCopy (pmvel, pmtmp);
	pmspeed = VectorNormalize (pmtmp);	// don't alter pmvel

	if (pmspeed > movevars.maxspeed) {	// there IS a spoon, Neo..
		VectorScale (pmvel, movevars.maxspeed / pmspeed, pmvel);
		pmspeed = movevars.maxspeed;
	}
	PM_Accelerate (pmtmp, pmspeed, movevars.wateraccelerate);

	VectorMultAdd (pmove.origin, frametime, pmove.velocity, dest);
	VectorCopy (dest, start);
	start[2] += STEPSIZE + 1;
	trace = PM_PlayerMove (start, dest);
	if (!trace.startsolid && !trace.allsolid) {
		VectorCopy (trace.endpos, pmove.origin);
		return;							// just step up
	}

	PM_FlyMove ();						// NOW we fly.
}

/*
	PM_GroundMove

	Player is on ground, with no upwards velocity
*/
static void
PM_GroundMove (void)
{
	float       downdist, updist;
	trace_t     trace;
	vec3_t      dest;
	vec3_t      original, originalvel, down, up, downvel;

	pmove.velocity[2] = 0;
	if (VectorIsZero (pmove.velocity))
		return;

	// first try just moving to the destination
	dest[0] = pmove.origin[0] + pmove.velocity[0] * frametime;
	dest[1] = pmove.origin[1] + pmove.velocity[1] * frametime;
	dest[2] = pmove.origin[2];

	// first try moving directly to the next spot
	trace = PM_PlayerMove (pmove.origin, dest);
	if (trace.fraction == 1) {
		VectorCopy (trace.endpos, pmove.origin);
		return;
	}
	// try sliding forward both on ground and up 16 pixels
	// take the move that goes farthest
	VectorCopy (pmove.origin, original);
	VectorCopy (pmove.velocity, originalvel);

	// slide move
	PM_FlyMove ();

	VectorCopy (pmove.origin, down);
	VectorCopy (pmove.velocity, downvel);

	VectorCopy (original, pmove.origin);
	VectorCopy (originalvel, pmove.velocity);

	// move up a stair height
	VectorCopy (pmove.origin, dest);
	dest[2] += STEPSIZE;
	trace = PM_PlayerMove (pmove.origin, dest);
	if (!trace.startsolid && !trace.allsolid)
		VectorCopy (trace.endpos, pmove.origin);
	// slide move
	PM_FlyMove ();

	// press down the stepheight
	VectorCopy (pmove.origin, dest);
	dest[2] -= STEPSIZE;
	trace = PM_PlayerMove (pmove.origin, dest);
	if (trace.plane.normal[2] < 0.7)
		goto usedown;
	if (!trace.startsolid && !trace.allsolid)
		VectorCopy (trace.endpos, pmove.origin);
	VectorCopy (pmove.origin, up);

	// decide which one went farther
	downdist = (down[0] - original[0]) * (down[0] - original[0])
		+ (down[1] - original[1]) * (down[1] - original[1]);
	updist = (up[0] - original[0]) * (up[0] - original[0])
		+ (up[1] - original[1]) * (up[1] - original[1]);

	if (downdist > updist) {
	  usedown:
		VectorCopy (down, pmove.origin);
		VectorCopy (downvel, pmove.velocity);
	} else								// copy z value from slide move
		pmove.velocity[2] = downvel[2];

	// if at a dead stop, retry the move with nudges to get around lips
}

/*
	PM_Friction

	Handles both ground friction and water friction
*/
static void
PM_Friction (void)
{
	float       drop, friction, speed, newspeed;
	float      *vel;
	trace_t     trace;
	vec3_t      start, stop;

	if (pmove.waterjumptime)
		return;

	vel = pmove.velocity;

	speed = DotProduct (vel, vel);
	if (speed < 1) {
		vel[0] = 0;
		vel[1] = 0;
		return;
	}

	speed = sqrt (speed);
	friction = movevars.friction;

	// if the leading edge is over a dropoff, increase friction
	if (onground != -1) {
		start[0] = stop[0] = pmove.origin[0] + vel[0] / speed * 16;
		start[1] = stop[1] = pmove.origin[1] + vel[1] / speed * 16;
		start[2] = pmove.origin[2] + player_mins[2];
		stop[2] = start[2] - 34;

		trace = PM_PlayerMove (start, stop);

		if (trace.fraction == 1) {
			friction *= 2;
		}
	}

	drop = 0;

	if (waterlevel >= 2)				// apply water friction
		drop += speed * movevars.waterfriction * waterlevel * frametime;
	else if (pmove.flying)				// apply flying friction
		drop += max (movevars.stopspeed, speed) * friction * frametime;
	else if (onground != -1)			// apply ground friction
		drop += max (movevars.stopspeed, speed) * friction * frametime;

	// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;
	newspeed /= speed;

	VectorScale (vel, newspeed, vel);
}

void
PM_Accelerate (vec3_t wishdir, float wishspeed, float accel)
{
	float       addspeed, accelspeed, currentspeed;
	int         i;

	if (pmove.dead)
		return;
	if (pmove.waterjumptime)
		return;

	currentspeed = DotProduct (pmove.velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = accel * frametime * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		pmove.velocity[i] += accelspeed * wishdir[i];
}

static void
PM_AirAccelerate (vec3_t wishdir, float wishspeed, float accel)
{
	float       addspeed, accelspeed, currentspeed, wishspd = wishspeed;
	int         i;

	if (pmove.dead)
		return;
	if (pmove.waterjumptime)
		return;

	if (wishspd > 30)
		wishspd = 30;
	currentspeed = DotProduct (pmove.velocity, wishdir);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = accel * wishspeed * frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		pmove.velocity[i] += accelspeed * wishdir[i];
}

static void
PM_WaterMove (void)
{
	float       wishspeed;
	int         i;
	trace_t     trace;
	vec3_t      start, dest, wishdir, wishvel;

	// user intentions
	for (i = 0; i < 3; i++)
		wishvel[i] =
			forward[i] * pmove.cmd.forwardmove + right[i] * pmove.cmd.sidemove;

	if (!pmove.cmd.forwardmove && !pmove.cmd.sidemove && !pmove.cmd.upmove)
		wishvel[2] -= 60;				// drift towards bottom
	else
		wishvel[2] += pmove.cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize (wishdir);

	if (wishspeed > movevars.maxspeed) {
		VectorScale (wishvel, movevars.maxspeed / wishspeed, wishvel);
		wishspeed = movevars.maxspeed;
	}
	wishspeed *= 0.7;

	// water acceleration
//	if (pmove.waterjumptime)
//		Sys_Printf ("wm->%f, %f, %f\n", pmove.velocity[0], pmove.velocity[1],
//					pmove.velocity[2]);
	PM_Accelerate (wishdir, wishspeed, movevars.wateraccelerate);

	// assume it is a stair or a slope, so press down from stepheight above
	VectorMultAdd (pmove.origin, frametime, pmove.velocity, dest);
	VectorCopy (dest, start);
	start[2] += STEPSIZE + 1;
	trace = PM_PlayerMove (start, dest);
	if (!trace.startsolid && !trace.allsolid) {	// FIXME: check steep slope?
												// walked up the step
		VectorCopy (trace.endpos, pmove.origin);
		return;
	}

	PM_FlyMove ();
}

static void
PM_AirMove (void)
{
	float       fmove, smove, wishspeed;
	int         i;
	vec3_t      wishdir, wishvel;

	fmove = pmove.cmd.forwardmove;
	smove = pmove.cmd.sidemove;

	forward[2] = 0;
	right[2] = 0;
	VectorNormalize (forward);
	VectorNormalize (right);

	for (i = 0; i < 2; i++)
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize (wishdir);

	// clamp to server defined max speed
	if (wishspeed > movevars.maxspeed) {
		VectorScale (wishvel, movevars.maxspeed / wishspeed, wishvel);
		wishspeed = movevars.maxspeed;
	}
	pmove.add_grav = false;

	if (onground != -1) {
		pmove.velocity[2] = 0;
		PM_Accelerate (wishdir, wishspeed, movevars.accelerate);
		pmove.velocity[2] -= movevars.entgravity * movevars.gravity *
			frametime;
		pmove.add_grav = true;
		PM_GroundMove ();
	} else if (pmove.flying) {
		PM_AirAccelerate (wishdir, wishspeed, movevars.accelerate);
		PM_FlyMove ();
	} else {
		// not on ground, so little effect on velocity
		PM_AirAccelerate (wishdir, wishspeed, movevars.accelerate);

		// add gravity
		pmove.velocity[2] -= movevars.entgravity * movevars.gravity *
			frametime;
		pmove.add_grav = true;

		if (!PM_FlyMove ()) {
			// the move didn't get blocked
			PM_CategorizePosition ();
			if (onground != -1) {			// but we're on ground now
				vec3_t      original;

				// This is a hack to fix the jumping bug
				VectorCopy (pmove.origin, original);
				// Calculate correct velocity
				if (!PM_FlyMove ()) {
					// This shouldn't probably happen (?)
					if (pmove.velocity[2] < 0)
						pmove.velocity[2] = 0;
				}
				VectorCopy (original, pmove.origin);
			}
		}
	}
}

void
PM_CategorizePosition (void)
{
	int         cont;
	trace_t     tr;
	vec3_t      point;

	// if the player hull point one unit down is solid, the player is on ground

	// see if standing on something solid
	point[0] = pmove.origin[0];
	point[1] = pmove.origin[1];
	point[2] = pmove.origin[2] - 1;
	if (pmove.velocity[2] > 180) {
		onground = -1;
	} else {
		tr = PM_PlayerMove (pmove.origin, point);
		if (tr.plane.normal[2] < 0.7 || !tr.ent)
			onground = -1;				// too steep
		else
			onground = (physent_t *) tr.ent - pmove.physents;
		if (onground != -1) {
			pmove.waterjumptime = 0;
			if (!tr.startsolid && !tr.allsolid)
				VectorCopy (tr.endpos, pmove.origin);
		}
		// standing on an entity other than the world
		if (tr.ent && (physent_t *) tr.ent - pmove.physents > 0) {
			pmove.touchindex[pmove.numtouch] = (physent_t *) tr.ent;
			pmove.numtouch++;
		}
	}

	// get waterlevel
	waterlevel = 0;
	watertype = CONTENTS_EMPTY;

	point[2] = pmove.origin[2] + player_mins[2] + 1;
	cont = PM_PointContents (point);

	if (cont <= CONTENTS_WATER) {
		watertype = cont;
		waterlevel = 1;
		point[2] = pmove.origin[2] + (player_mins[2] + player_maxs[2]) * 0.5;
		cont = PM_PointContents (point);
		if (cont <= CONTENTS_WATER) {
			waterlevel = 2;
			point[2] = pmove.origin[2] + 22;
			cont = PM_PointContents (point);
			if (cont <= CONTENTS_WATER)
				waterlevel = 3;
		}
	}
}

static void
JumpButton (void)
{
	if (pmove.dead) {
		pmove.oldbuttons |= BUTTON_JUMP;	// don't jump again until released
		return;
	}

	if (pmove.waterjumptime) {
		pmove.waterjumptime -= frametime;
		if (pmove.waterjumptime < 0)
			pmove.waterjumptime = 0;
		return;
	}

	if (waterlevel >= 2) {				// swimming, not jumping
		onground = -1;

		if (watertype == CONTENTS_WATER)
			pmove.velocity[2] = 100;
		else if (watertype == CONTENTS_SLIME)
			pmove.velocity[2] = 80;
		else
			pmove.velocity[2] = 50;
		return;
	}

	if (onground == -1) {
		if (no_pogo_stick & 1)
			pmove.oldbuttons |= BUTTON_JUMP;	// don't jump again until
												// released
		return;							// in air, so no effect
	}

	if (pmove.oldbuttons & BUTTON_JUMP)
		return;							// don't pogo stick

	onground = -1;
	pmove.velocity[2] += 270;

	pmove.oldbuttons |= BUTTON_JUMP;	// don't jump again until released
}

static void
CheckWaterJump (void)
{
	int         cont;
	vec3_t      flatforward, spot;

	if (pmove.waterjumptime || pmove.flying)
		return;

	// ZOID, don't hop out if we just jumped in
	if (pmove.velocity[2] < -180)
		return;							// hop out only if we are moving up

	// see if near an edge
	flatforward[0] = forward[0];
	flatforward[1] = forward[1];
	flatforward[2] = 0;
	VectorNormalize (flatforward);

	VectorMultAdd (pmove.origin, 24, flatforward, spot);
	spot[2] += 8;
	cont = PM_PointContents (spot);
	if (cont != CONTENTS_SOLID)
		return;
	spot[2] += 24;
	cont = PM_PointContents (spot);
	if (cont != CONTENTS_EMPTY)
		return;
	// jump out of water
	VectorScale (flatforward, 50, pmove.velocity);
	pmove.velocity[2] = 310;
	pmove.waterjumptime = 2;			// safety net
	pmove.oldbuttons |= BUTTON_JUMP;	// don't jump again until released
}

/*
	NudgePosition

	If pmove.origin is in a solid position,
	try nudging slightly on all axis to
	allow for the cut precision of the net coordinates
*/
static void
NudgePosition (void)
{
	int         i, x, y, z;
	static int  sign[3] = { 0, -1, 1 };
	vec3_t      base;

	VectorCopy (pmove.origin, base);

	for (i = 0; i < 3; i++)
		pmove.origin[i] = ((int) (pmove.origin[i] * 8)) * 0.125;
//	pmove.origin[2] += 0.124;

//	if (pmove.dead)
//		return;     // might be a squished point, so don'y bother
//	if (PM_TestPlayerPosition (pmove.origin) )
//		return;

	for (z = 0; z <= 2; z++) {
		for (x = 0; x <= 2; x++) {
			for (y = 0; y <= 2; y++) {
				pmove.origin[0] = base[0] + (sign[x] * 1.0 / 8);
				pmove.origin[1] = base[1] + (sign[y] * 1.0 / 8);
				pmove.origin[2] = base[2] + (sign[z] * 1.0 / 8);
				if (PM_TestPlayerPosition (pmove.origin))
					return;
			}
		}
	}
	VectorCopy (base, pmove.origin);
//	Sys_MaskPrintf (SYS_dev, "NudgePosition: stuck\n");
}

static void
SpectatorMove (void)
{
	float       control, drop, friction, fmove, smove, speed, newspeed;
	float       currentspeed, addspeed, accelspeed, wishspeed;
	int         i;
	vec3_t      wishdir, wishvel;

	// friction
	speed = DotProduct (pmove.velocity, pmove.velocity);
	if (speed < 1) {
		VectorZero (pmove.velocity);
	} else {
		speed = sqrt (speed);
		drop = 0;

		friction = movevars.friction * 1.5;					// extra friction
		control = speed < movevars.stopspeed ? movevars.stopspeed : speed;
		drop += control * friction * frametime;

		// scale the velocity
		newspeed = speed - drop;
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;

		VectorScale (pmove.velocity, newspeed, pmove.velocity);
	}

	// accelerate
	fmove = pmove.cmd.forwardmove;
	smove = pmove.cmd.sidemove;

	VectorNormalize (forward);
	VectorNormalize (right);

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	wishvel[2] += pmove.cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize (wishdir);

	// clamp to server defined max speed
	if (wishspeed > movevars.spectatormaxspeed) {
		VectorScale (wishvel, movevars.spectatormaxspeed / wishspeed, wishvel);
		wishspeed = movevars.spectatormaxspeed;
	}

	currentspeed = DotProduct (pmove.velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = movevars.accelerate * frametime * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		pmove.velocity[i] += accelspeed * wishdir[i];

	// move
	VectorMultAdd (pmove.origin, frametime, pmove.velocity, pmove.origin);
}

/*
	PlayerMove

	Returns with origin, angles, and velocity modified in place.

	Numtouch and touchindex[] will be set if any of the physents
	were contacted during the move.
*/
void
PlayerMove (void)
{
	frametime = pmove.cmd.msec * 0.001;
	pmove.numtouch = 0;

	AngleVectors (pmove.angles, forward, right, up);

	if (pmove.spectator) {
		SpectatorMove ();
		return;
	}

	NudgePosition ();

	// take angles directly from command
	VectorCopy (pmove.cmd.angles, pmove.angles);

	// set onground, watertype, and waterlevel
	PM_CategorizePosition ();

	if (((pmove.cmd.buttons & BUTTON_JUMP) || (no_pogo_stick & 1))
		&& onground != -1 && pmove.oldonground == -1	// just landed
		&& (no_pogo_stick & 2)) {
		float save = movevars.friction;

		pmove.waterjumptime = 0;
		movevars.friction *= 3;
		PM_Friction ();
		movevars.friction = save;
	}
	pmove.oldonground = onground;

	if (waterlevel == 2)
		CheckWaterJump ();

	if (pmove.velocity[2] < 0)
		pmove.waterjumptime = 0;

	if (pmove.cmd.buttons & BUTTON_JUMP)
		JumpButton ();
	else
		pmove.oldbuttons &= ~BUTTON_JUMP;

	PM_Friction ();

	if (waterlevel >= 2)
		PM_WaterMove ();
	else if (pmove.flying)
		PM_FlymodeMove ();
	else
		PM_AirMove ();

	// set onground, watertype, and waterlevel for final spot
	PM_CategorizePosition ();
}
