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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/clip_hull.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/va.h"

#include "compat.h"
#include "crudefile.h"
#include "server.h"
#include "sv_pr_cmds.h"
#include "sv_progs.h"
#include "world.h"

#define	RETURN_EDICT(p, e) ((p)->pr_globals[OFS_RETURN].integer_var = EDICT_TO_PROG(p, e))
#define	RETURN_STRING(p, s) ((p)->pr_globals[OFS_RETURN].integer_var = PR_SetString((p), s))

/* BUILT-IN FUNCTIONS */


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
	SV_Printf ("======SERVER ERROR in %s:\n%s\n",
				PR_GetString (pr, pr->pr_xfunction->s_name), s);
	ed = PROG_TO_EDICT (pr, *sv_globals.self);
	ED_Print (pr, ed);

	SV_Error ("Program error");
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
	SV_Printf ("======OBJECT ERROR in %s:\n%s\n",
				PR_GetString (pr, pr->pr_xfunction->s_name), s);
	ed = PROG_TO_EDICT (pr, *sv_globals.self);
	ED_Print (pr, ed);
	ED_Free (pr, ed);

	SV_Error ("Program error");
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
	VectorCopy (min, SVvector (e, mins));
	VectorCopy (max, SVvector (e, maxs));
	VectorSubtract (max, min, SVvector (e, size));
	SV_LinkEdict (e, false);
}

/*
	PF_setmodel

	setmodel(entity, model)
	Also sets size, mins, and maxs for inline bmodels
*/
void
PF_setmodel (progs_t *pr)
{
	edict_t    *e;
	const char *m, **check;
	int         i;
	model_t    *mod;

	e = G_EDICT (pr, OFS_PARM0);
	m = G_STRING (pr, OFS_PARM1);

	// check to see if model was properly precached
	for (i = 0, check = sv.model_precache; *check; i++, check++)
		if (!strcmp (*check, m))
			break;

	if (!*check)
		PR_RunError (pr, "no precache: %s\n", m);

	SVstring (e, model) = PR_SetString (pr, m);
	SVfloat (e, modelindex) = i;

	// if it is an inline model, get the size information for it
	if (m[0] == '*') {
		mod = Mod_ForName (m, true);
		VectorCopy (mod->mins, SVvector (e, mins));
		VectorCopy (mod->maxs, SVvector (e, maxs));
		VectorSubtract (mod->maxs, mod->mins, SVvector (e, size));
		SV_LinkEdict (e, false);
	}
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
	int         level;

	level = G_FLOAT (pr, OFS_PARM0);

	s = PF_VarString (pr, 1);
	SV_BroadcastPrintf (level, "%s", s);
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
	int         entnum, level;

	entnum = G_EDICTNUM (pr, OFS_PARM0);
	level = G_FLOAT (pr, OFS_PARM1);

	s = PF_VarString (pr, 2);

	if (entnum < 1 || entnum > MAX_CLIENTS) {
		SV_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum - 1];

	SV_ClientPrintf (client, level, "%s", s);
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
	client_t   *cl;
	int         entnum;

	entnum = G_EDICTNUM (pr, OFS_PARM0);
	s = PF_VarString (pr, 1);

	if (entnum < 1 || entnum > MAX_CLIENTS) {
		SV_Printf ("tried to sprint to a non-client\n");
		return;
	}

	cl = &svs.clients[entnum - 1];

	ClientReliableWrite_Begin (cl, svc_centerprint, 2 + strlen (s));
	ClientReliableWrite_String (cl, s);
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
	int         soundnum;

	pos = G_VECTOR (pr, OFS_PARM0);
	samp = G_STRING (pr, OFS_PARM1);
	vol = G_FLOAT (pr, OFS_PARM2);
	attenuation = G_FLOAT (pr, OFS_PARM3);

	// check to see if samp was properly precached
	for (soundnum = 0, check = sv.sound_precache; *check; check++, soundnum++)
		if (!strcmp (*check, samp))
			break;

	if (!*check) {
		SV_Printf ("no precache: %s\n", samp);
		return;
	}

	// add an svc_spawnambient command to the level signon packet
	MSG_WriteByte (&sv.signon, svc_spawnstaticsound);
	MSG_WriteCoordV (&sv.signon, pos);

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
	edict_t    *entity;
	float       attenuation;
	int         channel, volume;

	entity = G_EDICT (pr, OFS_PARM0);
	channel = G_FLOAT (pr, OFS_PARM1);
	sample = G_STRING (pr, OFS_PARM2);
	volume = G_FLOAT (pr, OFS_PARM3) * 255;
	attenuation = G_FLOAT (pr, OFS_PARM4);

	SV_StartSound (entity, channel, sample, volume, attenuation);
}

