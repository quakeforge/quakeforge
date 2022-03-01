/*
	cl_cam.c

	Player camera tracking in Spectator mode

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
/*
  ZOID - This takes over player controls for spectator automatic camera.
  Player moves as a spectator, but the camera tracks an enemy player
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <math.h>

#include "QF/cvar.h"
#include "QF/msg.h"

#include "compat.h"
#include "sbar.h"

#include "qw/include/cl_cam.h"
#include "qw/include/cl_input.h"
#include "qw/include/client.h"
#include "qw/pmove.h"

#define	PM_SPECTATORMAXSPEED 500
#define	PM_STOPSPEED 100
#define	PM_MAXSPEED 320
#define BUTTON_JUMP 2
#define BUTTON_ATTACK 1
#define MAX_ANGLE_TURN 10

#include "QF/sys.h"
#include "QF/keys.h"
#include "QF/input.h"
#include "QF/mathlib.h"
#include "world.h"

cvar_t     *cl_hightrack;	// track high fragger
cvar_t     *cl_chasecam;
cvar_t     *cl_camera_maxpitch;
cvar_t     *cl_camera_maxyaw;

static vec3_t desired_position;			// where the camera wants to be
static qboolean locked = false;
static int  oldbuttons;

double      cam_lastviewtime;
qboolean    cam_forceview;
vec3_t      cam_viewangles;

int         spec_track = 0;				// player# of who we are tracking
int         ideal_track = 0;
float       last_lock = 0;
int         autocam = CAM_NONE;


static void
vectoangles (vec3_t vec, vec3_t ang)
{
	float		forward, pitch, yaw;

	if (vec[1] == 0 && vec[0] == 0) {
		yaw = 0;
		if (vec[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	} else {
		yaw = (int) (atan2 (vec[1], vec[0]) * (180.0 / M_PI));
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (vec[0] * vec[0] + vec[1] * vec[1]);
		pitch = (int) (atan2 (vec[2], forward) * (180.0 / M_PI));
		if (pitch < 0)
			pitch += 360;
	}

	ang[0] = pitch;
	ang[1] = yaw;
	ang[2] = 0;
}

// returns true if weapon model should be drawn in camera mode
qboolean
Cam_DrawViewModel (void)
{
	if (cl.viewstate.chase && chase_active->int_val)
		return false;

	if (!cl.spectator)
		return true;

	if (autocam && locked && cl_chasecam->int_val)
		return true;
	return false;
}

// returns true if we should draw this player, we don't if we are chase camming
qboolean
Cam_DrawPlayer (int playernum)
{
	if (playernum == cl.playernum) {						// client player
		if (cl.viewstate.chase == 0 || chase_active->int_val == 0)
			return false;
		if (!cl.spectator)
			return true;
	} else {
		if (!cl_chasecam->int_val)
			return true;
		if (cl.spectator && autocam && locked && spec_track == playernum)
			return false;
		if (cl.viewstate.chase == 0 || chase_active->int_val == 0)
			return true;
	}
	return false;
}

int
Cam_TrackNum (void)
{
	if (!autocam)
		return -1;
	return spec_track;
}

static void
Cam_Unlock (void)
{
	if (autocam) {
		if (!cls.demoplayback) {
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			MSG_WriteString (&cls.netchan.message, "ptrack");
		}
		autocam = CAM_NONE;
		locked = false;
		Sbar_Changed ();
	}
}

void
Cam_Lock (int playernum)
{
	char		st[40];

	snprintf (st, sizeof (st), "ptrack %i", playernum);
	if (cls.demoplayback2) {
		memcpy (cl.stats, cl.players[playernum].stats, sizeof (cl.stats));
	}

	if (!cls.demoplayback) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, st);
	}
	spec_track = playernum;
	last_lock = realtime;
	cam_forceview = true;
	locked = false;
	Sbar_Changed ();
}

static trace_t
Cam_DoTrace (vec3_t vec1, vec3_t vec2)
{
#if 0
	memset (&pmove, 0, sizeof (pmove));

	pmove.numphysent = 1;
	VectorZero (pmove.physents[0].origin);
	pmove.physents[0].model = cl.worldmodel;
#endif

	VectorCopy (vec1, pmove.origin);
	return PM_PlayerMove (pmove.origin, vec2);
}

// Returns distance or 9999 if invalid for some reason
static float
Cam_TryFlyby (player_state_t * self, player_state_t * player, vec3_t vec,
			  qboolean checkvis)
{
	float       len;
	trace_t     trace;
	vec3_t      v;

	vectoangles (vec, v);
	VectorCopy (v, pmove.angles);
	VectorNormalize (vec);
	VectorMultAdd (player->pls.es.origin, 800, vec, v);
	// v is endpos
	// fake a player move
	trace = Cam_DoTrace (&player->pls.es.origin[0], v);//FIXME
	if ( /* trace.inopen || */ trace.inwater)
		return 9999;
	VectorCopy (trace.endpos, vec);
	len = VectorDistance (trace.endpos, player->pls.es.origin);

	if (len < 32 || len > 800)
		return 9999;
	if (checkvis) {
		trace = Cam_DoTrace (&self->pls.es.origin[0], vec);//FIXME
		if (trace.fraction != 1 || trace.inwater)
			return 9999;

		len = VectorDistance (trace.endpos, self->pls.es.origin);
	}

	return len;
}

