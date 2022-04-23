/*
	sv_user.c

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/sys.h"

#include "world.h"

#include "nq/include/host.h"
#include "nq/include/server.h"
#include "nq/include/sv_progs.h"

int sv_nostep;
static cvar_t sv_nostep_cvar = {
	.name = "sv_nostep",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_nostep },
};

float sv_rollangle;
static cvar_t sv_rollangle_cvar = {
	.name = "cl_rollangle",
	.description =
		"How much your screen tilts when strafing",
	.default_value = "2.0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sv_rollangle },
};
float sv_rollspeed;
static cvar_t sv_rollspeed_cvar = {
	.name = "cl_rollspeed",
	.description =
		"How quickly you straighten out after strafing",
	.default_value = "200",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sv_rollspeed },
};

edict_t    *sv_player;

float sv_edgefriction;
static cvar_t sv_edgefriction_cvar = {
	.name = "edgefriction",
	.description =
		"None",
	.default_value = "2",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sv_edgefriction },
};

vec3_t      forward, right, up;

vec3_t      wishdir;
float       wishspeed;

// world
float      *angles;
float      *origin;
float      *velocity;

qboolean    onground;

usercmd_t   cmd;

float sv_idealpitchscale;
static cvar_t sv_idealpitchscale_cvar = {
	.name = "sv_idealpitchscale",
	.description =
		"None",
	.default_value = "0.8",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sv_idealpitchscale },
};

#define	MAX_FORWARD	6

/*
   FIXME duplicates V_CalcRoll in cl_view.c
*/
static float
SV_CalcRoll (const vec3_t angles, const vec3_t velocity)
{
	float       side, sign, value;

	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs (side);

	value = sv_rollangle;
//	if (cl.inwater)
//		value *= 6;

	if (side < sv_rollspeed)
		side = side * value / sv_rollspeed;
	else
		side = value;

	return side * sign;
}

void
SV_SetIdealPitch (void)
{
	int         dir, step, steps, i, j;
	float       angleval, sinval, cosval;
	float       z[MAX_FORWARD];
	trace_t     tr;
	vec3_t      top, bottom;

	if (!((int) SVfloat (sv_player, flags) & FL_ONGROUND))
		return;

	angleval = SVvector (sv_player, angles)[YAW] * M_PI * 2 / 360;
	sinval = sin (angleval);
	cosval = cos (angleval);

	for (i = 0; i < MAX_FORWARD; i++) {
		top[0] = SVvector (sv_player, origin)[0] + cosval * (i + 3) * 12;
		top[1] = SVvector (sv_player, origin)[1] + sinval * (i + 3) * 12;
		top[2] = SVvector (sv_player, origin)[2] + SVvector (sv_player,
															 view_ofs)[2];
		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;

		tr = SV_Move (top, vec3_origin, vec3_origin, bottom, 1, sv_player);
		if (tr.allsolid)
			return;						// looking at a wall, leave ideal the
										// way it was
		if (tr.fraction == 1)
			return;						// near a dropoff

		z[i] = top[2] + tr.fraction * (bottom[2] - top[2]);
	}

	dir = 0;
	steps = 0;
	for (j = 1; j < i; j++) {
		step = z[j] - z[j - 1];
		if (step > -ON_EPSILON && step < ON_EPSILON)
			continue;

		if (dir && (step - dir > ON_EPSILON || step - dir < -ON_EPSILON))
			return;						// mixed changes

		steps++;
		dir = step;
	}

	if (!dir) {
		SVfloat (sv_player, idealpitch) = 0;
		return;
	}

	if (steps < 2)
		return;
	SVfloat (sv_player, idealpitch) = -dir * sv_idealpitchscale;
}

static void
SV_UserFriction (void)
{
	float      *vel;
	float       control, friction, speed, newspeed;
	vec3_t      start, stop;
	trace_t     trace;

	vel = velocity;

	speed = sqrt (vel[0] * vel[0] + vel[1] * vel[1]);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = origin[0] + vel[0] / speed * 16;
	start[1] = stop[1] = origin[1] + vel[1] / speed * 16;
	start[2] = origin[2] + SVvector (sv_player, mins)[2];
	stop[2] = start[2] - 34;

	trace = SV_Move (start, vec3_origin, vec3_origin, stop, true, sv_player);

	if (trace.fraction == 1.0)
		friction = sv_friction * sv_edgefriction;
	else
		friction = sv_friction;

	// apply friction
	control = speed < sv_stopspeed ? sv_stopspeed : speed;
	newspeed = speed - host_frametime * control * friction;

	if (newspeed < 0)
		newspeed = 0;
	newspeed /= speed;

	vel[0] = vel[0] * newspeed;
	vel[1] = vel[1] * newspeed;
	vel[2] = vel[2] * newspeed;
}

