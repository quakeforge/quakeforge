/*
	pr->pr_cmds.c

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

	$Id$
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

#include "QF/console.h"
#include "compat.h"
#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/cmd.h"
#include "QF/va.h"
#include "host.h"
#include "world.h"
#include "QF/msg.h"
#include "server.h"
#include "sv_progs.h"

#define	RETURN_EDICT(p, e) ((p)->pr_globals[OFS_RETURN].integer_var = EDICT_TO_PROG(p, e))

/*
						BUILT-IN FUNCTIONS
*/


/*
	PF_error

	This is a TERMINAL error, which will kill off the entire server.
	Dumps self.

	error(value)
*/
void
PF_error (progs_t *pr)
{
	const char *s;
	edict_t    *ed;

	s = PF_VarString (pr, 0);
	Con_Printf ("======SERVER ERROR in %s:\n%s\n",
				PR_GetString (&sv_pr_state, pr->pr_xfunction->s_name), s);
	ed = PROG_TO_EDICT (pr, *sv_globals.self);
	ED_Print (pr, ed);

	Host_Error ("Program error");
}

/*
	PF_objerror

	Dumps out self, then an error message.  The program is aborted and self is
	removed, but the level can continue.

	objerror(value)
*/
void
PF_objerror (progs_t *pr)
{
	const char *s;
	edict_t    *ed;

	s = PF_VarString (pr, 0);
	Con_Printf ("======OBJECT ERROR in %s:\n%s\n",
				PR_GetString (&sv_pr_state, pr->pr_xfunction->s_name), s);
	ed = PROG_TO_EDICT (pr, *sv_globals.self);
	ED_Print (pr, ed);
	ED_Free (pr, ed);

	Host_Error ("Program error");
}



/*
	PF_makevectors

	Writes new values for v_forward, v_up, and v_right based on angles
	makevectors(vector)
*/
void
PF_makevectors (progs_t *pr)
{
	AngleVectors (G_VECTOR (pr, OFS_PARM0), *sv_globals.v_forward,
				  *sv_globals.v_right, *sv_globals.v_up);
}

/*
	PF_setorigin

	This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.

	setorigin (entity, origin)
*/
void
PF_setorigin (progs_t *pr)
{
	edict_t    *e;
	float      *org;

	e = G_EDICT (pr, OFS_PARM0);
	org = G_VECTOR (pr, OFS_PARM1);
	VectorCopy (org, SVvector (e, origin));
	SV_LinkEdict (e, false);
}


void
SetMinMaxSize (progs_t *pr, edict_t *e, float *min, float *max,
			   qboolean rotate)
{
	float      *angles;
	vec3_t      rmin, rmax;
	float       bounds[2][3];
	float       xvector[2], yvector[2];
	float       a;
	vec3_t      base, transformed;
	int         i, j, k, l;

	for (i = 0; i < 3; i++)
		if (min[i] > max[i])
			PR_RunError (pr, "backwards mins/maxs");

	rotate = false;						// FIXME: implement rotation properly 
										// 
	// 
	// again

	if (!rotate) {
		VectorCopy (min, rmin);
		VectorCopy (max, rmax);
	} else {
		// find min / max for rotations
		angles = SVvector (e, angles);

		a = angles[1] / 180 * M_PI;

		xvector[0] = cos (a);
		xvector[1] = sin (a);
		yvector[0] = -sin (a);
		yvector[1] = cos (a);

		VectorCopy (min, bounds[0]);
		VectorCopy (max, bounds[1]);

		rmin[0] = rmin[1] = rmin[2] = 9999;
		rmax[0] = rmax[1] = rmax[2] = -9999;

		for (i = 0; i <= 1; i++) {
			base[0] = bounds[i][0];
			for (j = 0; j <= 1; j++) {
				base[1] = bounds[j][1];
				for (k = 0; k <= 1; k++) {
					base[2] = bounds[k][2];

					// transform the point
					transformed[0] =
						xvector[0] * base[0] + yvector[0] * base[1];
					transformed[1] =
						xvector[1] * base[0] + yvector[1] * base[1];
					transformed[2] = base[2];

					for (l = 0; l < 3; l++) {
						if (transformed[l] < rmin[l])
							rmin[l] = transformed[l];
						if (transformed[l] > rmax[l])
							rmax[l] = transformed[l];
					}
				}
			}
		}
	}

// set derived values
	VectorCopy (rmin, SVvector (e, mins));
	VectorCopy (rmax, SVvector (e, maxs));
	VectorSubtract (max, min, SVvector (e, size));

	SV_LinkEdict (e, false);
}

/*
	PF_setsize

	the size box is rotated by the current angle

	setsize (entity, minvector, maxvector)
*/
void
PF_setsize (progs_t *pr)
{
	edict_t    *e;
	float      *min, *max;

	e = G_EDICT (pr, OFS_PARM0);
	min = G_VECTOR (pr, OFS_PARM1);
	max = G_VECTOR (pr, OFS_PARM2);
	SetMinMaxSize (pr, e, min, max, false);
}