// Is player visible?
static qboolean
Cam_IsVisible (player_state_t *player, vec3_t vec)
{
	float       d;
	trace_t     trace;
	vec3_t      v;

	trace = Cam_DoTrace (&player->pls.es.origin[0], vec);//FIXME
	if (trace.fraction != 1 || /* trace.inopen || */ trace.inwater)
		return false;
	// check distance, don't let the player get too far away or too close
	VectorSubtract (player->pls.es.origin, vec, v);
	d = VectorLength (v);

	return (d > 16.0);
}

static qboolean
InitFlyby (player_state_t * self, player_state_t * player, int checkvis)
{
	float       f, max;
	vec3_t      forward, right, up, vec, vec2;

	VectorCopy (player->viewangles, vec);
	vec[0] = 0;
	AngleVectors (vec, forward, right, up);
//	for (i = 0; i < 3; i++)
//		forward[i] *= 3;

	max = 1000;
	VectorAdd (forward, up, vec2);
	VectorAdd (vec2, right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorAdd (forward, up, vec2);
	VectorSubtract (vec2, right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorAdd (forward, right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorSubtract (forward, right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorAdd (forward, up, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorSubtract (forward, up, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorAdd (up, right, vec2);
	VectorSubtract (vec2, forward, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorSubtract (up, right, vec2);
	VectorSubtract (vec2, forward, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	// invert
	VectorNegate (forward, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorCopy (forward, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	// invert
	VectorNegate (right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorCopy (right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	// ack, can't find him
	if (max >= 1000) {
//		Cam_Unlock ();
		return false;
	}
	locked = true;
	VectorCopy (vec, desired_position);
	return true;
}

static void
Cam_CheckHighTarget (void)
{
	int				i, j, max;
	player_info_t  *s;

	j = -1;
	for (i = 0, max = -9999; i < MAX_CLIENTS; i++) {
		s = &cl.players[i];
		if (s->name && s->name->value[0] && !s->spectator && s->frags > max) {
			max = s->frags;
			j = i;
		}
	}
	if (j >= 0) {
		if (!locked || cl.players[j].frags > cl.players[spec_track].frags) {
			Cam_Lock (j);
			ideal_track = spec_track;
		}
	} else
		Cam_Unlock ();
}

// ZOID
//
// Take over the user controls and track a player.
// We find a nice position to watch the player and move there
void
Cam_Track (usercmd_t *cmd)
{
	float			len;
	frame_t		   *frame;
	player_state_t *player, *self;
	vec3_t			vec;

	if (!cl.spectator)
		return;

	if (cl_hightrack->int_val && !locked)
		Cam_CheckHighTarget ();

	if (!autocam || cls.state != ca_active)
		return;

	if (locked
		&& (!cl.players[spec_track].name->value[0]
			|| cl.players[spec_track].spectator)) {
		locked = false;
		if (cl_hightrack->int_val)
			Cam_CheckHighTarget ();
		else
			Cam_Unlock ();
		return;
	}

	frame = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];
	if (autocam && cls.demoplayback2 && 0) {
		if (ideal_track != spec_track && realtime - last_lock > 1
			&& frame->playerstate[ideal_track].messagenum == cl.parsecount)
			Cam_Lock (ideal_track);

		if (frame->playerstate[spec_track].messagenum != cl.parsecount) {
			int         i;

			for (i = 0; i < MAX_CLIENTS; i++) {
				if (frame->playerstate[i].messagenum == cl.parsecount)
					break;
			}
			if (i < MAX_CLIENTS)
				Cam_Lock (i);
		}
	}

	player = frame->playerstate + spec_track;
	self = frame->playerstate + cl.playernum;

	if (!locked || !Cam_IsVisible (player, desired_position)) {
		if (!locked || realtime - cam_lastviewtime > 0.1) {
			if (!InitFlyby (self, player, true))
				InitFlyby (self, player, false);
			cam_lastviewtime = realtime;
		}
	} else
		cam_lastviewtime = realtime;

	// couldn't track for some reason
	if (!locked || !autocam)
		return;

	if (cl_chasecam->int_val) {
		cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;

		VectorCopy (player->viewangles, cl.viewstate.player_angles);
		VectorCopy (player->pls.es.origin, desired_position);
		if (memcmp (&desired_position, &self->pls.es.origin,
					sizeof (desired_position)) != 0) {
			if (!cls.demoplayback) {
				MSG_WriteByte (&cls.netchan.message, clc_tmove);
				MSG_WriteCoordV (&cls.netchan.message, desired_position);
			}
			// move there locally immediately
			VectorCopy (desired_position, self->pls.es.origin);
		}
		self->pls.es.weaponframe = player->pls.es.weaponframe;

	} else {
		// Ok, move to our desired position and set our angles to view
		// the player
		VectorSubtract (desired_position, self->pls.es.origin, vec);
		len = VectorLength (vec);
		cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;
		if (len > 16) {					// close enough?
			if (!cls.demoplayback) {
				MSG_WriteByte (&cls.netchan.message, clc_tmove);
				MSG_WriteCoordV (&cls.netchan.message, desired_position);
			}
		}
		// move there locally immediately
		VectorCopy (desired_position, self->pls.es.origin);

		VectorSubtract (player->pls.es.origin, desired_position, vec);
		vectoangles (vec, cl.viewstate.player_angles);
		cl.viewstate.player_angles[0] = -cl.viewstate.player_angles[0];
	}
}

#if 0
static float
adjustang (float current, float ideal, float speed)
{
	float		move;

	current = anglemod (current);
	ideal = anglemod (ideal);

	if (current == ideal)
		return current;

	move = ideal - current;
	if (ideal > current) {
		if (move >= 180)
			move = move - 360;
	} else {
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0) {
		if (move > speed)
			move = speed;
	} else {
		if (move < -speed)
			move = -speed;
	}

	return anglemod (current + move);
}
#endif

#if 0
void
Cam_SetView (void)
{
	frame_t		   *frame;
	player_state_t *player, *self;
	vec3_t			vec, vec2;

	if (cls.state != ca_active || !cl.spectator || !autocam || !locked)
		return;

	frame = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];
	player = frame->playerstate + spec_track;
	self = frame->playerstate + cl.playernum;

	VectorSubtract (player->pls.es.origin, cl.simorg, vec);
	if (cam_forceview) {
		cam_forceview = false;
		vectoangles (vec, cam_viewangles);
		cam_viewangles[0] = -cam_viewangles[0];
	} else {
		vectoangles (vec, vec2);
		vec2[PITCH] = -vec2[PITCH];

		cam_viewangles[PITCH] =
			adjustang (cam_viewangles[PITCH], vec2[PITCH],
					   cl_camera_maxpitch->value);
		cam_viewangles[YAW] =
			adjustang (cam_viewangles[YAW], vec2[YAW],
					   cl_camera_maxyaw->value);
	}
	VectorCopy (cam_viewangles, cl.viewstate.player_angles);
	VectorCopy (cl.viewstate.player_angles, cl.simangles);
	cl.simangles[ROLL] = 0;						// FIXME @@@
}
#endif

void
Cam_FinishMove (usercmd_t *cmd)
{
	int				end, i;
	player_info_t  *s;

	if (cls.state != ca_active)
		return;

	if (!cl.spectator)					// only in spectator mode
		return;

#if 0
	if (autocam && locked) {
		frame = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];
		player = frame->playerstate + spec_track;
		self = frame->playerstate + cl.playernum;

		VectorSubtract (player->pls.es.origin, self->pls.es.origin, vec);
		if (cam_forceview) {
			cam_forceview = false;
			vectoangles (vec, cam_viewangles);
			cam_viewangles[0] = -cam_viewangles[0];
		} else {
			vectoangles (vec, vec2);
			vec2[PITCH] = -vec2[PITCH];

			cam_viewangles[PITCH] =
				adjustang (cam_viewangles[PITCH], vec2[PITCH],
						   cl_camera_maxpitch->value);
			cam_viewangles[YAW] =
				adjustang (cam_viewangles[YAW], vec2[YAW],
						   cl_camera_maxyaw->value);
		}
		VectorCopy (cam_viewangles, cl.viewstate.player_angles);
	}
#endif

	if (cmd->buttons & BUTTON_ATTACK) {
		if (!(oldbuttons & BUTTON_ATTACK)) {
			oldbuttons |= BUTTON_ATTACK;
			autocam++;

			if (autocam > CAM_TRACK) {
				Cam_Unlock ();
				VectorCopy (cl.viewstate.player_angles, cmd->angles);
				return;
			}
		} else
			return;
	} else {
		oldbuttons &= ~BUTTON_ATTACK;
		if (!autocam)
			return;
	}

	if (autocam && cl_hightrack->int_val) {
		Cam_CheckHighTarget ();
		return;
	}

	if (locked) {
		if ((cmd->buttons & BUTTON_JUMP) && (oldbuttons & BUTTON_JUMP))
			return;						// don't pogo stick

		if (!(cmd->buttons & BUTTON_JUMP)) {
			oldbuttons &= ~BUTTON_JUMP;
			return;
		}
		oldbuttons |= BUTTON_JUMP;		// don't jump again until released
	}
//	Sys_Printf ("Selecting track target...\n");

	if (locked && autocam)
		end = (spec_track + 1) % MAX_CLIENTS;
	else
		end = spec_track;
	i = end;
	do {
		s = &cl.players[i];
		if (s->name && s->name->value[0] && !s->spectator) {
			Cam_Lock (i);
			ideal_track = i;
			return;
		}
		i = (i + 1) % MAX_CLIENTS;
	} while (i != end);
	// stay on same guy?
	i = spec_track;
	s = &cl.players[i];
	if (s->name && s->name->value[0] && !s->spectator) {
		Cam_Lock (i);
		ideal_track = i;
		return;
	}
	Sys_Printf ("No target found ...\n");
	autocam = locked = false;
}

void
Cam_Reset (void)
{
	autocam = CAM_NONE;
	spec_track = 0;
	ideal_track = 0;
}

void
CL_Cam_Init_Cvars (void)
{
	cl_camera_maxpitch = Cvar_Get ("cl_camera_maxpitch", "10", CVAR_NONE, NULL,
								   "highest camera pitch in spectator mode");
	cl_camera_maxyaw = Cvar_Get ("cl_camera_maxyaw", "30", CVAR_NONE, NULL,
								 "highest camera yaw in spectator mode");
	cl_chasecam = Cvar_Get ("cl_chasecam", "0", CVAR_NONE, NULL, "get first "
							"person view of the person you are tracking in "
							"spectator mode");
	cl_hightrack = Cvar_Get ("cl_hightrack", "0", CVAR_NONE, NULL, "view the "
							 "player who has the most frags while you are in "
							 "spectator mode.");
}
