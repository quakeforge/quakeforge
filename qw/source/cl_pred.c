/*
	cl_pred.c

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/keys.h"

#include "bothdefs.h"
#include "compat.h"
#include "cl_ents.h"
#include "client.h"
#include "pmove.h"

cvar_t     *cl_nopred;
cvar_t     *cl_pushlatency;
cvar_t     *cl_nostatpred;



void
CL_PredictUsercmd (player_state_t * from, player_state_t * to, usercmd_t *u,
				   qboolean spectator)
{
// Dabb: if there is no movement to start with, don't predict...
	if(cl_nostatpred->int_val && VectorIsZero (from->velocity)) {
		VectorCopy (from->origin, to->origin);
		VectorCopy (u->angles, to->viewangles);
		VectorCopy (from->velocity, to->velocity);
		return;
	}

	// split up very long moves
	if (u->msec > 50) {
		player_state_t temp;
		usercmd_t   split;

		split = *u;
		split.msec /= 2;

		CL_PredictUsercmd (from, &temp, &split, spectator);
		CL_PredictUsercmd (&temp, to, &split, spectator);
		return;
	}

	VectorCopy (from->origin, pmove.origin);
//	VectorCopy (from->viewangles, pmove.angles);
	VectorCopy (u->angles, pmove.angles);
	VectorCopy (from->velocity, pmove.velocity);

	pmove.oldbuttons = from->oldbuttons;
	pmove.waterjumptime = from->waterjumptime;
	pmove.dead = cl.stats[STAT_HEALTH] <= 0;
	pmove.spectator = spectator;
	pmove.flying = cl.stats[STAT_FLYMODE];

	pmove.cmd = *u;

	PlayerMove ();
//	for (i=0 ; i<3 ; i++)
//		pmove.origin[i] = ((int)(pmove.origin[i] * 8)) * 0.125;
	to->waterjumptime = pmove.waterjumptime;
	to->oldbuttons = pmove.oldbuttons;	// Tonik
//	to->oldbuttons = pmove.cmd.buttons;
	VectorCopy (pmove.origin, to->origin);
	VectorCopy (pmove.angles, to->viewangles);
	VectorCopy (pmove.velocity, to->velocity);
	to->onground = onground;
	to->weaponframe = from->weaponframe;
}

void
CL_PredictMove (void)
{
	float       f;
	int         oldphysent, i;
	frame_t    *from, *to = NULL;

	if (cl_pushlatency->value > 0)
		Cvar_Set (cl_pushlatency, "0");

	if (cl.paused)
		return;

	// assume on ground unless prediction says different
	cl.onground = 0;

	cl.time = realtime - cls.latency - cl_pushlatency->value * 0.001;
	if (cl.time > realtime)
		cl.time = realtime;

	if (cl.intermission)
		return;

	if (!cl.validsequence)
		return;

	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >=
		UPDATE_BACKUP - 1)
		return;

	VectorCopy (cl.viewangles, cl.simangles);

	// this is the last frame received from the server
	from = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];

	// we can now render a frame
	if (cls.state == ca_onserver) {
		// first update is the final signon stage
		VID_SetCaption (cls.servername);
		CL_SetState (ca_active);
	}

	if (cl_nopred->int_val) {
		VectorCopy (from->playerstate[cl.playernum].velocity, cl.simvel);
		VectorCopy (from->playerstate[cl.playernum].origin, cl.simorg);
		return;
	}

	// Dabb: if there is no movement to start with, don't predict...
	if (cl_nostatpred->int_val
		&& VectorIsZero (from->playerstate[cl.playernum].velocity)) {
		VectorCopy (from->playerstate[cl.playernum].velocity, cl.simvel);
		VectorCopy (from->playerstate[cl.playernum].origin, cl.simorg);
		return;
	}

	// predict forward until cl.time <= to->senttime
	oldphysent = pmove.numphysent;
	CL_SetSolidPlayers (cl.playernum);

//	to = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];

	for (i = 1; i < UPDATE_BACKUP - 1 && cls.netchan.incoming_sequence + i <
		 cls.netchan.outgoing_sequence; i++) {
		to = &cl.frames[(cls.netchan.incoming_sequence + i) & UPDATE_MASK];
		CL_PredictUsercmd (&from->playerstate[cl.playernum],
						   &to->playerstate[cl.playernum], &to->cmd,
						   cl.spectator);
		cl.onground = onground;
		if (to->senttime >= cl.time)
			break;
		from = to;
	}

	pmove.numphysent = oldphysent;

	if (i == UPDATE_BACKUP - 1 || !to)
		return;							// net hasn't deliver packets in a
										// long time...

	// now interpolate some fraction of the final frame
	if (to->senttime == from->senttime)
		f = 0;
	else
		f = bound(0, (cl.time - from->senttime) / (to->senttime - from->senttime), 1);

	for (i = 0; i < 3; i++)
		if (fabs (from->playerstate[cl.playernum].origin[i] -
				  to->playerstate[cl.playernum].origin[i]) > 128) {
			// teleported, so don't lerp
			VectorCopy (to->playerstate[cl.playernum].velocity, cl.simvel);
			VectorCopy (to->playerstate[cl.playernum].origin, cl.simorg);
			return;
		}

	for (i = 0; i < 3; i++) {
		cl.simorg[i] = from->playerstate[cl.playernum].origin[i]
			+ f * (to->playerstate[cl.playernum].origin[i] -
				   from->playerstate[cl.playernum].origin[i]);
		cl.simvel[i] = from->playerstate[cl.playernum].velocity[i]
			+ f * (to->playerstate[cl.playernum].velocity[i] -
				   from->playerstate[cl.playernum].velocity[i]);
	}
}

void
CL_Prediction_Init_Cvars (void)
{
	cl_nopred = Cvar_Get ("cl_nopred", "0", CVAR_NONE, NULL,
						  "Set to turn off client prediction");
	cl_nostatpred = Cvar_Get ("cl_nostatpred", "0", CVAR_NONE, NULL,
							  "Set to turn off static player prediction");
	cl_pushlatency = Cvar_Get ("pushlatency", "-999", CVAR_NONE, NULL,
							   "How much prediction should the client make");
}