/*
	PF_setmodel

	setmodel(entity, model)
*/
void
PF_setmodel (progs_t *pr)
{
	edict_t    *e;
	const char *m, **check;
	model_t    *mod;
	int         i;

	e = G_EDICT (pr, OFS_PARM0);
	m = G_STRING (pr, OFS_PARM1);

// check to see if model was properly precached
	for (i = 0, check = sv.model_precache; *check; i++, check++)
		if (!strcmp (*check, m))
			break;

	if (!*check)
		PR_RunError (pr, "no precache: %s\n", m);


	SVstring (e, model) = PR_SetString (pr, m);
	SVfloat (e, modelindex) = i;				// SV_ModelIndex (m);

	mod = sv.models[(int) SVfloat (e, modelindex)];	// Mod_ForName (m, true);

	if (mod)
		SetMinMaxSize (pr, e, mod->mins, mod->maxs, true);
	else
		SetMinMaxSize (pr, e, vec3_origin, vec3_origin, true);
}

/*
	PF_bprint

	broadcast print to everyone on server

	bprint(value)
*/
void
PF_bprint (progs_t *pr)
{
	const char       *s;

	s = PF_VarString (pr, 0);
	SV_BroadcastPrintf ("%s", s);
}

/*
	PF_sprint

	single print to a specific client

	sprint(clientent, value)
*/
void
PF_sprint (progs_t *pr)
{
	const char       *s;
	client_t   *client;
	int         entnum;

	entnum = G_EDICTNUM (pr, OFS_PARM0);
	s = PF_VarString (pr, 1);

	if (entnum < 1 || entnum > svs.maxclients) {
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum - 1];

	MSG_WriteChar (&client->message, svc_print);
	MSG_WriteString (&client->message, s);
}


/*
	PF_centerprint

	single print to a specific client

	centerprint(clientent, value)
*/
void
PF_centerprint (progs_t *pr)
{
	const char       *s;
	client_t   *client;
	int         entnum;

	entnum = G_EDICTNUM (pr, OFS_PARM0);
	s = PF_VarString (pr, 1);

	if (entnum < 1 || entnum > svs.maxclients) {
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum - 1];

	MSG_WriteChar (&client->message, svc_centerprint);
	MSG_WriteString (&client->message, s);
}


/*
	PF_particle

	particle(origin, color, count)
*/
void
PF_particle (progs_t *pr)
{
	float      *org, *dir;
	float       color;
	float       count;

	org = G_VECTOR (pr, OFS_PARM0);
	dir = G_VECTOR (pr, OFS_PARM1);
	color = G_FLOAT (pr, OFS_PARM2);
	count = G_FLOAT (pr, OFS_PARM3);
	SV_StartParticle (org, dir, color, count);
}


/*
	PF_ambientsound
*/
void
PF_ambientsound (progs_t *pr)
{
	const char      **check;
	const char       *samp;
	float      *pos;
	float       vol, attenuation;
	int         i, soundnum;

	pos = G_VECTOR (pr, OFS_PARM0);
	samp = G_STRING (pr, OFS_PARM1);
	vol = G_FLOAT (pr, OFS_PARM2);
	attenuation = G_FLOAT (pr, OFS_PARM3);

// check to see if samp was properly precached
	for (soundnum = 0, check = sv.sound_precache; *check; check++, soundnum++)
		if (!strcmp (*check, samp))
			break;

	if (!*check) {
		Con_Printf ("no precache: %s\n", samp);
		return;
	}
// add an svc_spawnambient command to the level signon packet

	MSG_WriteByte (&sv.signon, svc_spawnstaticsound);
	for (i = 0; i < 3; i++)
		MSG_WriteCoord (&sv.signon, pos[i]);

	MSG_WriteByte (&sv.signon, soundnum);

	MSG_WriteByte (&sv.signon, vol * 255);
	MSG_WriteByte (&sv.signon, attenuation * 64);

}

/*
	PF_sound

	Each entity can have eight independant sound sources, like voice,
	weapon, feet, etc.

	Channel 0 is an auto-allocate channel, the others override anything
	already running on that entity/channel pair.

	An attenuation of 0 will play full volume everywhere in the level.
	Larger attenuations will drop off.
*/
void
PF_sound (progs_t *pr)
{
	const char       *sample;
	int         channel;
	edict_t    *entity;
	int         volume;
	float       attenuation;

	entity = G_EDICT (pr, OFS_PARM0);
	channel = G_FLOAT (pr, OFS_PARM1);
	sample = G_STRING (pr, OFS_PARM2);
	volume = G_FLOAT (pr, OFS_PARM3) * 255;
	attenuation = G_FLOAT (pr, OFS_PARM4);

	if (volume < 0 || volume > 255)
		Sys_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Sys_Error ("SV_StartSound: channel = %i", channel);

	SV_StartSound (entity, channel, sample, volume, attenuation);
}


/*
	PF_traceline

	Used for use tracing and shot targeting
	Traces are blocked by bbox and exact bsp entityes, and also slide box entities
	if the tryents flag is set.

	traceline (vector1, vector2, tryents)
*/
void
PF_traceline (progs_t *pr)
{
	float      *v1, *v2;
	trace_t     trace;
	int         nomonsters;
	edict_t    *ent;

	v1 = G_VECTOR (pr, OFS_PARM0);
	v2 = G_VECTOR (pr, OFS_PARM1);
	nomonsters = G_FLOAT (pr, OFS_PARM2);
	ent = G_EDICT (pr, OFS_PARM3);

	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	*sv_globals.trace_allsolid = trace.allsolid;
	*sv_globals.trace_startsolid = trace.startsolid;
	*sv_globals.trace_fraction = trace.fraction;
	*sv_globals.trace_inwater = trace.inwater;
	*sv_globals.trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, *sv_globals.trace_endpos);
	VectorCopy (trace.plane.normal, *sv_globals.trace_plane_normal);
	*sv_globals.trace_plane_dist = trace.plane.dist;
	if (trace.ent)
		*sv_globals.trace_ent = EDICT_TO_PROG (pr, trace.ent);
	else
		*sv_globals.trace_ent = EDICT_TO_PROG (pr, sv.edicts);
}


