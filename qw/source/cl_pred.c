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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/keys.h"

#include "compat.h"

#include "qw/bothdefs.h"
#include "qw/include/cl_ents.h"
#include "qw/include/cl_pred.h"
#include "qw/include/client.h"
#include "qw/pmove.h"

int cl_predict;
static cvar_t cl_predict_cvar = {
	.name = "cl_predict",
	.description =
		"Set to enable client prediction",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_predict },
};
float cl_pushlatency;
static cvar_t cl_pushlatency_cvar = {
	.name = "pushlatency",
	.description =
		"How much prediction should the client make",
	.default_value = "-999",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &cl_pushlatency },
};


void
CL_PredictUsercmd (player_state_t *from, player_state_t *to, usercmd_t *u,
				   bool clientplayer)
{
	if (!clientplayer) {
		if (VectorIsZero (from->pls.es.velocity)) {
			VectorCopy (from->pls.es.origin, to->pls.es.origin);
			VectorCopy (u->angles, to->viewangles);
			VectorCopy (from->pls.es.velocity, to->pls.es.velocity);
			return;
		}
	}

	// split up very long moves
	if (u->msec > 50) {
		player_state_t temp;
		usercmd_t   split;

		split = *u;
		split.msec /= 2;

		CL_PredictUsercmd (from, &temp, &split, clientplayer);
		CL_PredictUsercmd (&temp, to, &split, clientplayer);
		return;
	}

	VectorCopy (from->pls.es.origin, pmove.origin);
	VectorCopy (u->angles, pmove.angles);
	VectorCopy (from->pls.es.velocity, pmove.velocity);

	pmove.oldbuttons = from->oldbuttons;
	pmove.oldonground = from->oldonground;
	pmove.waterjumptime = from->waterjumptime;
	pmove.dead = cl.stats[STAT_HEALTH] <= 0;
	if (clientplayer)
		pmove.spectator = cl.spectator;
	else
		pmove.spectator = false;
	pmove.flying = cl.stats[STAT_FLYMODE];

	pmove.cmd = *u;

	PlayerMove ();
	to->waterjumptime = pmove.waterjumptime;
	to->oldbuttons = pmove.oldbuttons;	// Tonik
	to->oldonground = pmove.oldonground;
	VectorCopy (pmove.origin, to->pls.es.origin);
	VectorCopy (pmove.angles, to->viewangles);
	VectorCopy (pmove.velocity, to->pls.es.velocity);
	to->onground = onground;
	to->pls.es.weaponframe = from->pls.es.weaponframe;
}

void
CL_PredictMove (void)
{
	float       f;
	int         oldphysent, i;
	frame_t    *from, *to = NULL;
	entity_state_t *fromes;
	entity_state_t *toes;

	if (cl_pushlatency > 0)
		Cvar_Set ("pushlatency", "0");

	if (cl.paused)
		return;

	// assume on ground unless prediction says different
	cl.viewstate.onground = 0;

	cl.time = realtime - cls.latency - cl_pushlatency * 0.001;
	if (cl.time > realtime)
		cl.time = realtime;
	cl.viewstate.time = cl.time;

	if (cl.intermission) {
		return;
	}

	if (!cl.validsequence)
		return;

	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >=
		UPDATE_BACKUP - 1)
		return;

	//VectorCopy (cl.viewstate.angles, cl.viewstate.angles);
	cl.viewstate.player_angles[ROLL] = 0;						// FIXME @@@

	// this is the last frame received from the server
	from = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];
	fromes = &from->playerstate[cl.playernum].pls.es;

	if (!cl_predict) {
		cl.viewstate.velocity = fromes->velocity;
		cl.viewstate.player_origin = fromes->origin;
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
						   true);
		cl.viewstate.onground = onground;
		if (to->senttime >= cl.time)
			break;
		from = to;
	}

	pmove.numphysent = oldphysent;

	if (i == UPDATE_BACKUP - 1 || !to)
		return;							// net hasn't deliver packets in a
										// long time...
	toes = &to->playerstate[cl.playernum].pls.es;

	// now interpolate some fraction of the final frame
	if (to->senttime == from->senttime)
		f = 0;
	else {
		f = (cl.time - from->senttime) / (to->senttime - from->senttime);
		f = bound (0, f, 1);
	}

	for (i = 0; i < 3; i++)
		if (fabs (fromes->origin[i] - toes->origin[i]) > 128) {
			// teleported, so don't lerp
			cl.viewstate.velocity = toes->velocity;
			cl.viewstate.player_origin = toes->origin;
			return;
		}

	cl.viewstate.player_origin = fromes->origin
		+ f * (toes->origin - fromes->origin);
}

void
CL_Prediction_Init_Cvars (void)
{
	Cvar_Register (&cl_predict_cvar, 0, 0);
	Cvar_Register (&cl_pushlatency_cvar, 0, 0);
}
