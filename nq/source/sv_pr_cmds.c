/*
	sv_pr_cmds.c

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <ctype.h>

#include "QF/cbuf.h"
#include "QF/clip_hull.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/ruamoko.h"
#include "QF/set.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"
#include "world.h"

#include "nq/include/host.h"
#include "nq/include/server.h"
#include "nq/include/sv_progs.h"

/* BUILT-IN FUNCTIONS */


/*
	PF_error

	This is a TERMINAL error, which will kill off the entire server.
	Dumps self.

	error (value)
	// void (string e) error
*/
static void
PF_error (progs_t *pr)
{
	const char *s;
	edict_t    *ed;

	s = PF_VarString (pr, 0);
	Sys_Printf ("======SERVER ERROR in %s:\n%s\n",
				PR_GetString (pr, pr->pr_xfunction->descriptor->s_name), s);
	ed = PROG_TO_EDICT (pr, *sv_globals.self);
	ED_Print (pr, ed, 0);

	Host_Error ("Program error");
}

/*
	PF_objerror

	Dumps out self, then an error message.  The program is aborted and self is
	removed, but the level can continue.

	objerror (value)
	// void (string e) objerror
*/
static void
PF_objerror (progs_t *pr)
{
	const char *s;
	edict_t    *ed;

	s = PF_VarString (pr, 0);
	Sys_Printf ("======OBJECT ERROR in %s:\n%s\n",
				PR_GetString (pr, pr->pr_xfunction->descriptor->s_name), s);
	ed = PROG_TO_EDICT (pr, *sv_globals.self);
	ED_Print (pr, ed, 0);
	ED_Free (pr, ed);

	Host_Error ("Program error");
}

/*
	PF_makevectors

	Writes new values for v_forward, v_up, and v_right based on angles
	void (vector angles) makevectors
*/
static void
PF_makevectors (progs_t *pr)
{
	AngleVectors (P_VECTOR (pr, 0), *sv_globals.v_forward,
				  *sv_globals.v_right, *sv_globals.v_up);
}

/*
	PF_setorigin

	This is the only valid way to move an object without using the physics of
	the world (setting velocity and waiting).  Directly changing origin will
	not set internal links correctly, so clipping would be messed up.  This
	should be called when an object is spawned, and then only if it is
	teleported.

	setorigin (entity, origin)
	// void (entity e, vector o) setorigin
*/
static void
PF_setorigin (progs_t *pr)
{
	edict_t    *e;
	float      *org;

	e = P_EDICT (pr, 0);
	org = P_VECTOR (pr, 1);
	VectorCopy (org, SVvector (e, origin));
	SV_LinkEdict (e, false);
}