#ifdef QUAKE2
extern trace_t SV_Trace_Toss (edict_t *ent, edict_t *ignore);

void
PF_TraceToss (progs_t *pr)
{
	trace_t     trace;
	edict_t    *ent;
	edict_t    *ignore;

	ent = G_EDICT (pr, OFS_PARM0);
	ignore = G_EDICT (pr, OFS_PARM1);

	trace = SV_Trace_Toss (ent, ignore);

	*sv_globals.trace_allsolid = trace.allsolid;
	*sv_globals.trace_startsolid = trace.startsolid;
	*sv_globals.trace_fraction = trace.fraction;
	*sv_globals.trace_inwater = trace.inwater;
	*sv_globals.trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, *sv_globals.trace_endpos);
	VectorCopy (trace.plane.normal, *sv_globals.trace_plane_normal);
	*sv_globals.trace_plane_dist = trace.plane.dist;
	if (trace.ent)
		*sv_globals.trace_ent = EDICT_TO_PROG (pr, trace.ent);
	else
		*sv_globals.trace_ent = EDICT_TO_PROG (pr, sv.edicts);
}
#endif


/*
	PF_checkpos

	Returns true if the given entity can move to the given position from it's
	current position by walking or rolling.
	FIXME: make work...
	scalar checkpos (entity, vector)
*/
void
PF_checkpos (progs_t *pr)
{
}

//============================================================================

byte        checkpvs[MAX_MAP_LEAFS / 8];

int
PF_newcheckclient (progs_t *pr, int check)
{
	int         i;
	byte       *pvs;
	edict_t    *ent;
	mleaf_t    *leaf;
	vec3_t      org;

// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > svs.maxclients)
		check = svs.maxclients;

	if (check == svs.maxclients)
		i = 1;
	else
		i = check + 1;

	for (;; i++) {
		if (i == svs.maxclients + 1)
			i = 1;

		ent = EDICT_NUM (pr, i);

		if (i == check)
			break;						// didn't find anything else

		if (ent->free)
			continue;
		if (SVfloat (ent, health) <= 0)
			continue;
		if ((int) SVfloat (ent, flags) & FL_NOTARGET)
			continue;

		// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (SVvector (ent, origin), SVvector (ent, view_ofs), org);
	leaf = Mod_PointInLeaf (org, sv.worldmodel);
	pvs = Mod_LeafPVS (leaf, sv.worldmodel);
	memcpy (checkpvs, pvs, (sv.worldmodel->numleafs + 7) >> 3);

	return i;
}

/*
	PF_checkclient

	Returns a client (or object that has a client enemy) that would be a
	valid target.

	If there are more than one valid options, they are cycled each frame

	If (self.origin + self.viewofs) is not in the PVS of the current target,
	it is not returned at all.

	name checkclient ()
*/
#define	MAX_CHECK	16
int         c_invis, c_notvis;
void
PF_checkclient (progs_t *pr)
{
	edict_t    *ent, *self;
	mleaf_t    *leaf;
	int         l;
	vec3_t      view;

// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1) {
		sv.lastcheck = PF_newcheckclient (pr, sv.lastcheck);
		sv.lastchecktime = sv.time;
	}
// return check if it might be visible  
	ent = EDICT_NUM (pr, sv.lastcheck);
	if (ent->free || SVfloat (ent, health) <= 0) {
		RETURN_EDICT (pr, sv.edicts);
		return;
	}
// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT (pr, *sv_globals.self);
	VectorAdd (SVvector (self, origin), SVvector (self, view_ofs), view);
	leaf = Mod_PointInLeaf (view, sv.worldmodel);
	l = (leaf - sv.worldmodel->leafs) - 1;
	if ((l < 0) || !(checkpvs[l >> 3] & (1 << (l & 7)))) {
		c_notvis++;
		RETURN_EDICT (pr, sv.edicts);
		return;
	}
// might be able to see it
	c_invis++;
	RETURN_EDICT (pr, ent);
}

//============================================================================


/*
	PF_stuffcmd

	Sends text over to the client's execution buffer

	stuffcmd (clientent, value)
*/
void
PF_stuffcmd (progs_t *pr)
{
	int         entnum;
	const char       *str;
	client_t   *old;

	entnum = G_EDICTNUM (pr, OFS_PARM0);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError (pr, "Parm 0 not a client");
	str = G_STRING (pr, OFS_PARM1);

	old = host_client;
	host_client = &svs.clients[entnum - 1];
	Host_ClientCommands ("%s", str);
	host_client = old;
}

/*
	PF_localcmd

	Sends text over to the client's execution buffer

	localcmd (string)
*/
void
PF_localcmd (progs_t *pr)
{
	const char       *str;

	str = G_STRING (pr, OFS_PARM0);
	Cbuf_AddText (str);
}