float sv_maxspeed;
static cvar_t sv_maxspeed_cvar = {
	.name = "sv_maxspeed",
	.description =
		"None",
	.default_value = "320",
	.flags = CVAR_SERVERINFO,
	.value = { .type = &cexpr_float, .value = &sv_maxspeed },
};
float sv_accelerate;
static cvar_t sv_accelerate_cvar = {
	.name = "sv_accelerate",
	.description =
		"None",
	.default_value = "10",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sv_accelerate },
};

#if 0
void
SV_Accelerate (vec3_t wishvel)
{
	int         i;
	float       addspeed, accelspeed;
	vec3_t      pushvec;

	if (wishspeed == 0)
		return;

	VectorSubtract (wishvel, velocity, pushvec);
	addspeed = VectorNormalize (pushvec);

	accelspeed = sv_accelerate * host_frametime * addspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		velocity[i] += accelspeed * pushvec[i];
}
#endif

static void
SV_Accelerate (void)
{
	int         i;
	float       addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct (velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate * host_frametime * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		velocity[i] += accelspeed * wishdir[i];
}

static void
SV_AirAccelerate (vec3_t wishveloc)
{
	int         i;
	float       addspeed, wishspd, accelspeed, currentspeed;

	wishspd = VectorNormalize (wishveloc);
	if (wishspd > 30)
		wishspd = 30;
	currentspeed = DotProduct (velocity, wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
//	accelspeed = sv_accelerate * host_frametime;
	accelspeed = sv_accelerate * wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		velocity[i] += accelspeed * wishveloc[i];
}

static void
DropPunchAngle (void)
{
	float       len;

	len = VectorNormalize (SVvector (sv_player, punchangle));

	len -= 10 * host_frametime;
	if (len < 0)
		len = 0;
	VectorScale (SVvector (sv_player, punchangle), len, SVvector (sv_player,
																  punchangle));
}

static void
SV_WaterMove (void)
{
	int         i;
	float       speed, newspeed, wishspeed, addspeed, accelspeed;
	vec3_t      wishvel;

	// user intentions
	AngleVectors (SVvector (sv_player, v_angle), forward, right, up);

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * cmd.forwardmove + right[i] * cmd.sidemove;

	if (!cmd.forwardmove && !cmd.sidemove && !cmd.upmove)
		wishvel[2] -= 60;				// drift towards bottom
	else
		wishvel[2] += cmd.upmove;

	wishspeed = VectorLength (wishvel);
	if (wishspeed > sv_maxspeed) {
		VectorScale (wishvel, sv_maxspeed / wishspeed, wishvel);
		wishspeed = sv_maxspeed;
	}
	wishspeed *= 0.7;

	// water friction
	speed = VectorLength (velocity);
	if (speed) {
		newspeed = speed - host_frametime * speed * sv_friction;
		if (newspeed < 0)
			newspeed = 0;
		VectorScale (velocity, newspeed / speed, velocity);
	} else
		newspeed = 0;

	// water acceleration
	if (!wishspeed)
		return;

	addspeed = wishspeed - newspeed;
	if (addspeed <= 0)
		return;

	VectorNormalize (wishvel);
	accelspeed = sv_accelerate * wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		velocity[i] += accelspeed * wishvel[i];
}

static void
SV_WaterJump (void)
{
	if (sv.time > SVfloat (sv_player, teleport_time) ||
		!SVfloat (sv_player, waterlevel)) {
		SVfloat (sv_player, flags) = (int) SVfloat (sv_player, flags) &
			~FL_WATERJUMP;
		SVfloat (sv_player, teleport_time) = 0;
	}
	SVvector (sv_player, velocity)[0] = SVvector (sv_player, movedir)[0];
	SVvector (sv_player, velocity)[1] = SVvector (sv_player, movedir)[1];
}

static void
SV_AirMove (void)
{
	int         i;
	float       fmove, smove;
	vec3_t      wishvel;

	AngleVectors (SVvector (sv_player, angles), forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

	// hack to not let you back into teleporter
	if (sv.time < SVfloat (sv_player, teleport_time) && fmove < 0)
		fmove = 0;

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * fmove + right[i] * smove;

	if ((int) SVfloat (sv_player, movetype) != MOVETYPE_WALK)
		wishvel[2] = cmd.upmove;
	else
		wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize (wishdir);
	if (wishspeed > sv_maxspeed) {
		VectorScale (wishvel, sv_maxspeed / wishspeed, wishvel);
		wishspeed = sv_maxspeed;
	}

	if (SVfloat (sv_player, movetype) == MOVETYPE_NOCLIP) {	// noclip
		VectorCopy (wishvel, velocity);
	} else if (onground) {
		SV_UserFriction ();
		SV_Accelerate ();
	} else {					// not on ground, so little effect on velocity
		SV_AirAccelerate (wishvel);
	}
}

/*
  SV_ClientThink

  the move fields specify an intended velocity in pix/sec
  the angle fields specify an exact angular motion in degrees
*/
void
SV_ClientThink (void)
{
	vec3_t      v_angle;

	if (SVfloat (sv_player, movetype) == MOVETYPE_NONE)
		return;

	onground = (int) SVfloat (sv_player, flags) & FL_ONGROUND;

	origin = SVvector (sv_player, origin);
	velocity = SVvector (sv_player, velocity);

	DropPunchAngle ();

	// if dead, behave differently
	if (SVfloat (sv_player, health) <= 0)
		return;

	// angles
	// show 1/3 the pitch angle and all the roll angle
	cmd = host_client->cmd;
	angles = SVvector (sv_player, angles);

	VectorAdd (SVvector (sv_player, v_angle), SVvector (sv_player,
														punchangle), v_angle);
	angles[ROLL] = SV_CalcRoll (SVvector (sv_player, angles),
								SVvector (sv_player, velocity)) * 4;
	if (!SVfloat (sv_player, fixangle)) {
		angles[PITCH] = -v_angle[PITCH] / 3;
		angles[YAW] = v_angle[YAW];
	}

	if ((int) SVfloat (sv_player, flags) & FL_WATERJUMP) {
		SV_WaterJump ();
		return;
	}
	// walk
	if ((SVfloat (sv_player, waterlevel) >= 2)
		&& (SVfloat (sv_player, movetype) != MOVETYPE_NOCLIP)) {
		SV_WaterMove ();
		return;
	}

	SV_AirMove ();
}

static void
SV_ReadClientMove (usercmd_t *move)
{
	int         i, bits;
	vec3_t      angle;

	// read ping time
	host_client->ping_times[host_client->num_pings % NUM_PING_TIMES]
		= sv.time - MSG_ReadFloat (net_message);
	host_client->num_pings++;

	// read current angles
	if (sv.protocol == PROTOCOL_NETQUAKE)
		MSG_ReadAngleV (net_message, angle);
	else
		MSG_ReadAngle16V (net_message, angle);

	VectorCopy (angle, SVvector (host_client->edict, v_angle));

	// read movement
	move->forwardmove = (short) MSG_ReadShort (net_message);
	move->sidemove = (short) MSG_ReadShort (net_message);
	move->upmove = (short) MSG_ReadShort (net_message);

	// read buttons
	bits = MSG_ReadByte (net_message);
	SVfloat (host_client->edict, button0) = bits & 1;
	SVfloat (host_client->edict, button2) = (bits & 2) >> 1;

	i = MSG_ReadByte (net_message);
	if (i)
		SVfloat (host_client->edict, impulse) = i;
}

/*
  SV_ReadClientMessage

  Returns false if the client should be killed
*/
static qboolean
SV_ReadClientMessage (void)
{
	int         cmd, ret;
	const char *s;

	do {
	  nextmsg:
		ret = NET_GetMessage (host_client->netconnection);
		if (ret == -1) {
			Sys_Printf ("SV_ReadClientMessage: NET_GetMessage failed\n");
			return false;
		}
		if (!ret)
			return true;

		MSG_BeginReading (net_message);

		while (1) {
			if (!host_client->active)
				return false;			// a command caused an error

			if (net_message->badread) {
				Sys_Printf ("SV_ReadClientMessage: badread\n");
				return false;
			}

			cmd = MSG_ReadByte (net_message);

			switch (cmd) {
			case -1:
				goto nextmsg;			// end of message

			default:
				Sys_Printf ("SV_ReadClientMessage: unknown command char\n");
				return false;

			case clc_nop:
				break;

			case clc_stringcmd:
				s = MSG_ReadString (net_message);
				if (host_client->privileged)
					ret = 2;
				else
					ret = 0;
				if (strncasecmp (s, "status", 6) == 0)
					ret = 1;
				else if (strncasecmp (s, "god", 3) == 0)
					ret = 1;
				else if (strncasecmp (s, "notarget", 8) == 0)
					ret = 1;
				else if (strncasecmp (s, "fly", 3) == 0)
					ret = 1;
				else if (strncasecmp (s, "name", 4) == 0)
					ret = 1;
				else if (strncasecmp (s, "noclip", 6) == 0)
					ret = 1;
				else if (strncasecmp (s, "say", 3) == 0)
					ret = 1;
				else if (strncasecmp (s, "say_team", 8) == 0)
					ret = 1;
				else if (strncasecmp (s, "tell", 4) == 0)
					ret = 1;
				else if (strncasecmp (s, "color", 5) == 0)
					ret = 1;
				else if (strncasecmp (s, "kill", 4) == 0)
					ret = 1;
				else if (strncasecmp (s, "pause", 5) == 0)
					ret = 1;
				else if (strncasecmp (s, "spawn", 5) == 0)
					ret = 1;
				else if (strncasecmp (s, "begin", 5) == 0)
					ret = 1;
				else if (strncasecmp (s, "prespawn", 8) == 0)
					ret = 1;
				else if (strncasecmp (s, "kick", 4) == 0)
					ret = 1;
				else if (strncasecmp (s, "ping", 4) == 0)
					ret = 1;
				else if (strncasecmp (s, "give", 4) == 0)
					ret = 1;
				else if (strncasecmp (s, "ban", 3) == 0)
					ret = 1;
				if (ret == 2)
					Cbuf_InsertText (host_cbuf, s);
				else if (ret == 1)
					Cmd_ExecuteString (s, src_client);
				else
					Sys_MaskPrintf (SYS_dev, "%s tried to %s\n",
									host_client->name, s);
				break;

			case clc_disconnect:
				return false;

			case clc_move:
				SV_ReadClientMove (&host_client->cmd);
				break;
			}
		}
	} while (ret == 1);

	return true;
}

void
SV_RunClients (void)
{
	unsigned    i;

	for (i = 0, host_client = svs.clients; i < svs.maxclients;
		 i++, host_client++) {
		if (!host_client->active)
			continue;

		sv_player = host_client->edict;

		if (!SV_ReadClientMessage ()) {
			SV_DropClient (false);		// client misbehaved...
			continue;
		}

		if (!host_client->spawned) {
			// clear client movement until a new packet is received
			memset (&host_client->cmd, 0, sizeof (host_client->cmd));
			continue;
		}
		// always pause in single player if in console or menus
		if (!sv.paused && (svs.maxclients > 1 || host_in_game))
			SV_ClientThink ();
	}
}

static void
sv_rollspeed_f (void *data, const cvar_t *cvar)
{
	sv_rollspeed = *(float *) cvar->value.value;
}

static void
sv_rollangle_f (void *data, const cvar_t *cvar)
{
	sv_rollangle = *(float *) cvar->value.value;
}

void
SV_User_Init_Cvars (void)
{
	//NOTE: the cl/sv clash is deliberate: dedicated server will use the right
	//vars, but client/server combo will use the one.
	if (isDedicated) {
		Cvar_Register (&sv_rollspeed_cvar, 0, 0);
		Cvar_Register (&sv_rollangle_cvar, 0, 0);
	} else {
		cvar_t     *var;
		var = Cvar_FindVar ("cl_rollspeed");
		Cvar_AddListener (var, sv_rollspeed_f, 0);
		sv_rollspeed = *(float *) var->value.value;
		var = Cvar_FindVar ("cl_rollangle");
		Cvar_AddListener (var, sv_rollangle_f, 0);
		sv_rollangle = *(float *) var->value.value;
	}
	Cvar_Register (&sv_edgefriction_cvar, 0, 0);
	Cvar_Register (&sv_maxspeed_cvar, Cvar_Info, &sv_maxspeed);
	Cvar_Register (&sv_accelerate_cvar, 0, 0);
	Cvar_Register (&sv_idealpitchscale_cvar, 0, 0);

	Cvar_Register (&sv_nostep_cvar, 0, 0);
}