/*
	PF_traceline

	Used for use tracing and shot targeting
	Traces are blocked by bbox and exact bsp entityes, and also slide box
	entities if the tryents flag is set.

	traceline (vector1, vector2, tryents)
*/
void
PF_traceline (progs_t *pr)
{
	float      *v1, *v2;
	edict_t    *ent;
	int         nomonsters;
	trace_t     trace;

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

/*
  PF_checkmove

  Wrapper around SV_Move, this makes PF_movetoground and PF_traceline
  redundant.
*/
void
PF_checkmove (progs_t *pr)
{
	edict_t	*ent;
	float	*start, *end, *mins, *maxs;
	int		type;
	trace_t	trace;

	start = G_VECTOR (pr, OFS_PARM0);
	mins = G_VECTOR (pr, OFS_PARM1);
	maxs = G_VECTOR (pr, OFS_PARM2);
	end = G_VECTOR (pr, OFS_PARM3);
	type = G_FLOAT (pr, OFS_PARM4);
	ent = G_EDICT (pr, OFS_PARM5);

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
void
PF_checkpos (progs_t *pr)
{
}

byte        checkpvs[MAX_MAP_LEAFS / 8];

int
PF_newcheckclient (progs_t *pr, int check)
{
	byte       *pvs;
	edict_t    *ent;
	int         i;
	mleaf_t    *leaf;
	vec3_t      org;

	// cycle to the next one
	if (check < 1)
		check = 1;
	if (check > MAX_CLIENTS)
		check = MAX_CLIENTS;

	if (check == MAX_CLIENTS)
		i = 1;
	else
		i = check + 1;

	for (;; i++) {
		if (i == MAX_CLIENTS + 1)
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

#define	MAX_CHECK	16
int         c_invis, c_notvis;

/*
  PF_checkclient

  Returns a client (or object that has a client enemy) that would be a valid
  target.

  If there are more than one valid options, they are cycled each frame

  If (self.origin + self.viewofs) is not in the PVS of the current target, it
  is not returned at all.
*/
void
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

/*
	PF_stuffcmd

	Sends text over to the client's execution buffer

	stuffcmd (clientent, value)
*/
void
PF_stuffcmd (progs_t *pr)
{
	const char       *str;
	char       *buf, *p;
	client_t   *cl;
	int         entnum;

	entnum = G_EDICTNUM (pr, OFS_PARM0);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError (pr, "Parm 0 not a client");
	str = G_STRING (pr, OFS_PARM1);

	cl = &svs.clients[entnum - 1];

	buf = cl->stufftext_buf;
	if (strlen (buf) + strlen (str) >= MAX_STUFFTEXT)
		PR_RunError (pr, "stufftext buffer overflow");
	strcat (buf, str);

	if (!strcmp (buf, "disconnect\n")) {
		// so long and thanks for all the fish
		cl->drop = true;
		buf[0] = 0;
		return;
	}

	p = strrchr (buf, '\n');
	if (p) {
		char t = p[1];
		p[1] = 0;
		ClientReliableWrite_Begin (cl, svc_stufftext, 2 + p - buf);
		ClientReliableWrite_String (cl, buf);
		p[1] = t;
		strcpy (buf, p + 1);		// safe because this is a downward, in
									// buffer move
	}
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
	float      *eorigin, *emins, *emaxs;
	int         i, j;
	vec3_t      eorg, org;

	chain = (edict_t *) sv.edicts;

	VectorScale (G_VECTOR (pr, OFS_PARM0), 2, org);
	rad = G_FLOAT (pr, OFS_PARM1);
	// * 4 because the * 0.5 was removed from the emins-emaxs average
	rad *= 4 * rad;					// Square early, sqrt never

	ent = NEXT_EDICT (pr, sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, ent = NEXT_EDICT (pr, ent)) {
		if (ent->free)
			continue;
		if (SVfloat (ent, solid) == SOLID_NOT)
			continue;
		eorigin = SVvector (ent, origin);
		emins = SVvector (ent, mins);
		emaxs = SVvector (ent, maxs);

		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - 2 * eorigin[j] - emins[j] - emaxs[j];
		if (DotProduct (eorg, eorg) > rad)
			continue;

		SVentity (ent, chain) = EDICT_TO_PROG (pr, chain);
		chain = ent;
	}

	RETURN_EDICT (pr, chain);
}

void
PF_Spawn (progs_t *pr)
{
	edict_t    *ed;

	ed = ED_Alloc (pr);
	RETURN_EDICT (pr, ed);
}

cvar_t *pr_double_remove;

void
PF_Remove (progs_t *pr)
{
	edict_t    *ed;

	ed = G_EDICT (pr, OFS_PARM0);
	if (ed->free && pr_double_remove->int_val) {
		if (pr_double_remove->int_val == 1) {
			PR_DumpState (pr);
			SV_Printf ("Double entity remove\n");
		} else // == 2
			PR_RunError (pr, "Double entity remove\n");
	}
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
										// files with qcc, it does nothing
	G_INT (pr, OFS_RETURN) = G_INT (pr, OFS_PARM0);
}

void
PF_precache_sound (progs_t *pr)
{
	const char       *s;
	int         i;

	if (sv.state != ss_loading)
		PR_RunError (pr, "PF_Precache_*: Precache can only be done in spawn "
					 "functions");

	s = G_STRING (pr, OFS_PARM0);
	G_INT (pr, OFS_RETURN) = G_INT (pr, OFS_PARM0);
	PR_CheckEmptyString (pr, s);

	for (i = 0; i < MAX_SOUNDS; i++) {
		if (!sv.sound_precache[i]) {
			char *c = Hunk_Alloc (strlen (s) + 1);
			strcpy (c, s);
			sv.sound_precache[i] = c; // blah, const
			Con_DPrintf ("PF_precache_sound: %3d %s\n", i, s);
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
	const char *s;
	int         i;

	if (sv.state != ss_loading)
		PR_RunError
			(pr, "PF_Precache_model: Precache can only be done in spawn "
			 "functions");

	s = G_STRING (pr, OFS_PARM0);
	G_INT (pr, OFS_RETURN) = G_INT (pr, OFS_PARM0);
	PR_CheckEmptyString (pr, s);

	for (i = 0; i < MAX_MODELS; i++) {
		if (!sv.model_precache[i]) {
			char *c = Hunk_Alloc (strlen (s) + 1);
			strcpy (c, s);
			sv.model_precache[i] = c; // blah, const
			Con_DPrintf ("PF_precache_model: %3d %s\n", i, s);
			return;
		}
		if (!strcmp (sv.model_precache[i], s))
			return;
	}
	Con_DPrintf ("PF_precache_model: %s\n", s);
	PR_RunError (pr, "PF_precache_model: overflow");
}

/*
	PF_walkmove

	float(float yaw, float dist) walkmove
*/
void
PF_walkmove (progs_t *pr)
{
	dfunction_t *oldf;
	edict_t    *ent;
	float       yaw, dist;
	int         oldself;
	vec3_t      move;

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
	trace_t     trace;
	vec3_t      end;

	ent = PROG_TO_EDICT (pr, *sv_globals.self);

	VectorCopy (SVvector (ent, origin), end);
	end[2] -= 256;

	trace = SV_Move (SVvector (ent, origin), SVvector (ent, mins),
					 SVvector (ent, maxs), end, false, ent);

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
	const char       *val;
	client_t   *client;
	int         style, j;

	style = G_FLOAT (pr, OFS_PARM0);
	val = G_STRING (pr, OFS_PARM1);

	// change the string in sv
	sv.lightstyles[style] = val;

	// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
		if (client->state == cs_spawned) {
			ClientReliableWrite_Begin (client, svc_lightstyle,
									   strlen (val) + 3);
			ClientReliableWrite_Char (client, style);
			ClientReliableWrite_String (client, val);
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

void
PF_pointcontents (progs_t *pr)
{
	float      *v;

	v = G_VECTOR (pr, OFS_PARM0);

	G_FLOAT (pr, OFS_RETURN) = SV_PointContents (v);
}

cvar_t     *sv_aim;

/*
	PF_aim

	Pick a vector for the player to shoot along
	vector aim(entity, missilespeed)
*/
void
PF_aim (progs_t *pr)
{
	edict_t    *ent, *check, *bestent;
	vec3_t      start, dir, end, bestdir;
	int         i, j;
	trace_t     tr;
	float       dist, bestdist;
	float       speed;
	const char       *noaim;

	ent = G_EDICT (pr, OFS_PARM0);
	speed = G_FLOAT (pr, OFS_PARM1);

	VectorCopy (SVvector (ent, origin), start);
	start[2] += 20;

	// noaim option
	i = NUM_FOR_EDICT (pr, ent);
	if (i > 0 && i < MAX_CLIENTS) {
		noaim = Info_ValueForKey (svs.clients[i - 1].userinfo, "noaim");
		if (atoi (noaim) > 0) {
			VectorCopy (*sv_globals.v_forward, G_VECTOR (pr, OFS_RETURN));
			return;
		}
	}
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
			&& SVfloat (ent, team) == SVfloat (check, team)) continue;
			// don't aim at teammate
		for (j = 0; j < 3; j++)
			end[j] = SVvector (check, origin)[j] + 0.5 *
				(SVvector (check, mins)[j] + SVvector (check, maxs)[j]);
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

/* MESSAGE WRITING */

#define	MSG_BROADCAST	0				// unreliable to all
#define	MSG_ONE			1				// reliable to one (msg_entity)
#define	MSG_ALL			2				// reliable to all
#define	MSG_INIT		3				// write to the init string
#define	MSG_MULTICAST	4				// for multicast()

sizebuf_t  *
WriteDest (progs_t *pr)
{
	int         dest;

	dest = G_FLOAT (pr, OFS_PARM0);
	switch (dest) {
	case MSG_BROADCAST:
		return &sv.datagram;

	case MSG_ONE:
		SV_Error ("Shouldn't be at MSG_ONE");
#if 0
		ent = PROG_TO_EDICT (pr, *sv_globals.msg_entity);
		entnum = NUM_FOR_EDICT (pr, ent);
		if (entnum < 1 || entnum > MAX_CLIENTS)
			PR_RunError (pr, "WriteDest: not a client");
		return &svs.clients[entnum - 1].netchan.message;
#endif

	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		if (sv.state != ss_loading)
			PR_RunError (pr, "PF_Write_*: MSG_INIT can only be written in "
						 "spawn functions");
		return &sv.signon;

	case MSG_MULTICAST:
		return &sv.multicast;

	default:
		PR_RunError (pr, "WriteDest: bad destination");
		break;
	}

	return NULL;
}

static client_t *
Write_GetClient (progs_t *pr)
{
	edict_t    *ent;
	int         entnum;

	ent = PROG_TO_EDICT (pr, *sv_globals.msg_entity);
	entnum = NUM_FOR_EDICT (pr, ent);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError (pr, "Write_GetClient: not a client");
	return &svs.clients[entnum - 1];
}

void
PF_WriteByte (progs_t *pr)
{
	if (G_FLOAT (pr, OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		ClientReliableCheckBlock (cl, 1);
		ClientReliableWrite_Byte (cl, G_FLOAT (pr, OFS_PARM1));
	} else
		MSG_WriteByte (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteChar (progs_t *pr)
{
	if (G_FLOAT (pr, OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		ClientReliableCheckBlock (cl, 1);
		ClientReliableWrite_Char (cl, G_FLOAT (pr, OFS_PARM1));
	} else
		MSG_WriteChar (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteShort (progs_t *pr)
{
	if (G_FLOAT (pr, OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		ClientReliableCheckBlock (cl, 2);
		ClientReliableWrite_Short (cl, G_FLOAT (pr, OFS_PARM1));
	} else
		MSG_WriteShort (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteLong (progs_t *pr)
{
	if (G_FLOAT (pr, OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		ClientReliableCheckBlock (cl, 4);
		ClientReliableWrite_Long (cl, G_FLOAT (pr, OFS_PARM1));
	} else
		MSG_WriteLong (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteAngle (progs_t *pr)
{
	if (G_FLOAT (pr, OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		ClientReliableCheckBlock (cl, 1);
		ClientReliableWrite_Angle (cl, G_FLOAT (pr, OFS_PARM1));
	} else
		MSG_WriteAngle (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteCoord (progs_t *pr)
{
	if (G_FLOAT (pr, OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		ClientReliableCheckBlock (cl, 2);
		ClientReliableWrite_Coord (cl, G_FLOAT (pr, OFS_PARM1));
	} else
		MSG_WriteCoord (WriteDest (pr), G_FLOAT (pr, OFS_PARM1));
}

void
PF_WriteString (progs_t *pr)
{
	if (G_FLOAT (pr, OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		ClientReliableCheckBlock (cl, 1 + strlen (G_STRING (pr, OFS_PARM1)));
		ClientReliableWrite_String (cl, G_STRING (pr, OFS_PARM1));
	} else
		MSG_WriteString (WriteDest (pr), G_STRING (pr, OFS_PARM1));
}

void
PF_WriteEntity (progs_t *pr)
{
	if (G_FLOAT (pr, OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		ClientReliableCheckBlock (cl, 2);
		ClientReliableWrite_Short (cl, G_EDICTNUM (pr, OFS_PARM1));
	} else
		MSG_WriteShort (WriteDest (pr), G_EDICTNUM (pr, OFS_PARM1));
}

int         SV_ModelIndex (const char *name);

void
PF_makestatic (progs_t *pr)
{
	const char *model;
	edict_t    *ent;

	ent = G_EDICT (pr, OFS_PARM0);

	MSG_WriteByte (&sv.signon, svc_spawnstatic);

	model = PR_GetString (pr, SVstring (ent, model));
//	SV_Printf ("Model: %d %s\n", SVstring (ent, model), model);
	MSG_WriteByte (&sv.signon, SV_ModelIndex (model));

	MSG_WriteByte (&sv.signon, SVfloat (ent, frame));
	MSG_WriteByte (&sv.signon, SVfloat (ent, colormap));
	MSG_WriteByte (&sv.signon, SVfloat (ent, skin));

	MSG_WriteCoordAngleV (&sv.signon, SVvector (ent, origin),
						  SVvector (ent, angles));
	// throw the entity away now
	ED_Free (pr, ent);
}

void
PF_setspawnparms (progs_t *pr)
{
	client_t   *client;
	edict_t    *ent;
	int         i;

	ent = G_EDICT (pr, OFS_PARM0);
	i = NUM_FOR_EDICT (pr, ent);
	if (i < 1 || i > MAX_CLIENTS)
		PR_RunError (pr, "Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i - 1);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		sv_globals.parms[i] = client->spawn_parms[i];
}

void
PF_changelevel (progs_t *pr)
{
	const char       *s;
	static int  last_spawncount;

	// make sure we don't issue two changelevels
	if (svs.spawncount == last_spawncount)
		return;
	last_spawncount = svs.spawncount;

	s = G_STRING (pr, OFS_PARM0);
	Cbuf_AddText (va ("map %s\n", s));
}

/*
	PF_logfrag

	logfrag (killer, killee)
*/
void
PF_logfrag (progs_t *pr)
{
	const char *s;
	edict_t    *ent1, *ent2;
	int         e1, e2;

	ent1 = G_EDICT (pr, OFS_PARM0);
	ent2 = G_EDICT (pr, OFS_PARM1);

	e1 = NUM_FOR_EDICT (pr, ent1);
	e2 = NUM_FOR_EDICT (pr, ent2);

	if (e1 < 1 || e1 > MAX_CLIENTS || e2 < 1 || e2 > MAX_CLIENTS)
		return;

	s = va ("\\%s\\%s\\\n", svs.clients[e1 - 1].name,
			svs.clients[e2 - 1].name);

	SZ_Print (&svs.log[svs.logsequence & 1], s);
	if (sv_fraglogfile) {
		Qprintf (sv_fraglogfile, s);
		Qflush (sv_fraglogfile);
	}
}

/*
	PF_infokey

	string(entity e, string key) infokey
*/
void
PF_infokey (progs_t *pr)
{
	const char *key, *value;
	static char ov[256];
	edict_t    *e;
	int         e1;

	e = G_EDICT (pr, OFS_PARM0);
	e1 = NUM_FOR_EDICT (pr, e);
	key = G_STRING (pr, OFS_PARM1);

	if (sv_hide_version_info->int_val
		&& (strequal (key, "*qf_version")
			|| strequal (key, "*qsg_version")
			|| strequal (key, "no_pogo_stick"))) {
		e1 = -1;
	}

	if (e1 == 0) {
		if ((value = Info_ValueForKey (svs.info, key)) == NULL || !*value)
			value = Info_ValueForKey (localinfo, key);
	} else if (e1 > 0 && e1 <= MAX_CLIENTS) {
		if (!strcmp (key, "ip"))
			value =
				strcpy (ov,
						NET_BaseAdrToString (svs.clients[e1 - 1].netchan.
											 remote_address));
		else if (!strcmp (key, "ping")) {
			int         ping = SV_CalcPing (&svs.clients[e1 - 1]);

			snprintf (ov, sizeof (ov), "%d", ping);
			value = ov;
		} else
			value = Info_ValueForKey (svs.clients[e1 - 1].userinfo, key);
	} else
		value = "";

	RETURN_STRING (pr, value);
}

/*
	PF_multicast

	void(vector where, float set) multicast
*/
void
PF_multicast (progs_t *pr)
{
	float      *o;
	int         to;

	o = G_VECTOR (pr, OFS_PARM0);
	to = G_FLOAT (pr, OFS_PARM1);

	SV_Multicast (o, to);
}

/*
	PF_cfopen

	float(string path, string mode) cfopen
*/
void
PF_cfopen (progs_t *pr)
{
	G_FLOAT (pr, OFS_RETURN) = CF_Open (G_STRING (pr, OFS_PARM0),
										G_STRING (pr, OFS_PARM1));
}

/*
	PF_cfclose

	void (float desc) cfclose
*/
void
PF_cfclose (progs_t *pr)
{
	CF_Close ((int) G_FLOAT (pr, OFS_PARM0));
}

/*
	PF_cfread

	string (float desc) cfread
*/
void
PF_cfread (progs_t *pr)
{
	RETURN_STRING (pr, CF_Read((int) G_FLOAT (pr, OFS_PARM0)));
}

/*
	PF_cfwrite

	float (float desc, string buf) cfwrite
*/
void
PF_cfwrite (progs_t *pr)
{
	G_FLOAT (pr, OFS_RETURN) = CF_Write((int) G_FLOAT(pr, OFS_PARM0),
										G_STRING (pr, OFS_PARM1));
}

/*
	PF_cfeof

	float () cfeof
*/
void
PF_cfeof (progs_t *pr)
{
	G_FLOAT (pr, OFS_RETURN) = CF_EOF ((int) G_FLOAT(pr, OFS_PARM0));
}

/*
	PF_cfquota

	float () cfquota
*/
void
PF_cfquota (progs_t *pr)
{
	G_FLOAT (pr, OFS_RETURN) = CF_Quota();
}

void
PF_setinfokey (progs_t *pr)
{
	edict_t    *edict = G_EDICT (pr, OFS_PARM0);
	int         e1 = NUM_FOR_EDICT (pr, edict);
	const char *key = G_STRING (pr, OFS_PARM1);
	const char *value = G_STRING (pr, OFS_PARM2);

	if (e1 == 0) {
		if (*value)
			Info_SetValueForKey (localinfo, key, value,
								 !sv_highchars->int_val);
		else
			Info_RemoveKey (localinfo, key);
	} else if (e1 <= MAX_CLIENTS) {
		Info_SetValueForKey (svs.clients[e1 - 1].userinfo, key, value,
							 !sv_highchars->int_val);
		SV_ExtractFromUserinfo (&svs.clients[e1 - 1]);

		if (Info_FilterForKey (key, client_info_filters)) {
			MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
			MSG_WriteByte (&sv.reliable_datagram, e1 - 1);
			MSG_WriteString (&sv.reliable_datagram, key);
			MSG_WriteString (&sv.reliable_datagram,
							 Info_ValueForKey (svs.clients[e1 - 1].userinfo,
								 			   key));
		}
	}
}

static void
PF_testentitypos (progs_t *pr)
{
	edict_t    *ent = G_EDICT (pr, OFS_PARM0);
	ent = SV_TestEntityPosition (ent);
	RETURN_EDICT (pr, ent ? ent : sv.edicts);
}

#define MAX_PF_HULLS 64		// FIXME make dynamic?
clip_hull_t *pf_hull_list[MAX_PF_HULLS];

static void
PF_hullpointcontents (progs_t *pr)
{
	edict_t    *edict = G_EDICT (pr, OFS_PARM0);
	float      *mins = G_VECTOR (pr, OFS_PARM1);
	float      *maxs = G_VECTOR (pr, OFS_PARM2);
	float      *point = G_VECTOR (pr, OFS_PARM3);
	hull_t     *hull;
	vec3_t      offset;

	hull = SV_HullForEntity (edict, mins, maxs, offset);
	VectorSubtract (point, offset, offset);
	G_INT (pr, OFS_RETURN) = SV_HullPointContents (hull, 0, offset);
}

static void
PF_getboxbounds (progs_t *pr)
{
	clip_hull_t *ch;
	int          h = G_INT (pr, OFS_PARM0) - 1;

	if (h < 0 || h > MAX_PF_HULLS - 1 || !(ch = pf_hull_list[h]))
		PR_RunError (pr, "PF_getboxbounds: invalid box hull handle\n");

	if (G_INT (pr, OFS_PARM1)) {
		VectorCopy (ch->maxs, G_VECTOR (pr, OFS_RETURN));
	} else {
		VectorCopy (ch->mins, G_VECTOR (pr, OFS_RETURN));
	}
}

static void
PF_getboxhull (progs_t *pr)
{
	clip_hull_t *ch = 0;
	int          i;

	for (i = 0; i < MAX_PF_HULLS; i++) {
		if (!pf_hull_list[i]) {
			ch = MOD_Alloc_Hull (6, 6);
			break;
		}
	}

	if (ch) {
		pf_hull_list[i] = ch;
		G_INT (pr, OFS_RETURN) = i + 1;
		for (i = 0; i < MAX_MAP_HULLS; i++)
			SV_InitHull (ch->hulls[i], ch->hulls[i]->clipnodes,
						 ch->hulls[i]->planes);
	} else {
		G_INT (pr, OFS_RETURN) = 0;
	}
}

static void
PF_freeboxhull (progs_t *pr)
{
	int          h = G_INT (pr, OFS_PARM0) - 1;
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

static void
PF_rotate_bbox (progs_t *pr)
{
	clip_hull_t *ch;
	float       l;
	float      *mi = G_VECTOR (pr, OFS_PARM4);
	float      *ma = G_VECTOR (pr, OFS_PARM5);
	float      *dir[3] = {
					G_VECTOR (pr, OFS_PARM1),
					G_VECTOR (pr, OFS_PARM2),
					G_VECTOR (pr, OFS_PARM3),
				};

	hull_t     *hull;
	int         i, j;
	int         h = G_INT (pr, OFS_PARM0) - 1;

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
		//SV_Printf ("'%0.1f %0.1f %0.1f'\n", v[i][0], v[i][1], v[i][2]);
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
//			SV_Printf ("%f   %f %f %f\n",
//						hull->planes[i].dist,
//						hull->planes[i].normal[0], hull->planes[i].normal[1],
//						hull->planes[i].normal[2]);
		}
	}
}

void
PF_Fixme (progs_t *pr)
{
	PR_RunError (pr, "unimplemented bulitin");
}

void
PF_Checkextension (progs_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = 0; //FIXME make this function actually useful :P
}

static void
PF_sv_cvar (progs_t *pr)
{
	const char      *str;

	str = G_STRING (pr, OFS_PARM0);

	if (sv_hide_version_info->int_val
		&& strequal (str, "sv_hide_version_info")) {
		G_FLOAT (pr, OFS_RETURN) = 0;
	} else {
		G_FLOAT (pr, OFS_RETURN) = Cvar_VariableValue (str);
	}
}

void
SV_PR_Cmds_Init ()
{
	PR_Cmds_Init (&sv_pr_state);

	sv_pr_state.builtins[45].proc = 0;	// (override standard builtin)
	PR_AddBuiltin (&sv_pr_state, "cvar", PF_sv_cvar, 45);
		// float (string s) cvar

	PR_AddBuiltin (&sv_pr_state, "makevectors", PF_makevectors, 1);
		// void (entity e) makevectors = #1
	PR_AddBuiltin (&sv_pr_state, "setorigin", PF_setorigin, 2);
		// void (entity e, vector o) setorigin = #2
	PR_AddBuiltin (&sv_pr_state, "setmodel", PF_setmodel, 3);
		// void(entity e, string m) setmodel = #3
	PR_AddBuiltin (&sv_pr_state, "setsize", PF_setsize, 4);
		// void (entity e, vector min, vector max) setsize = #4
	PR_AddBuiltin (&sv_pr_state, "fixme", PF_Fixme, 5);
		// void (entity e, vector min, vector max) setabssize = #5
	PR_AddBuiltin (&sv_pr_state, "sound", PF_sound, 8);
		// void (entity e, float chan, string samp) sound = #8
	PR_AddBuiltin (&sv_pr_state, "error", PF_error, 10);
		// void (string e) error = #10
	PR_AddBuiltin (&sv_pr_state, "objerror", PF_objerror, 11);
		// void (string e) objerror = #11
	PR_AddBuiltin (&sv_pr_state, "spawn", PF_Spawn, 14);
		// entity () spawn = #14
	PR_AddBuiltin (&sv_pr_state, "remove", PF_Remove, 15);
		// void (entity e) remove = #15
	PR_AddBuiltin (&sv_pr_state, "traceline", PF_traceline, 16);
		// float (vector v1, vector v2, float tryents) traceline = #16
	PR_AddBuiltin (&sv_pr_state, "checkclient", PF_checkclient, 17);
		// entity () clientlist = #17
	PR_AddBuiltin (&sv_pr_state, "precache_sound", PF_precache_sound, 19);
		// void (string s) precache_sound = #19
	PR_AddBuiltin (&sv_pr_state, "precache_model", PF_precache_model, 20);
		// void (string s) precache_model = #20
	PR_AddBuiltin (&sv_pr_state, "stuffcmd", PF_stuffcmd, 21);
		// void (entity client, string s) stuffcmd = #21
	PR_AddBuiltin (&sv_pr_state, "findradius", PF_findradius, 22);
		// entity (vector org, float rad) findradius = #22
	PR_AddBuiltin (&sv_pr_state, "bprint", PF_bprint, 23);
		// void (string s) bprint = #23
	PR_AddBuiltin (&sv_pr_state, "sprint", PF_sprint, 24);
		// void (entity client, string s) sprint = #24
	PR_AddBuiltin (&sv_pr_state, "walkmove", PF_walkmove, 32);
		// float (float yaw, float dist) walkmove = #32
	// no #33
	PR_AddBuiltin (&sv_pr_state, "droptofloor", PF_droptofloor, 34);
		// float () droptofloor = #34
	PR_AddBuiltin (&sv_pr_state, "lightstyle", PF_lightstyle, 35);
		// void (float style, string value) lightstyle = #35
	// no #39
	PR_AddBuiltin (&sv_pr_state, "checkbottom", PF_checkbottom, 40);
		// float (entity e) checkbottom = #40
	PR_AddBuiltin (&sv_pr_state, "pointcontents", PF_pointcontents, 41);
		// float (vector v) pointcontents = #41
	// no #42
	PR_AddBuiltin (&sv_pr_state, "aim", PF_aim, 44);
		// vector (entity e, float speed) aim = #44
	PR_AddBuiltin (&sv_pr_state, "localcmd", PF_localcmd, 46);
		// void (string s) localcmd = #46
	// no #48
	PR_AddBuiltin (&sv_pr_state, "changeyaw", PF_changeyaw, 49);
		// void () ChangeYaw = #49
	// no #50
	PR_AddBuiltin (&sv_pr_state, "writebyte", PF_WriteByte, 52);
		// void (float to, float f) WriteByte = #52
	PR_AddBuiltin (&sv_pr_state, "writechar", PF_WriteChar, 53);
		// void (float to, float f) WriteChar = #53
	PR_AddBuiltin (&sv_pr_state, "writeshort", PF_WriteShort, 54);
		// void (float to, float f) WriteShort = #54
	PR_AddBuiltin (&sv_pr_state, "writelong", PF_WriteLong, 55);
		// void(float to, float f) WriteLong = #55
	PR_AddBuiltin (&sv_pr_state, "writecoord", PF_WriteCoord, 56);
		// void(float to, float f) WriteCoord = #56
	PR_AddBuiltin (&sv_pr_state, "writeangle", PF_WriteAngle, 57);
		// void(float to, float f) WriteAngle = #57
	PR_AddBuiltin (&sv_pr_state, "writestring", PF_WriteString, 58);
		// void(float to, string s) WriteString = #58
	PR_AddBuiltin (&sv_pr_state, "writeentity", PF_WriteEntity, 59);
		// void(float to, entity s) WriteEntity = #59
	// 60
	// 61
	// 62
	// 63
	// 64
	// 65
	// 66
	PR_AddBuiltin (&sv_pr_state, "movetogoal", SV_MoveToGoal, 67);
		// void (float step) movetogoal = #67
	PR_AddBuiltin (&sv_pr_state, "precache_file", PF_precache_file, 68);
		// string (string s) precache_file = #68
	PR_AddBuiltin (&sv_pr_state, "makestatic", PF_makestatic, 69);
		// void (entity e) makestatic = #69
	PR_AddBuiltin (&sv_pr_state, "changelevel", PF_changelevel, 70);
		// void (string s) changelevel = #70
	// 71
	PR_AddBuiltin (&sv_pr_state, "centerprint", PF_centerprint, 73);
		// void (...) centerprint = #73
	PR_AddBuiltin (&sv_pr_state, "ambientsound", PF_ambientsound, 74);
		// void (vector pos, string samp, float vol, float atten)
		//  ambientsound = #74
	PR_AddBuiltin (&sv_pr_state, "precache_model", PF_precache_model, 75);
		// string (string s) precache_model2 = #75
	PR_AddBuiltin (&sv_pr_state, "precache_sound", PF_precache_sound, 76);
		// string (string s) precache_sound2 = #76 precache_sound2 is different
		//  only for qcc
	PR_AddBuiltin (&sv_pr_state, "precache_file", PF_precache_file, 77);
		// string (string s) precache_file2 = #77
	PR_AddBuiltin (&sv_pr_state, "setspawnparms", PF_setspawnparms, 78);
		// void (entity e) setspawnparms = #78
	PR_AddBuiltin (&sv_pr_state, "logfrag", PF_logfrag, 79);
		// void (entity killer, entity killee) logfrag = #79
	PR_AddBuiltin (&sv_pr_state, "infokey", PF_infokey, 80);
		// string (entity e, string key) infokey = #80
	PR_AddBuiltin (&sv_pr_state, "multicast", PF_multicast, 82);
		// void (vector where, float set) multicast = #82
	// 83
	// 84
	// 85
	// 86
	// 87
	// 88
	// 89
	// 90
	// 91
	PR_AddBuiltin (&sv_pr_state, "testentitypos", PF_testentitypos, 92);
		// entity (entity ent) testentitypos = #92
	PR_AddBuiltin (&sv_pr_state, "hullpointcontents", PF_hullpointcontents,
				   93);	// integer (entity ent, vector point)
						// hullpointcontents = #93
	PR_AddBuiltin (&sv_pr_state, "getboxbounds", PF_getboxbounds, 94);
		// vector (integer hull, integer max) getboxbounds = #94
	PR_AddBuiltin (&sv_pr_state, "getboxhull", PF_getboxhull, 95);
		// integer () getboxhull = #95
	PR_AddBuiltin (&sv_pr_state, "freeboxhull", PF_freeboxhull, 96);
		// void (integer hull) freeboxhull = #96
	PR_AddBuiltin (&sv_pr_state, "rotate_bbox", PF_rotate_bbox, 97);
		// void (integer hull, vector right, vector forward, vector up,
		//		 vector mins, vector maxs) rotate_bbox = #97
	PR_AddBuiltin (&sv_pr_state, "checkmove", PF_checkmove, 98);
		// void (vector start, vector mins, vector maxs, vector end,
		//		 float type, entity passent) checkmove = #98
	PR_AddBuiltin (&sv_pr_state, "checkextension", PF_Checkextension, 99);
		// = #99
	PR_AddBuiltin (&sv_pr_state, "setinfokey", PF_setinfokey, 102);
		// void (entity ent, string key, string value) setinfokey = #102
	PR_AddBuiltin (&sv_pr_state, "cfopen", PF_cfopen, 103);
		// float (string path, string mode) cfopen = #103
	PR_AddBuiltin (&sv_pr_state, "cfclose", PF_cfclose, 104);
		// void (float desc) cfclose = #104
	PR_AddBuiltin (&sv_pr_state, "cfread", PF_cfread, 105);
		// string (float desc) cfread = #105
	PR_AddBuiltin (&sv_pr_state, "cfwrite", PF_cfwrite, 106);
		// float (float desc, string buf) cfwrite = #106
	PR_AddBuiltin (&sv_pr_state, "cfeof", PF_cfeof, 107);
		// float (float desc) cfeof = #107
	PR_AddBuiltin (&sv_pr_state, "cfquota", PF_cfquota, 108);
		// float () cfquota = #108
};