static void
SetMinMaxSize (progs_t *pr, edict_t *e, const vec3_t min, const vec3_t max,
			   qboolean rotate)
{
	float       a;
	float       bounds[2][3];
	float       xvector[2], yvector[2];
	float      *angles;
	int         i, j, k, l;
	vec3_t      rmin, rmax, base, transformed;

	for (i = 0; i < 3; i++)
		if (min[i] > max[i])
			PR_RunError (pr, "backwards mins/maxs");

	rotate = false;					// FIXME: implement rotation properly again

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
	// void (entity e, vector min, vector max) setsize
*/
static void
PF_setsize (progs_t *pr)
{
	edict_t    *e;
	float      *min, *max;

	e = P_EDICT (pr, 0);
	min = P_VECTOR (pr, 1);
	max = P_VECTOR (pr, 2);
	SetMinMaxSize (pr, e, min, max, false);
}

/*
	PF_setmodel

	setmodel (entity, model)
	// void (entity e, string m) setmodel
*/
static void
PF_setmodel (progs_t *pr)
{
	edict_t    *e;
	const char *m, **check;
	int         i;
	model_t    *mod;

	e = P_EDICT (pr, 0);
	m = P_GSTRING (pr, 1);

	// check to see if model was properly precached
	for (i = 0, check = sv.model_precache; *check; i++, check++)
		if (!strcmp (*check, m))
			break;

	if (!*check)
		PR_RunError (pr, "no precache: %s\n", m);

	SVstring (e, model) = PR_SetString (pr, m);
	SVfloat (e, modelindex) = i;

	mod = sv.models[i];

	if (mod) {
		// FIXME disabled for now as setting clipmins/maxs is currently
		// too messy
		//if (mod->type == mod_brush)
		//	SetMinMaxSize (pr, e, mod->clipmins, mod->clipmaxs, true);
		//else
			SetMinMaxSize (pr, e, mod->mins, mod->maxs, true);
	} else {
		SetMinMaxSize (pr, e, vec3_origin, vec3_origin, true);
	}
}

/*
	PF_bprint

	broadcast print to everyone on server

	bprint (value)
	// void (string s) bprint
*/
static void
PF_bprint (progs_t *pr)
{
	const char *s;

	s = PF_VarString (pr, 0);
	SV_BroadcastPrintf ("%s", s);
}

/*
	PF_sprint

	single print to a specific client

	sprint (clientent, value)
	// void (entity client, string s) sprint
*/
static void
PF_sprint (progs_t *pr)
{
	const char *s;
	client_t   *client;
	int         entnum;

	entnum = P_EDICTNUM (pr, 0);
	s = PF_VarString (pr, 1);

	if (entnum < 1 || entnum > svs.maxclients) {
		Sys_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum - 1];

	MSG_WriteByte (&client->message, svc_print);
	MSG_WriteString (&client->message, s);
}

/*
	PF_centerprint

	single print to a specific client

	centerprint (clientent, value)
	// void (...) centerprint
*/
static void
PF_centerprint (progs_t *pr)
{
	const char *s;
	client_t   *cl;
	int         entnum;

	entnum = P_EDICTNUM (pr, 0);
	s = PF_VarString (pr, 1);

	if (entnum < 1 || entnum > svs.maxclients) {
		Sys_Printf ("tried to sprint to a non-client\n");
		return;
	}

	cl = &svs.clients[entnum - 1];

	MSG_WriteByte (&cl->message, svc_centerprint);
	MSG_WriteString (&cl->message, s);
}

// void (vector o, vector d, float color, float count) particle
static void
PF_particle (progs_t *pr)
{
	float      *org, *dir;
	float       color;
	float       count;

	org = P_VECTOR (pr, 0);
	dir = P_VECTOR (pr, 1);
	color = P_FLOAT (pr, 2);
	count = P_FLOAT (pr, 3);
	SV_StartParticle (org, dir, color, count);
}

/*
	PF_ambientsound
	// void (vector pos, string samp, float vol, float atten) ambientsound
*/
static void
PF_ambientsound (progs_t *pr)
{
	const char **check;
	const char *samp;
	float      *pos;
	float       vol, attenuation;
	int         soundnum;
	int         large = false;

	pos = P_VECTOR (pr, 0);
	samp = P_GSTRING (pr, 1);
	vol = P_FLOAT (pr, 2);
	attenuation = P_FLOAT (pr, 3);

	// check to see if samp was properly precached
	for (soundnum = 0, check = sv.sound_precache; *check; check++, soundnum++)
		if (!strcmp (*check, samp))
			break;

	if (!*check) {
		Sys_Printf ("no precache: %s\n", samp);
		return;
	}
	if (soundnum > 255) {
		if (sv.protocol == PROTOCOL_NETQUAKE)
			return;
		large = true;
	}

	// add an svc_spawnambient command to the level signon packet
	MSG_WriteByte (&sv.signon,
				   large ? svc_spawnstaticsound2 : svc_spawnstaticsound);
	MSG_WriteCoordV (&sv.signon, pos);
	if (large)
		MSG_WriteShort (&sv.signon, soundnum);
	else
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
	// void (entity e, float chan, string samp) sound
*/
static void
PF_sound (progs_t *pr)
{
	const char *sample;
	edict_t    *entity;
	float       attenuation;
	int         channel, volume;

	entity = P_EDICT (pr, 0);
	channel = P_FLOAT (pr, 1);
	sample = P_GSTRING (pr, 2);
	volume = P_FLOAT (pr, 3) * 255;
	attenuation = P_FLOAT (pr, 4);

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

	Used for use tracing and shot targeting.
	Traces are blocked by bbox and exact bsp entityes, and also slide box
	entities if the tryents flag is set.

	traceline (vector1, vector2, tryents)
	// float (vector v1, vector v2, float tryents) traceline
*/
static void
PF_traceline (progs_t *pr)
{
	float      *v1, *v2;
	edict_t    *ent;
	int         nomonsters;
	trace_t     trace;

	v1 = P_VECTOR (pr, 0);
	v2 = P_VECTOR (pr, 1);
	nomonsters = P_FLOAT (pr, 2);
	ent = P_EDICT (pr, 3);

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

/*
	PF_tracebox
	// void (vector start, vector mins, vector maxs, vector end, float type,
	//       entity passent) tracebox

	Wrapper around SV_Move, this makes PF_movetoground and PF_traceline
	redundant.
*/
static void
PF_tracebox (progs_t *pr)
{
	edict_t    *ent;
	float      *start, *end, *mins, *maxs;
	int         type;
	trace_t     trace;

	start = P_VECTOR (pr, 0);
	mins = P_VECTOR (pr, 1);
	maxs = P_VECTOR (pr, 2);
	end = P_VECTOR (pr, 3);
	type = P_FLOAT (pr, 4);
	ent = P_EDICT (pr, 5);

	trace = SV_Move (start, mins, maxs, end, type, ent);

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

/*
	PF_checkpos

	Returns true if the given entity can move to the given position from it's
	current position by walking or rolling.
	FIXME: make work...
	scalar checkpos (entity, vector)
*/
static void __attribute__ ((used))
PF_checkpos (progs_t *pr)
{
}

static set_t *checkpvs;

static int
PF_newcheckclient (progs_t *pr, int check)
{
	edict_t    *ent;
	int         i;
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
	if (!checkpvs) {
		checkpvs = set_new_size (sv.worldmodel->brush.numleafs);
	}
	set_assign (checkpvs, Mod_LeafPVS (leaf, sv.worldmodel));

	return i;
}

#define	MAX_CHECK	16
int         c_invis, c_notvis;

/*
	PF_checkclient

	Returns a client (or object that has a client enemy) that would be a
	valid target.

	If there are more than one valid options, they are cycled each frame

	If (self.origin + self.viewofs) is not in the PVS of the current target,
	it is not returned at all.

	name checkclient ()
	// entity () clientlist
*/
static void
PF_checkclient (progs_t *pr)
{
	edict_t    *ent, *self;
	int         l;
	mleaf_t    *leaf;
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
	l = (leaf - sv.worldmodel->brush.leafs) - 1;
	if (!set_is_member (checkpvs, l)) {
		c_notvis++;
		RETURN_EDICT (pr, sv.edicts);
		return;
	}
	// might be able to see it
	c_invis++;
	RETURN_EDICT (pr, ent);
}

/*
	PF_stuffcmd

	Sends text over to the client's execution buffer

	stuffcmd (clientent, value)
	// void (entity client, string s) stuffcmd
*/
static void
PF_stuffcmd (progs_t *pr)
{
	const char *str;
	client_t   *old;
	int         entnum;

	entnum = P_EDICTNUM (pr, 0);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError (pr, "Parm 0 not a client");
	str = P_GSTRING (pr, 1);

	old = host_client;
	host_client = &svs.clients[entnum - 1];
	Host_ClientCommands ("%s", str);
	host_client = old;
}

/*
	PF_localcmd

	Inserts text into the server console's execution buffer

	localcmd (string)
	// void (string s) localcmd
*/
static void
PF_localcmd (progs_t *pr)
{
	const char       *str;

	str = P_GSTRING (pr, 0);
	Cbuf_AddText (host_cbuf, str);
}

/*
	PF_findradius

	Returns a chain of entities that have origins within a spherical area

	findradius (origin, radius)
	// entity (vector org, float rad) findradius
*/
static void
PF_findradius (progs_t *pr)
{
	edict_t    *ent, *chain;
	float       rsqr;
	vec_t      *emins, *emaxs, *org;
	int         i, j;
	vec3_t      eorg;

	chain = (edict_t *) sv.edicts;

	org = P_VECTOR (pr, 0);
	rsqr = P_FLOAT (pr, 1);
	rsqr *= rsqr;					// Square early, sqrt never

	ent = NEXT_EDICT (pr, sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, ent = NEXT_EDICT (pr, ent)) {
		if (ent->free)
			continue;
		if (SVfloat (ent, solid) == SOLID_NOT)
			continue;
		emins = SVvector (ent, absmin);
		emaxs = SVvector (ent, absmax);
		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - 0.5 * (emins[j] + emaxs[j]);
		if (DotProduct (eorg, eorg) > rsqr)
			continue;

		SVentity (ent, chain) = EDICT_TO_PROG (pr, chain);
		chain = ent;
	}

	RETURN_EDICT (pr, chain);
}

// entity () spawn
static void
PF_Spawn (progs_t *pr)
{
	edict_t    *ed;

	ed = ED_Alloc (pr);
	RETURN_EDICT (pr, ed);
}

// void (entity e) remove
static void
PF_Remove (progs_t *pr)
{
	edict_t    *ed;

	ed = P_EDICT (pr, 0);
	ED_Free (pr, ed);
}

static void
PR_CheckEmptyString (progs_t *pr, const char *s)
{
	if (s[0] <= ' ')
		PR_RunError (pr, "Bad string");
}

static int
do_precache (progs_t *pr, const char **cache, int max, const char *name,
			 const char *func)
{
	int         i;
	char       *s;

	if (sv.state != ss_loading)
		PR_RunError (pr, "%s: Precache can be done only in spawn functions",
					 func);

	PR_CheckEmptyString (pr, name);

	s = Hunk_TempAlloc (strlen (name) + 1);
	for (i = 0; *name; i++, name++) {
		int         c = (byte) *name;
		s[i] = tolower (c);
	}
	s[i] = 0;

	for (i = 0; i < MAX_SOUNDS; i++) {
		if (!cache[i]) {
			char *c = Hunk_Alloc (strlen (s) + 1);
			strcpy (c, s);
			cache[i] = c; // blah, const
			Sys_MaskPrintf (SYS_dev, "%s: %3d %s\n", func, i, s);
			return i;
		}
		if (!strcmp (cache[i], s))
			return i;
	}
	PR_RunError (pr, "%s: overflow", func);
}

// string (string s) precache_file
// string (string s) precache_file2
static void
PF_precache_file (progs_t *pr)
{
	// precache_file is used only to copy files with qcc, it does nothing
	R_INT (pr) = P_INT (pr, 0);
}

// void (string s) precache_sound
// string (string s) precache_sound2
static void
PF_precache_sound (progs_t *pr)
{
	do_precache (pr, sv.sound_precache, MAX_SOUNDS, P_GSTRING (pr, 0),
				 "precache_sound");
	R_INT (pr) = P_INT (pr, 0);
}

// void (string s) precache_model
// string (string s) precache_model2
static void
PF_precache_model (progs_t *pr)
{
	int         ind;
	const char *mod = P_GSTRING (pr, 0);
	ind = do_precache (pr, sv.model_precache, MAX_MODELS, mod,
					   "precache_model");
	sv.models[ind] = Mod_ForName (mod, true);
	R_INT (pr) = P_INT (pr, 0);
}

/*
	PF_walkmove

	float (float yaw, float dist) walkmove
	// float (float yaw, float dist) walkmove
*/
static void
PF_walkmove (progs_t *pr)
{
	edict_t    *ent;
	float       yaw, dist;
	int         oldself;
	vec3_t      move;

	ent = PROG_TO_EDICT (pr, *sv_globals.self);
	yaw = P_FLOAT (pr, 0);
	dist = P_FLOAT (pr, 1);

	if (!((int) SVfloat (ent, flags) & (FL_ONGROUND | FL_FLY | FL_SWIM))) {
		R_FLOAT (pr) = 0;
		return;
	}

	yaw = yaw * M_PI * 2 / 360;

	move[0] = cos (yaw) * dist;
	move[1] = sin (yaw) * dist;
	move[2] = 0;

	// save program state, because SV_movestep may call other progs
	oldself = *sv_globals.self;

	R_FLOAT (pr) = SV_movestep (ent, move, true);

	// restore program state
	*sv_globals.self = oldself;
}

/*
	PF_droptofloor

	void () droptofloor
	// float () droptofloor
*/
static void
PF_droptofloor (progs_t *pr)
{
	edict_t    *ent;
	trace_t     trace;
	vec3_t      end;

	ent = PROG_TO_EDICT (pr, *sv_globals.self);

	VectorCopy (SVvector (ent, origin), end);
	end[2] -= 256;

	trace = SV_Move (SVvector (ent, origin), SVvector (ent, mins),
					 SVvector (ent, maxs), end, false, ent);

	if (trace.fraction == 1 || trace.allsolid) {
		R_FLOAT (pr) = 0;
	} else {
		VectorCopy (trace.endpos, SVvector (ent, origin));
		SV_LinkEdict (ent, false);
		SVfloat (ent, flags) = (int) SVfloat (ent, flags) | FL_ONGROUND;
		SVentity (ent, groundentity) = EDICT_TO_PROG (pr, trace.ent);
		R_FLOAT (pr) = 1;
	}
}

/*
	PF_lightstyle

	void (float style, string value) lightstyle
	// void (float style, string value) lightstyle
*/
static void
PF_lightstyle (progs_t *pr)
{
	const char *val;
	client_t   *cl;
	int         style, j;

	style = P_FLOAT (pr, 0);
	val = P_GSTRING (pr, 1);

	// change the string in sv
	sv.lightstyles[style] = val;

	// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j = 0, cl = svs.clients; j < svs.maxclients; j++, cl++)
		if (cl->active || cl->spawned) {
			MSG_WriteByte (&cl->message, svc_lightstyle);
			MSG_WriteByte (&cl->message, style);
			MSG_WriteString (&cl->message, val);
		}
}

// float (entity e) checkbottom
static void
PF_checkbottom (progs_t *pr)
{
	edict_t    *ent;

	ent = P_EDICT (pr, 0);

	R_FLOAT (pr) = SV_CheckBottom (ent);
}

// float (vector v) pointcontents
static void
PF_pointcontents (progs_t *pr)
{
	float      *v;

	v = P_VECTOR (pr, 0);

	R_FLOAT (pr) = SV_PointContents (v);
}

cvar_t     *sv_aim;

/*
	PF_aim

	Pick a vector for the player to shoot along
	vector aim (entity, missilespeed)
	// vector (entity e, float speed) aim
*/
static void
PF_aim (progs_t *pr)
{
	edict_t    *ent, *check, *bestent;
	float       dist, bestdist, speed;
	float      *mins, *maxs, *org;
	int         i, j;
	trace_t     tr;
	vec3_t      start, dir, end, bestdir;

	ent = P_EDICT (pr, 0);
	speed = P_FLOAT (pr, 1);
	(void) speed; //FIXME

	VectorCopy (SVvector (ent, origin), start);
	start[2] += 20;

	// try sending a trace straight
	VectorCopy (*sv_globals.v_forward, dir);
	VectorMultAdd (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
	if (tr.ent && SVfloat (tr.ent, takedamage) == DAMAGE_AIM
		&& (!teamplay->int_val || SVfloat (ent, team) <= 0
			|| SVfloat (ent, team) != SVfloat (tr.ent, team))) {
		VectorCopy (*sv_globals.v_forward, R_VECTOR (pr));
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
			&& SVfloat (ent, team) == SVfloat (check, team))
			continue;	// don't aim at teammate

		mins = SVvector (check, mins);
		maxs = SVvector (check, maxs);
		org = SVvector (check, origin);
		for (j = 0; j < 3; j++)
			end[j] = org[j] + 0.5 * (mins[j] + maxs[j]);
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
		VectorSubtract (SVvector (bestent, origin), SVvector (ent, origin),
						dir);
		dist = DotProduct (dir, *sv_globals.v_forward);
		VectorScale (*sv_globals.v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, R_VECTOR (pr));
	} else {
		VectorCopy (bestdir, R_VECTOR (pr));
	}
}

/*
	PF_changeyaw

	This was a major timewaster in progs, so it was converted to C
	// void () ChangeYaw
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

/* MESSAGE WRITING */

#define	MSG_BROADCAST	0				// unreliable to all
#define	MSG_ONE			1				// reliable to one (msg_entity)
#define	MSG_ALL			2				// reliable to all
#define	MSG_INIT		3				// write to the init string

static __attribute__((pure)) sizebuf_t *
WriteDest (progs_t *pr)
{
	int         entnum;
	int         dest;
	edict_t    *ent;

	dest = P_FLOAT (pr, 0);
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

// void (float to, ...) WriteBytes
static void
PF_WriteBytes (progs_t *pr)
{
	int         i, p;
	sizebuf_t  *msg = WriteDest (pr);

	for (i = 1; i < pr->pr_argc; i++) {
		p = P_FLOAT (pr, i);
		MSG_WriteByte (msg, p);
	}
}

// void (float to, float f) WriteByte
static void
PF_WriteByte (progs_t *pr)
{
	MSG_WriteByte (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, float f) WriteChar
static void
PF_WriteChar (progs_t *pr)
{
	MSG_WriteByte (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, float f) WriteShort
static void
PF_WriteShort (progs_t *pr)
{
	MSG_WriteShort (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, float f) WriteLong
static void
PF_WriteLong (progs_t *pr)
{
	MSG_WriteLong (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, float f) WriteAngle
static void
PF_WriteAngle (progs_t *pr)
{
	MSG_WriteAngle (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, float f) WriteCoord
static void
PF_WriteCoord (progs_t *pr)
{
	MSG_WriteCoord (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, vector v) WriteAngleV
static void
PF_WriteAngleV (progs_t *pr)
{
	float      *ang = P_VECTOR (pr, 1);

	MSG_WriteAngleV (WriteDest (pr), ang);
}

// void (float to, vector v) WriteCoordV
static void
PF_WriteCoordV (progs_t *pr)
{
	float      *coord = P_VECTOR (pr, 1);

	MSG_WriteCoordV (WriteDest (pr), coord);
}

// void (float to, string s) WriteString
static void
PF_WriteString (progs_t *pr)
{
	MSG_WriteString (WriteDest (pr), P_GSTRING (pr, 1));
}

// void (float to, entity s) WriteEntity
static void
PF_WriteEntity (progs_t *pr)
{
	MSG_WriteShort (WriteDest (pr), P_EDICTNUM (pr, 1));
}

// void (entity e) makestatic
static void
PF_makestatic (progs_t *pr)
{
	const char *model;
	edict_t    *ent;
	int         bits = 0;

	ent = P_EDICT (pr, 0);
	if (SVdata (ent)->alpha == ENTALPHA_ZERO) {
		//johnfitz -- don't send invisible static entities
		goto nosend;
	}

	model = PR_GetString (pr, SVstring (ent, model));
	if (sv.protocol == PROTOCOL_NETQUAKE) {
		if (SV_ModelIndex (model) & 0xff00
			|| (int) SVfloat (ent, frame) & 0xff00)
			goto nosend;
	} else {
		if (SV_ModelIndex (model) & 0xff00)
			bits |= B_LARGEMODEL;
		if ((int) SVfloat (ent, frame) & 0xff00)
			bits |= B_LARGEFRAME;
		if (SVdata (ent)->alpha != ENTALPHA_DEFAULT)
			bits |= B_ALPHA;
	}

	if (bits) {
		MSG_WriteByte (&sv.signon, svc_spawnstatic2);
		MSG_WriteByte (&sv.signon, bits);
	} else {
		MSG_WriteByte (&sv.signon, svc_spawnstatic);
	}

	if (bits & B_LARGEMODEL)
		MSG_WriteShort (&sv.signon, SV_ModelIndex (model));
	else
		MSG_WriteByte (&sv.signon, SV_ModelIndex (model));

	if (bits & B_LARGEFRAME)
		MSG_WriteShort (&sv.signon, SVfloat (ent, frame));
	else
		MSG_WriteByte (&sv.signon, SVfloat (ent, frame));

	MSG_WriteByte (&sv.signon, SVfloat (ent, colormap));
	MSG_WriteByte (&sv.signon, SVfloat (ent, skin));

	MSG_WriteCoordAngleV (&sv.signon, SVvector (ent, origin),
						  SVvector (ent, angles));

	if (bits & B_ALPHA)
		MSG_WriteByte (&sv.signon, SVdata (ent)->alpha);
	// throw the entity away now
nosend:
	ED_Free (pr, ent);
}

// void (entity e) setspawnparms
static void
PF_setspawnparms (progs_t *pr)
{
	client_t   *client;
	edict_t    *ent;
	int         i;

	ent = P_EDICT (pr, 0);
	i = NUM_FOR_EDICT (pr, ent);
	if (i < 1 || i > svs.maxclients)
		PR_RunError (pr, "Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i - 1);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		sv_globals.parms[i] = client->spawn_parms[i];
}

// void (string s) changelevel
static void
PF_changelevel (progs_t *pr)
{
	const char *s;

	// make sure we don't issue two changelevels
	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = true;

	s = P_GSTRING (pr, 0);
	Cbuf_AddText (host_cbuf, va (0, "changelevel %s\n", s));
}

// entity (entity ent) testentitypos
static void
PF_testentitypos (progs_t *pr)
{
	edict_t    *ent = P_EDICT (pr, 0);
	ent = SV_TestEntityPosition (ent);
	RETURN_EDICT (pr, ent ? ent : sv.edicts);
}

#define MAX_PF_HULLS 64		// FIXME make dynamic?
clip_hull_t *pf_hull_list[MAX_PF_HULLS];

// integer (entity ent, vector point) hullpointcontents
static void
PF_hullpointcontents (progs_t *pr)
{
	edict_t    *edict = P_EDICT (pr, 0);
	float      *mins = P_VECTOR (pr, 1);
	float      *maxs = P_VECTOR (pr, 2);
	float      *point = P_VECTOR (pr, 3);
	hull_t     *hull;
	vec3_t      offset;

	hull = SV_HullForEntity (edict, mins, maxs, 0, offset);
	VectorSubtract (point, offset, offset);
	R_INT (pr) = SV_HullPointContents (hull, 0, offset);
}

// vector (integer hull, integer max) getboxbounds
static void
PF_getboxbounds (progs_t *pr)
{
	clip_hull_t *ch;
	int         h = P_INT (pr, 0) - 1;

	if (h < 0 || h > MAX_PF_HULLS - 1 || !(ch = pf_hull_list[h]))
		PR_RunError (pr, "PF_getboxbounds: invalid box hull handle\n");

	if (P_INT (pr, 1)) {
		VectorCopy (ch->maxs, R_VECTOR (pr));
	} else {
		VectorCopy (ch->mins, R_VECTOR (pr));
	}
}

// integer () getboxhull
static void
PF_getboxhull (progs_t *pr)
{
	clip_hull_t *ch = 0;
	int         i;

	for (i = 0; i < MAX_PF_HULLS; i++) {
		if (!pf_hull_list[i]) {
			ch = MOD_Alloc_Hull (6, 6);
			break;
		}
	}

	if (ch) {
		pf_hull_list[i] = ch;
		R_INT (pr) = i + 1;
		for (i = 0; i < MAX_MAP_HULLS; i++)
			SV_InitHull (ch->hulls[i], ch->hulls[i]->clipnodes,
						 ch->hulls[i]->planes);
	} else {
		R_INT (pr) = 0;
	}
}

// void (integer hull) freeboxhull
static void
PF_freeboxhull (progs_t *pr)
{
	int         h = P_INT (pr, 0) - 1;
	clip_hull_t *ch;

	if (h < 0 || h > MAX_PF_HULLS - 1 || !(ch = pf_hull_list[h]))
		PR_RunError (pr, "PF_freeboxhull: invalid box hull handle\n");
	pf_hull_list[h] = 0;
	MOD_Free_Hull (ch);
}

static vec_t
calc_dist (vec3_t p, vec3_t n, vec3_t *offsets)
{
	int        i;
	vec_t      d = DotProduct (p, n);
	vec3_t     s, v;

	VectorScale (n, d, s);
	for (i = 0; i < 3; i++)
		if (s[i] < 0)
			v[i] = offsets[0][i];
		else
			v[i] = offsets[1][i];
	VectorAdd (p, v, v);
	return DotProduct (v, n);
}

// void (integer hull, vector right, vector forward, vector up, vector mins, vector maxs) rotate_bbox
static void
PF_rotate_bbox (progs_t *pr)
{
	clip_hull_t *ch;
	float       l;
	float      *mi = P_VECTOR (pr, 4);
	float      *ma = P_VECTOR (pr, 5);
	float      *dir[3] = {
					P_VECTOR (pr, 1),
					P_VECTOR (pr, 2),
					P_VECTOR (pr, 3),
				};

	hull_t     *hull;
	int         i, j;
	int         h = P_INT (pr, 0) - 1;

	vec3_t      mins, maxs, d;
	float      *verts[6] = {maxs, mins, maxs, mins, maxs, mins};
	vec3_t      v[8];
	vec3_t      offsets[3][2] = {
					{ {   0,   0,   0 }, {  0,  0,  0} },
					{ { -16, -16, -32 }, { 16, 16, 24} },
					{ { -32, -32, -64 }, { 32, 32, 24} },
				};

	if (h < 0 || h > MAX_PF_HULLS - 1 || !(ch = pf_hull_list[h]))
		PR_RunError (pr, "PF_rotate_bbox: invalid box hull handle\n");

	// set up the rotation matrix. the three orientation vectors form the
	// columns of the rotation matrix
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			ch->axis[i][j] = dir[j][i];
		}
	}
	// rotate the bounding box points
	for (i = 0; i < 3; i++) {
		mins[i] = DotProduct (ch->axis[i], mi);
		maxs[i] = DotProduct (ch->axis[i], ma);
	}
	// find all 8 corners of the rotated box
	VectorCopy (mins, v[0]);
	VectorCopy (maxs, v[1]);
	VectorSubtract (maxs, mins, d);
	for (i = 0; i < 3; i++) {
		vec3_t      x;

		l = DotProduct (d, dir[i]);
		VectorScale    (dir[i], l, x);
		VectorAdd      (mins, x, v[2 + i * 2]);
		VectorSubtract (maxs, x, v[3 + i * 2]);
	}
	// now find the aligned bounding box
	VectorCopy (v[0], ch->mins);
	VectorCopy (v[0], ch->maxs);
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 3; j++) {
			ch->mins[j] = min (ch->mins[j], v[i][j]);
			ch->maxs[j] = max (ch->maxs[j], v[i][j]);
		}
	}

	// set up the 3 size based hulls
	for (j = 0; j < 3; j++) {
		hull = ch->hulls[j];
		VectorScale (offsets[j][1], -1, hull->clip_mins);
		VectorScale (offsets[j][0], -1, hull->clip_maxs);
		// set up the clip planes
		for (i = 0; i < 6; i++) {
			hull->planes[i].dist = calc_dist (verts[i], dir[i / 2],
											  offsets[j]);
			hull->planes[i].type = 4;
			VectorCopy (dir[i / 2], hull->planes[i].normal);
		}
	}
}

// float () checkextension
static void
PF_checkextension (progs_t *pr)
{
	R_FLOAT (pr) = 0;			// FIXME: make this function actually useful
}

#define QF (PR_RANGE_QF << PR_RANGE_SHIFT) |

static builtin_t builtins[] = {
	{"makevectors",			PF_makevectors,			1},
	{"setorigin",			PF_setorigin,			2},
	{"setmodel",			PF_setmodel,			3},
	{"setsize",				PF_setsize,				4},

	{"sound",				PF_sound,				8},

	{"error",				PF_error,				10},
	{"objerror",			PF_objerror,			11},
	{"spawn",				PF_Spawn,				14},
	{"remove",				PF_Remove,				15},
	{"traceline",			PF_traceline,			16},
	{"checkclient",			PF_checkclient,			17},

	{"precache_sound",		PF_precache_sound,		19},
	{"precache_model",		PF_precache_model,		20},
	{"stuffcmd",			PF_stuffcmd,			21},
	{"findradius",			PF_findradius,			22},
	{"bprint",				PF_bprint,				23},
	{"sprint",				PF_sprint,				24},

	{"walkmove",			PF_walkmove,			32},

	{"droptofloor",			PF_droptofloor,			34},
	{"lightstyle",			PF_lightstyle,			35},

	{"checkbottom",			PF_checkbottom,			40},
	{"pointcontents",		PF_pointcontents,		41},

	{"aim",					PF_aim,					44},

	{"localcmd",			PF_localcmd,			46},

	{"particle",			PF_particle,			48},
	{"changeyaw",			PF_changeyaw,			49},

	{"writebyte",			PF_WriteByte,			52},
	{"WriteBytes",			PF_WriteBytes,			-1},
	{"writechar",			PF_WriteChar,			53},
	{"writeshort",			PF_WriteShort,			54},
	{"writelong",			PF_WriteLong,			55},
	{"writecoord",			PF_WriteCoord,			56},
	{"writeangle",			PF_WriteAngle,			57},
	{"WriteCoordV",			PF_WriteCoordV,			-1},
	{"WriteAngleV",			PF_WriteAngleV,			-1},
	{"writestring",			PF_WriteString,			58},
	{"writeentity",			PF_WriteEntity,			59},

	{"movetogoal",			SV_MoveToGoal,			67},
	{"precache_file",		PF_precache_file,		68},
	{"makestatic",			PF_makestatic,			69},
	{"changelevel",			PF_changelevel,			70},

	{"centerprint",			PF_centerprint,			73},
	{"ambientsound",		PF_ambientsound,		74},
	{"precache_model2",		PF_precache_model,		75},
	{"precache_sound2",		PF_precache_sound,		76},
	{"precache_file2",		PF_precache_file,		77},
	{"setspawnparms",		PF_setspawnparms,		78},

	{"testentitypos",		PF_testentitypos,		QF 92},
	{"hullpointcontents",	PF_hullpointcontents,	QF 93},
	{"getboxbounds",		PF_getboxbounds,		QF 94},
	{"getboxhull",			PF_getboxhull,			QF 95},
	{"freeboxhull",			PF_freeboxhull,			QF 96},
	{"rotate_bbox",			PF_rotate_bbox,			QF 97},
	{"tracebox",			PF_tracebox,			QF 98},
	{"checkextension",		PF_checkextension,		QF 99},

	{"EntityParseFunction", ED_EntityParseFunction,	-1},

	{0}
};

void
SV_PR_Cmds_Init ()
{
	RUA_Init (&sv_pr_state, 1);

	PR_Cmds_Init (&sv_pr_state);

	PR_RegisterBuiltins (&sv_pr_state, builtins);
}