/*
	PF_findradius

	Returns a chain of entities that have origins within a spherical area

	findradius (origin, radius)
*/
void
PF_findradius (progs_t *pr)
{
	edict_t    *ent, *chain;
	float       rad;
	float      *org;
	vec3_t      eorg;
	int         i, j;

	chain = (edict_t *) sv.edicts;

	org = G_VECTOR (pr, OFS_PARM0);
	rad = G_FLOAT (pr, OFS_PARM1);

	ent = NEXT_EDICT (pr, sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, ent = NEXT_EDICT (pr, ent)) {
		if (ent->free)
			continue;
		if (SVfloat (ent, solid) == SOLID_NOT)
			continue;
		for (j = 0; j < 3; j++)
			eorg[j] =
				org[j] - (SVvector (ent, origin)[j] +
						  (SVvector (ent, mins)[j] + SVvector (ent, maxs)[j]) * 0.5);
		if (Length (eorg) > rad)
			continue;

		SVentity (ent, chain) = EDICT_TO_PROG (pr, chain);
		chain = ent;
	}

	RETURN_EDICT (pr, chain);
}


#ifdef QUAKE2
void
PF_etos (progs_t *pr)
{
	snprintf (pr_string_temp, sizeof (pr_string_temp), "entity %i",
			  G_EDICTNUM (pr, OFS_PARM0));
	G_INT (pr, OFS_RETURN) = PR_SetString (pr, pr_string_temp);
}
#endif

void
PF_Spawn (progs_t *pr)
{
	edict_t    *ed;

	ed = ED_Alloc (pr);
	RETURN_EDICT (pr, ed);
}

void
PF_Remove (progs_t *pr)
{
	edict_t    *ed;

	ed = G_EDICT (pr, OFS_PARM0);
	ED_Free (pr, ed);
}


void
PR_CheckEmptyString (progs_t *pr, const char *s)
{
	if (s[0] <= ' ')
		PR_RunError (pr, "Bad string");
}

void
PF_precache_file (progs_t *pr)
{										// precache_file is only used to copy 
										// 
	// 
	// files with qcc, it does nothing
	G_INT (pr, OFS_RETURN) = G_INT (pr, OFS_PARM0);
}

