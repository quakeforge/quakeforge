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

#include "world.h"

#include "nq/include/host.h"
#include "nq/include/server.h"
#include "nq/include/sv_progs.h"

#define sv_frametime host_frametime

// CLIENT MOVEMENT ============================================================

/*
  SV_CheckStuck

  This is a big hack to try and fix the rare case of getting stuck in the world
  clipping hull.
*/
static void
SV_CheckStuck (edict_t *ent)
{
	int         i, j, z;
	vec3_t      org;

	if (!SV_TestEntityPosition (ent)) {
		VectorCopy (SVvector (ent, origin), SVvector (ent, oldorigin));
		return;
	}

	VectorCopy (SVvector (ent, origin), org);
	VectorCopy (SVvector (ent, oldorigin), SVvector (ent, origin));
	if (!SV_TestEntityPosition (ent)) {
		Sys_MaskPrintf (SYS_dev, "Unstuck.\n");
		SV_LinkEdict (ent, true);
		return;
	}

	for (z = 0; z < 18; z++)
		for (i = -1; i <= 1; i++)
			for (j = -1; j <= 1; j++) {
				SVvector (ent, origin)[0] = org[0] + i;
				SVvector (ent, origin)[1] = org[1] + j;
				SVvector (ent, origin)[2] = org[2] + z;
				if (!SV_TestEntityPosition (ent)) {
					Sys_MaskPrintf (SYS_dev, "Unstuck.\n");
					SV_LinkEdict (ent, true);
					return;
				}
			}

	VectorCopy (org, SVvector (ent, origin));
	Sys_MaskPrintf (SYS_dev, "player is stuck.\n");
}

static qboolean
SV_CheckWater (edict_t *ent)
{
	int         cont;
	vec3_t      point;

	point[0] = SVvector (ent, origin)[0];
	point[1] = SVvector (ent, origin)[1];
	point[2] = SVvector (ent, origin)[2] + SVvector (ent, mins)[2] + 1;

	SVfloat (ent, waterlevel) = 0;
	SVfloat (ent, watertype) = CONTENTS_EMPTY;
	cont = SV_PointContents (point);
	if (cont <= CONTENTS_WATER) {
		SVfloat (ent, watertype) = cont;
		SVfloat (ent, waterlevel) = 1;
		point[2] = SVvector (ent, origin)[2] + (SVvector (ent, mins)[2] +
												SVvector (ent, maxs)[2]) * 0.5;
		cont = SV_PointContents (point);
		if (cont <= CONTENTS_WATER) {
			SVfloat (ent, waterlevel) = 2;
			point[2] = SVvector (ent, origin)[2] + SVvector (ent, view_ofs)[2];
			cont = SV_PointContents (point);
			if (cont <= CONTENTS_WATER)
				SVfloat (ent, waterlevel) = 3;
		}
	}

	return SVfloat (ent, waterlevel) > 1;
}

static void
SV_WallFriction (edict_t *ent, trace_t *trace)
{
	float       d, i;
	vec3_t      forward, right, up, into, side;

	AngleVectors (SVvector (ent, v_angle), forward, right, up);
	d = DotProduct (trace->plane.normal, forward);

	d += 0.5;
	if (d >= 0)
		return;

	// cut the tangential velocity
	i = DotProduct (trace->plane.normal, SVvector (ent, velocity));
	VectorScale (trace->plane.normal, i, into);
	VectorSubtract (SVvector (ent, velocity), into, side);

	SVvector (ent, velocity)[0] = side[0] * (1 + d);
	SVvector (ent, velocity)[1] = side[1] * (1 + d);
}

/*
  SV_TryUnstick

  Player has come to a dead stop, possibly due to the problem with limited
  float precision at some angle joins in the BSP hull.

  Try fixing by pushing one pixel in each direction.

  This is a hack, but in the interest of good gameplay...
*/
static int
SV_TryUnstick (edict_t *ent, vec3_t oldvel)
{
	int         i, clip;
	vec3_t      dir, oldorg;
	trace_t     steptrace;

	VectorCopy (SVvector (ent, origin), oldorg);
	VectorZero (dir);

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
		SVvector (ent, velocity)[0] = oldvel[0];
		SVvector (ent, velocity)[1] = oldvel[1];
		SVvector (ent, velocity)[2] = 0;
		clip = SV_FlyMove (ent, 0.1, &steptrace);

		if (fabs (oldorg[1] - SVvector (ent, origin)[1]) > 4
			|| fabs (oldorg[0] - SVvector (ent, origin)[0]) > 4) {
//			Sys_MaskPrintf (SYS_dev, "unstuck!\n");
			return clip;
		}
		// go back to the original pos and try again
		VectorCopy (oldorg, SVvector (ent, origin));
	}

	VectorZero (SVvector (ent, velocity));
	return 7;							// still not moving
}

#define	STEPSIZE	18