void
PF_precache_sound (progs_t *pr)
{
	const char       *s;
	int         i;

	if (sv.state != ss_loading)
		PR_RunError
			(pr, "PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING (pr, OFS_PARM0);
	G_INT (pr, OFS_RETURN) = G_INT (pr, OFS_PARM0);
	PR_CheckEmptyString (pr, s);

	for (i = 0; i < MAX_SOUNDS; i++) {
		if (!sv.sound_precache[i]) {
			sv.sound_precache[i] = s;
			return;
		}
		if (!strcmp (sv.sound_precache[i], s))
			return;
	}
	PR_RunError (pr, "PF_precache_sound: overflow");
}

void
PF_precache_model (progs_t *pr)
{
	const char       *s;
	int         i;

	if (sv.state != ss_loading)
		PR_RunError
			(pr, "PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING (pr, OFS_PARM0);
	G_INT (pr, OFS_RETURN) = G_INT (pr, OFS_PARM0);
	PR_CheckEmptyString (pr, s);

	for (i = 0; i < MAX_MODELS; i++) {
		if (!sv.model_precache[i]) {
			sv.model_precache[i] = s;
			sv.models[i] = Mod_ForName (s, true);
			return;
		}
		if (!strcmp (sv.model_precache[i], s))
			return;
	}
	PR_RunError (pr, "PF_precache_model: overflow");
}


/*
	PF_walkmove

	float(float yaw, float dist) walkmove
*/
void
PF_walkmove (progs_t *pr)
{
	edict_t    *ent;
	float       yaw, dist;
	vec3_t      move;
	dfunction_t *oldf;
	int         oldself;

	ent = PROG_TO_EDICT (pr, *sv_globals.self);
	yaw = G_FLOAT (pr, OFS_PARM0);
	dist = G_FLOAT (pr, OFS_PARM1);

	if (!((int) SVfloat (ent, flags) & (FL_ONGROUND | FL_FLY | FL_SWIM))) {
		G_FLOAT (pr, OFS_RETURN) = 0;
		return;
	}

	yaw = yaw * M_PI * 2 / 360;

	move[0] = cos (yaw) * dist;
	move[1] = sin (yaw) * dist;
	move[2] = 0;

// save program state, because SV_movestep may call other progs
	oldf = pr->pr_xfunction;
	oldself = *sv_globals.self;

	G_FLOAT (pr, OFS_RETURN) = SV_movestep (ent, move, true);


// restore program state
	pr->pr_xfunction = oldf;
	*sv_globals.self = oldself;
}

/*
	PF_droptofloor

	void() droptofloor
*/
void
PF_droptofloor (progs_t *pr)
{
	edict_t    *ent;
	vec3_t      end;
	trace_t     trace;

	ent = PROG_TO_EDICT (pr, *sv_globals.self);

	VectorCopy (SVvector (ent, origin), end);
	end[2] -= 256;

	trace =
		SV_Move (SVvector (ent, origin), SVvector (ent, mins), SVvector (ent, maxs), end, false,
				 ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT (pr, OFS_RETURN) = 0;
	else {
		VectorCopy (trace.endpos, SVvector (ent, origin));
		SV_LinkEdict (ent, false);
		SVfloat (ent, flags) = (int) SVfloat (ent, flags) | FL_ONGROUND;
		SVentity (ent, groundentity) = EDICT_TO_PROG (pr, trace.ent);
		G_FLOAT (pr, OFS_RETURN) = 1;
	}
}

/*
	PF_lightstyle

	void(float style, string value) lightstyle
*/
void
PF_lightstyle (progs_t *pr)
{
	int         style;
	char       *val;
	client_t   *client;
	int         j;

	style = G_FLOAT (pr, OFS_PARM0);
	val = G_STRING (pr, OFS_PARM1);

// change the string in sv
	sv.lightstyles[style] = val;

// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
		if (client->active || client->spawned) {
			MSG_WriteChar (&client->message, svc_lightstyle);
			MSG_WriteChar (&client->message, style);
			MSG_WriteString (&client->message, val);
		}
}

/*
	PF_checkbottom
*/
void
PF_checkbottom (progs_t *pr)
{
	edict_t    *ent;

	ent = G_EDICT (pr, OFS_PARM0);

	G_FLOAT (pr, OFS_RETURN) = SV_CheckBottom (ent);
}

/*
	PF_pointcontents
*/
void
PF_pointcontents (progs_t *pr)
{
	float      *v;

	v = G_VECTOR (pr, OFS_PARM0);

	G_FLOAT (pr, OFS_RETURN) = SV_PointContents (v);
}

/*
	PF_aim

	Pick a vector for the player to shoot along
	vector aim(entity, missilespeed)
*/
cvar_t     *sv_aim;
void
PF_aim (progs_t *pr)
{
	edict_t    *ent, *check, *bestent;
	vec3_t      start, dir, end, bestdir;
	int         i, j;
	trace_t     tr;
	float       dist, bestdist;
	float       speed;

	ent = G_EDICT (pr, OFS_PARM0);
	speed = G_FLOAT (pr, OFS_PARM1);

	VectorCopy (SVvector (ent, origin), start);
	start[2] += 20;

// try sending a trace straight
	VectorCopy (*sv_globals.v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
	if (tr.ent && SVfloat (tr.ent, takedamage) == DAMAGE_AIM
		&& (!teamplay->int_val || SVfloat (ent, team) <= 0
			|| SVfloat (ent, team) != SVfloat (tr.ent, team))) {
		VectorCopy (*sv_globals.v_forward, G_VECTOR (pr, OFS_RETURN));
		return;
	}
// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim->value;
	bestent = NULL;

	check = NEXT_EDICT (pr, sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, check = NEXT_EDICT (pr, check)) {
		if (SVfloat (check, takedamage) != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay->int_val && SVfloat (ent, team) > 0
			&& SVfloat (ent, team) == SVfloat (check, team)) continue;	// don't aim at
		// teammate
		for (j = 0; j < 3; j++)
			end[j] = SVvector (check, origin)[j]
				+ 0.5 * (SVvector (check, mins)[j] + SVvector (check, maxs)[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, *sv_globals.v_forward);
		if (dist < bestdist)
			continue;					// to far to turn
		tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
		if (tr.ent == check) {			// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if (bestent) {
		VectorSubtract (SVvector (bestent, origin), SVvector (ent, origin), dir);
		dist = DotProduct (dir, *sv_globals.v_forward);
		VectorScale (*sv_globals.v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, G_VECTOR (pr, OFS_RETURN));
	} else {
		VectorCopy (bestdir, G_VECTOR (pr, OFS_RETURN));
	}
}

/*
	PF_changeyaw

	This was a major timewaster in progs, so it was converted to C
*/
void
PF_changeyaw (progs_t *pr)
{
	edict_t    *ent;
	float       ideal, current, move, speed;

	ent = PROG_TO_EDICT (pr, *sv_globals.self);
	current = anglemod (SVvector (ent, angles)[1]);
	ideal = SVfloat (ent, ideal_yaw);
	speed = SVfloat (ent, yaw_speed);

	if (current == ideal)
		return;
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

	SVvector (ent, angles)[1] = anglemod (current + move);
}

#ifdef QUAKE2
/*
	PF_changepitch
*/
void
PF_changepitch (progs_t *pr)
{
	edict_t    *ent;
	float       ideal, current, move, speed;

	ent = G_EDICT (pr, OFS_PARM0);
	current = anglemod (SVvector (ent, angles)[0]);
	ideal = SVfloat (ent, idealpitch);
	speed = SVfloat (ent, pitch_speed);

	if (current == ideal)
		return;
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

	SVvector (ent, angles)[0] = anglemod (current + move);
}
#endif

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

#define	MSG_BROADCAST	0				// unreliable to all
#define	MSG_ONE			1				// reliable to one (msg_entity)
#define	MSG_ALL			2				// reliable to all
#define	MSG_INIT		3				// write to the init string

sizebuf_t  *
WriteDest (progs_t *pr)
{
	int         entnum;
	int         dest;
	edict_t    *ent;

	dest = G_FLOAT (pr, OFS_PARM0);
	switch (dest) {
		case MSG_BROADCAST:
		return &sv.datagram;

		case MSG_ONE:
		ent = PROG_TO_EDICT (pr, *sv_globals.msg_entity);
		entnum = NUM_FOR_EDICT (pr, ent);
		if (entnum < 1 || entnum > svs.maxclients)
			PR_RunError (pr, "WriteDest: not a client");
		return &svs.clients[entnum - 1].message;

		case MSG_ALL:
		return &sv.reliable_datagram;

		case MSG_INIT:
		return &sv.signon;

		default:
		PR_RunError (pr, "WriteDest: bad destination");
		break;
	}

	return NULL;
}

void
PF_WriteByte (progs_t *pr)
{
	MSG_WriteByte (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteChar (progs_t *pr)
{
	MSG_WriteChar (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteShort (progs_t *pr)
{
	MSG_WriteShort (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteLong (progs_t *pr)
{
	MSG_WriteLong (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteAngle (progs_t *pr)
{
	MSG_WriteAngle (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteCoord (progs_t *pr)
{
	MSG_WriteCoord (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteString (progs_t *pr)
{
	MSG_WriteString (WriteDest (pr), G_STRING (pr, OFS_PARM1));
}


void
PF_WriteEntity (progs_t *pr)
{
	MSG_WriteShort (WriteDest (pr), G_EDICTNUM (pr, OFS_PARM1));
}

//=============================================================================

void
PF_makestatic (progs_t *pr)
{
	edict_t    *ent;
	int         i;

	ent = G_EDICT (pr, OFS_PARM0);

	MSG_WriteByte (&sv.signon, svc_spawnstatic);

	MSG_WriteByte (&sv.signon, SV_ModelIndex (PR_GetString (pr, SVstring (ent, model))));

	MSG_WriteByte (&sv.signon, SVfloat (ent, frame));
	MSG_WriteByte (&sv.signon, SVfloat (ent, colormap));
	MSG_WriteByte (&sv.signon, SVfloat (ent, skin));
	for (i = 0; i < 3; i++) {
		MSG_WriteCoord (&sv.signon, SVvector (ent, origin)[i]);
		MSG_WriteAngle (&sv.signon, SVvector (ent, angles)[i]);
	}

// throw the entity away now
	ED_Free (pr, ent);
}

//=============================================================================

/*
	PF_setspawnparms
*/
void
PF_setspawnparms (progs_t *pr)
{
	edict_t    *ent;
	int         i;
	client_t   *client;

	ent = G_EDICT (pr, OFS_PARM0);
	i = NUM_FOR_EDICT (pr, ent);
	if (i < 1 || i > svs.maxclients)
		PR_RunError (pr, "Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i - 1);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		sv_globals.parms[i] = client->spawn_parms[i];
}

/*
	PF_changelevel
*/
void
PF_changelevel (progs_t *pr)
{
#ifdef QUAKE2
	char       *s1, *s2;

	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = true;

	s1 = G_STRING (pr, OFS_PARM0);
	s2 = G_STRING (pr, OFS_PARM1);

	if ((int) *sv_globals.
		serverflags & (SFL_NEW_UNIT | SFL_NEW_EPISODE))
			Cbuf_AddText (va ("changelevel %s %s\n", s1, s2));
	else
		Cbuf_AddText (va ("changelevel2 %s %s\n", s1, s2));
#else
	char       *s;

// make sure we don't issue two changelevels
	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = true;

	s = G_STRING (pr, OFS_PARM0);
	Cbuf_AddText (va ("changelevel %s\n", s));
#endif
}


#ifdef QUAKE2

#define	CONTENT_WATER	-3
#define CONTENT_SLIME	-4
#define CONTENT_LAVA	-5

#define FL_IMMUNE_WATER	131072
#define	FL_IMMUNE_SLIME	262144
#define FL_IMMUNE_LAVA	524288

#define	CHAN_VOICE	2
#define	CHAN_BODY	4

#define	ATTN_NORM	1

void
PF_WaterMove (progs_t *pr)
{
	edict_t    *self;
	int         flags;
	int         waterlevel;
	int         watertype;
	float       drownlevel;
	float       damage = 0.0;

	self = PROG_TO_EDICT (pr, *sv_globals.self);

	if (SVfloat (self, movetype) == MOVETYPE_NOCLIP) {
		SVfloat (self, air_finished) = sv.time + 12;
		G_FLOAT (pr, OFS_RETURN) = damage;
		return;
	}

	if (SVfloat (self, health) < 0) {
		G_FLOAT (pr, OFS_RETURN) = damage;
		return;
	}

	if (SVfloat (self, deadflag) == DEAD_NO)
		drownlevel = 3;
	else
		drownlevel = 1;

	flags = (int) SVfloat (self, flags);
	waterlevel = (int) SVfloat (self, waterlevel);
	watertype = (int) SVfloat (self, watertype);

	if (!(flags & (FL_IMMUNE_WATER + FL_GODMODE)))
		if (((flags & FL_SWIM) && (waterlevel < drownlevel))
			|| (waterlevel >= drownlevel)) {
			if (SVfloat (self, air_finished) < sv.time)
				if (SVfloat (self, pain_finished) < sv.time) {
					SVfloat (self, dmg) = SVfloat (self, dmg) + 2;
					if (SVfloat (self, dmg) > 15)
						SVfloat (self, dmg) = 10;
//                  T_Damage (self, world, world, self.dmg, 0, FALSE);
					damage = SVfloat (self, dmg);
					SVfloat (self, pain_finished) = sv.time + 1.0;
				}
		} else {
			if (SVfloat (self, air_finished) < sv.time)
//              sound (self, CHAN_VOICE, "player/gasp2.wav", 1, ATTN_NORM);
				SV_StartSound (self, CHAN_VOICE, "player/gasp2.wav", 255,
							   ATTN_NORM);
			else if (SVfloat (self, air_finished) < sv.time + 9)
//              sound (self, CHAN_VOICE, "player/gasp1.wav", 1, ATTN_NORM);
				SV_StartSound (self, CHAN_VOICE, "player/gasp1.wav", 255,
							   ATTN_NORM);
			SVfloat (self, air_finished) = sv.time + 12.0;
			SVfloat (self, dmg) = 2;
		}

	if (!waterlevel) {
		if (flags & FL_INWATER) {
			// play leave water sound
//          sound (self, CHAN_BODY, "misc/outwater.wav", 1, ATTN_NORM);
			SV_StartSound (self, CHAN_BODY, "misc/outwater.wav", 255,
						   ATTN_NORM);
			SVfloat (self, flags) = (float) (flags & ~FL_INWATER);
		}
		SVfloat (self, air_finished) = sv.time + 12.0;
		G_FLOAT (pr, OFS_RETURN) = damage;
		return;
	}

	if (watertype == CONTENT_LAVA) {	// do damage
		if (!(flags & (FL_IMMUNE_LAVA + FL_GODMODE)))
			if (SVfloat (self, dmgtime) < sv.time) {
				if (SVfloat (self, radsuit_finished) < sv.time)
					SVfloat (self, dmgtime) = sv.time + 0.2;
				else
					SVfloat (self, dmgtime) = sv.time + 1.0;
//              T_Damage (self, world, world, 10*self.waterlevel, 0, TRUE);
				damage = (float) (10 * waterlevel);
			}
	} else if (watertype == CONTENT_SLIME) {	// do damage
		if (!(flags & (FL_IMMUNE_SLIME + FL_GODMODE)))
			if (SVfloat (self, dmgtime) < sv.time
				&& SVfloat (self, radsuit_finished) < sv.time) {
				SVfloat (self, dmgtime) = sv.time + 1.0;
//              T_Damage (self, world, world, 4*self.waterlevel, 0, TRUE);
				damage = (float) (4 * waterlevel);
			}
	}

	if (!(flags & FL_INWATER)) {

// player enter water sound
		if (watertype == CONTENT_LAVA)
//          sound (self, CHAN_BODY, "player/inlava.wav", 1, ATTN_NORM);
			SV_StartSound (self, CHAN_BODY, "player/inlava.wav", 255,
						   ATTN_NORM);
		if (watertype == CONTENT_WATER)
//          sound (self, CHAN_BODY, "player/inh2o.wav", 1, ATTN_NORM);
			SV_StartSound (self, CHAN_BODY, "player/inh2o.wav", 255, ATTN_NORM);
		if (watertype == CONTENT_SLIME)
//          sound (self, CHAN_BODY, "player/slimbrn2.wav", 1, ATTN_NORM);
			SV_StartSound (self, CHAN_BODY, "player/slimbrn2.wav", 255,
						   ATTN_NORM);

		SVfloat (self, flags) = (float) (flags | FL_INWATER);
		SVfloat (self, dmgtime) = 0;
	}

	if (!(flags & FL_WATERJUMP)) {
//      self.velocity = self.velocity - 0.8*self.waterlevel*frametime*self.velocity;
		VectorMA (SVvector (self, velocity),
				  -0.8 * SVfloat (self, waterlevel) * host_frametime,
				  SVvector (self, velocity), SVvector (self, velocity));
	}

	G_FLOAT (pr, OFS_RETURN) = damage;
}


void
PF_sin (progs_t *pr)
{
	G_FLOAT (pr, OFS_RETURN) = sin (G_FLOAT (pr, OFS_PARM0));
}

void
PF_cos (progs_t *pr)
{
	G_FLOAT (pr, OFS_RETURN) = cos (G_FLOAT (pr, OFS_PARM0));
}

void
PF_sqrt (progs_t *pr)
{
	G_FLOAT (pr, OFS_RETURN) = sqrt (G_FLOAT (pr, OFS_PARM0));
}

#endif

void
PF_Fixme (progs_t *pr)
{
	PR_RunError (pr, "unimplemented bulitin");
}


void
PF_checkextension (progs_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = 0; //FIXME make this function actually useful :P
}

void
SV_PR_Cmds_Init ()
{
	PR_Cmds_Init (&sv_pr_state);

	PR_AddBuiltin (&sv_pr_state, "makevectors", PF_makevectors, 1);	// void(entity e) makevectors = #1
	PR_AddBuiltin (&sv_pr_state, "setorigin", PF_setorigin, 2);	// void(entity e, vector o) setorigin = #2
	PR_AddBuiltin (&sv_pr_state, "setmodel", PF_setmodel, 3);	// void(entity e, string m) setmodel = #3
	PR_AddBuiltin (&sv_pr_state, "setsize", PF_setsize, 4);	// void(entity e, vector min, vector max) setsize = #4
	PR_AddBuiltin (&sv_pr_state, "fixme", PF_Fixme, 5);	// void(entity e, vector min, vector max) setabssize = #5
	PR_AddBuiltin (&sv_pr_state, "sound", PF_sound, 8);	// void(entity e, float chan, string samp) sound = #8
	PR_AddBuiltin (&sv_pr_state, "error", PF_error, 10);	// void(string e) error = #10
	PR_AddBuiltin (&sv_pr_state, "objerror", PF_objerror, 11);	// void(string e) objerror = #11
	PR_AddBuiltin (&sv_pr_state, "spawn", PF_Spawn, 14);	// entity() spawn = #14
	PR_AddBuiltin (&sv_pr_state, "remove", PF_Remove, 15);	// void(entity e) remove = #15
	PR_AddBuiltin (&sv_pr_state, "traceline", PF_traceline, 16);	// float(vector v1, vector v2, float tryents) traceline = #16
	PR_AddBuiltin (&sv_pr_state, "checkclient", PF_checkclient, 17);	// entity() clientlist = #17
	PR_AddBuiltin (&sv_pr_state, "precache_sound", PF_precache_sound, 19);	// void(string s) precache_sound = #19
	PR_AddBuiltin (&sv_pr_state, "precache_model", PF_precache_model, 20);	// void(string s) precache_model = #20
	PR_AddBuiltin (&sv_pr_state, "stuffcmd", PF_stuffcmd, 21);	// void(entity client, string s) stuffcmd = #21
	PR_AddBuiltin (&sv_pr_state, "findradius", PF_findradius, 22);	// entity(vector org, float rad) findradius = #22
	PR_AddBuiltin (&sv_pr_state, "bprint", PF_bprint, 23);	// void(string s) bprint = #23
	PR_AddBuiltin (&sv_pr_state, "sprint", PF_sprint, 24);	// void(entity client, string s) sprint = #24
	PR_AddBuiltin (&sv_pr_state, "walkmove", PF_walkmove, 32);	// float(float yaw, float dist) walkmove = #32
	// 33
	PR_AddBuiltin (&sv_pr_state, "droptofloor", PF_droptofloor, 34);	// float() droptofloor = #34
	PR_AddBuiltin (&sv_pr_state, "lightstyle", PF_lightstyle, 35);	// void(float style, string value) lightstyle = #35
	// 39
	PR_AddBuiltin (&sv_pr_state, "checkbottom", PF_checkbottom, 40);	// float(entity e) checkbottom = #40
	PR_AddBuiltin (&sv_pr_state, "pointcontents", PF_pointcontents, 41);	// float(vector v) pointcontents = #41
	// 42
	PR_AddBuiltin (&sv_pr_state, "aim", PF_aim, 44);	// vector(entity e, float speed) aim = #44
	PR_AddBuiltin (&sv_pr_state, "localcmd", PF_localcmd, 46);	// void(string s) localcmd = #46
	PR_AddBuiltin (&sv_pr_state, "particle", PF_particle, 48);
	PR_AddBuiltin (&sv_pr_state, "changeyaw", PF_changeyaw, 49);	// void() ChangeYaw = #49
	// 50

	PR_AddBuiltin (&sv_pr_state, "writebyte", PF_WriteByte, 52);	// void(float to, float f) WriteByte = #52
	PR_AddBuiltin (&sv_pr_state, "writechar", PF_WriteChar, 53);	// void(float to, float f) WriteChar = #53
	PR_AddBuiltin (&sv_pr_state, "writeshort", PF_WriteShort, 54);	// void(float to, float f) WriteShort = #54
	PR_AddBuiltin (&sv_pr_state, "writelong", PF_WriteLong, 55);	// void(float to, float f) WriteLong = #55
	PR_AddBuiltin (&sv_pr_state, "writecoord", PF_WriteCoord, 56);	// void(float to, float f) WriteCoord = #56
	PR_AddBuiltin (&sv_pr_state, "writeangle", PF_WriteAngle, 57);	// void(float to, float f) WriteAngle = #57
	PR_AddBuiltin (&sv_pr_state, "writestring", PF_WriteString, 58);	// void(float to, string s) WriteString = #58
	PR_AddBuiltin (&sv_pr_state, "writeentity", PF_WriteEntity, 59);	// void(float to, entity s) WriteEntity = #59

#ifdef QUAKE2
/*	PF_sin,
	PF_cos,
	PF_sqrt,
	PF_changepitch,
	PF_TraceToss,
	PF_etos,
	PF_WaterMove, */
#endif

	PR_AddBuiltin (&sv_pr_state, "movetogoal", SV_MoveToGoal, 67);		// void(float step) movetogoal = #67
	PR_AddBuiltin (&sv_pr_state, "precache_file", PF_precache_file, 68);	// string(string s) precache_file = #68
	PR_AddBuiltin (&sv_pr_state, "makestatic", PF_makestatic, 69);	// void(entity e) makestatic = #69

	PR_AddBuiltin (&sv_pr_state, "changelevel", PF_changelevel, 70);	// void(string s) changelevel = #70
	// 71

	PR_AddBuiltin (&sv_pr_state, "centerprint", PF_centerprint, 73);	// void(...) centerprint = #73

	PR_AddBuiltin (&sv_pr_state, "ambientsound", PF_ambientsound, 74);	// void(vector pos, string samp, float vol, float atten) ambientsound = #74

	PR_AddBuiltin (&sv_pr_state, "precache_model", PF_precache_model, 75);	// string(string s) precache_model2 = #75
	PR_AddBuiltin (&sv_pr_state, "precache_sound", PF_precache_sound, 76);	// string(string s) precache_sound2 = #76 precache_sound2 is different only for qcc
	PR_AddBuiltin (&sv_pr_state, "precache_file", PF_precache_file, 77);	// string(string s) precache_file2 = #77

	PR_AddBuiltin (&sv_pr_state, "setspawnparms", PF_setspawnparms, 78);	// void(entity e) setspawnparms = #78

	PR_AddBuiltin (&sv_pr_state, "checkextension", PF_checkextension, 99);	// = #99
}