/*
  SV_WalkMove

  Used only by players
*/
static void
SV_WalkMove (edict_t *ent)
{
	int         clip, oldonground;
	vec3_t      upmove, downmove, nosteporg, nostepvel, oldorg, oldvel;
	trace_t     steptrace, downtrace;

	// do a regular slide move unless it looks like you ran into a step
	oldonground = (int) SVfloat (ent, flags) & FL_ONGROUND;
	SVfloat (ent, flags) = (int) SVfloat (ent, flags) & ~FL_ONGROUND;

	VectorCopy (SVvector (ent, origin), oldorg);
	VectorCopy (SVvector (ent, velocity), oldvel);

	clip = SV_FlyMove (ent, sv_frametime, &steptrace);

	if (!(clip & 2))
		return;							// move didn't block on a step

	if (!oldonground && SVfloat (ent, waterlevel) == 0)
		return;							// don't stair up while jumping

	if (SVfloat (ent, movetype) != MOVETYPE_WALK)
		return;							// gibbed by a trigger

	if (sv_nostep)
		return;

	if ((int) SVfloat (sv_player, flags) & FL_WATERJUMP)
		return;

	VectorCopy (SVvector (ent, origin), nosteporg);
	VectorCopy (SVvector (ent, velocity), nostepvel);

	// try moving up and forward to go up a step
	VectorCopy (oldorg, SVvector (ent, origin));	// back to start pos
	VectorZero (upmove);
	VectorZero (downmove);
	upmove[2] = STEPSIZE;
	downmove[2] = -STEPSIZE + oldvel[2] * sv_frametime;

	// move up
	SV_PushEntity (ent, upmove);		// FIXME: don't link?

	// move forward
	SVvector (ent, velocity)[0] = oldvel[0];
	SVvector (ent, velocity)[1] = oldvel[1];
	SVvector (ent, velocity)[2] = 0;
	clip = SV_FlyMove (ent, sv_frametime, &steptrace);

	// check for stuckness, possibly due to the limited precision of floats
	// in the clipping hulls
	if (clip) {
		if (fabs (oldorg[1] - SVvector (ent, origin)[1]) < 0.03125
			&& fabs (oldorg[0] - SVvector (ent, origin)[0]) < 0.03125) {
			// stepping up didn't make any progress
			clip = SV_TryUnstick (ent, oldvel);
		}
	}
	// extra friction based on view angle
	if (clip & 2)
		SV_WallFriction (ent, &steptrace);

	// move down
	downtrace = SV_PushEntity (ent, downmove);	// FIXME: don't link?

	if (downtrace.plane.normal[2] > 0.7) {
		if (SV_EntCanSupportJump (ent)) {
			SVfloat (ent, flags) = (int) SVfloat (ent, flags) | FL_ONGROUND;
			SVfloat (ent, groundentity) = EDICT_TO_PROG (&sv_pr_state,
														 downtrace.ent);
		}
	} else {
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		VectorCopy (nosteporg, SVvector (ent, origin));
		VectorCopy (nostepvel, SVvector (ent, velocity));
	}
}

/*
  SV_Physics_Client

  Player character actions
*/
void
SV_Physics_Client (edict_t *ent, int num)
{
	if (!svs.clients[num - 1].active)
		return;							// unconnected slot

	// call standard client pre-think
	*sv_globals.time = sv.time;
	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
	PR_ExecuteProgram (&sv_pr_state, sv_funcs.PlayerPreThink);

	SVdata (ent)->add_grav = false;

	// do a move
	SV_CheckVelocity (ent);

	// decide which move function to call
	switch ((int) SVfloat (ent, movetype)) {
	case MOVETYPE_NONE:
		if (!SV_RunThink (ent))
			return;
		break;

	case MOVETYPE_WALK:
		if (!SV_RunThink (ent))
			return;
		if (!SV_CheckWater (ent)
			&& !((int) SVfloat (ent, flags) & FL_WATERJUMP))
			SV_AddGravity (ent);
		SV_CheckStuck (ent);
		SV_WalkMove (ent);
		break;

	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		SV_Physics_Toss (ent);
		break;

	case MOVETYPE_FLY:
		if (!SV_RunThink (ent))
			return;
		SV_FlyMove (ent, sv_frametime, NULL);
		break;

	case MOVETYPE_NOCLIP:
		if (!SV_RunThink (ent))
			return;
		VectorMultAdd (SVvector (ent, origin), sv_frametime,
					   SVvector (ent, velocity), SVvector (ent, origin));
		break;

	default:
		Sys_Error ("SV_Physics_client: bad movetype %i",
				   (int) SVfloat (ent, movetype));
	}

	SV_LinkEdict (ent, SVfloat (ent, movetype) != MOVETYPE_NOCLIP);

	// call standard player post-think
	*sv_globals.time = sv.time;
	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
	PR_ExecuteProgram (&sv_pr_state, sv_funcs.PlayerPostThink);
}
