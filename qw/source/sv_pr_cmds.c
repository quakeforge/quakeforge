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

#include "qw/include/crudefile.h"
#include "qw/include/server.h"
#include "qw/include/sv_gib.h"
#include "qw/include/sv_progs.h"
#include "qw/include/sv_recorder.h"
#include "world.h"

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

	Sys_Error ("Program error");
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

	PR_RunError (pr, "object error");
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
	VectorCopy (min, SVvector (e, mins));
	VectorCopy (max, SVvector (e, maxs));
	VectorSubtract (max, min, SVvector (e, size));
	SV_LinkEdict (e, false);
}

/*
	PF_setmodel

	setmodel (entity, model)
	// void (entity e, string m) setmodel
	Also sets size, mins, and maxs for inline bmodels
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

	bprint (value)
	// void (string s) bprint
*/
static void
PF_bprint (progs_t *pr)
{
	const char *s;
	int         level;

	level = P_FLOAT (pr, 0);

	s = PF_VarString (pr, 1);
	SV_BroadcastPrintf (level, "%s", s);
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
	int         entnum, level;

	entnum = P_EDICTNUM (pr, 0);
	level = P_FLOAT (pr, 1);

	if (entnum < 1 || entnum > MAX_CLIENTS) {
		Sys_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum - 1];

	if (client->state == cs_server) //FIXME record to mvd?
		return;

	s = PF_VarString (pr, 2);

	SV_ClientPrintf (1, client, level, "%s", s);
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

	if (entnum < 1 || entnum > MAX_CLIENTS) {
		Sys_Printf ("tried to sprint to a non-client\n");
		return;
	}

	cl = &svs.clients[entnum - 1];

	if (cl->state == cs_server) //FIXME record to mvd?
		return;

	s = PF_VarString (pr, 1);

	MSG_ReliableWrite_Begin (&cl->backbuf, svc_centerprint, 2 + strlen (s));
	MSG_ReliableWrite_String (&cl->backbuf, s);

	if (sv.recorders) {
		sizebuf_t  *dbuf;
		dbuf = SVR_WriteBegin (dem_single, entnum - 1, 2 + strlen (s));
		MSG_WriteByte (dbuf, svc_centerprint);
		MSG_WriteString (dbuf, s);
	}
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

	if (sv_antilag->int_val == 2)
		nomonsters |= MOVE_LAGGED;

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

set_t *checkpvs;

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
	char       *buf, *p;
	client_t   *cl;
	int         entnum;

	entnum = P_EDICTNUM (pr, 0);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError (pr, "Parm 0 not a client");

	cl = &svs.clients[entnum - 1];

	if (cl->state == cs_server) //FIXME record to mvd?
		return;

	str = P_GSTRING (pr, 1);

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
		char        t = p[1];
		int         len = (p - buf) + 2;
		p[1] = 0;
		MSG_ReliableWrite_Begin (&cl->backbuf, svc_stufftext, len + 1);
		MSG_ReliableWrite_String (&cl->backbuf, buf);
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, len + 1);
			MSG_WriteByte (dbuf, svc_stufftext);
			MSG_WriteString (dbuf, buf);
		}
		p[1] = t;
		strcpy (buf, p + 1);		// safe because this is a downward, in
									// buffer move
	}
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
	Cbuf_AddText (sv_cbuf, str);
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
		if (SVfloat (ent, solid) == SOLID_NOT
			&& !((int) SVfloat (ent, flags) & FL_FINDABLE_NONSOLID))
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

cvar_t *pr_double_remove;

// void (entity e) remove
static void
PF_Remove (progs_t *pr)
{
	edict_t    *ed;

	ed = P_EDICT (pr, 0);
	if (NUM_FOR_EDICT (pr, ed) < *pr->reserved_edicts) {
		if (pr_double_remove->int_val == 1) {
			PR_DumpState (pr);
			Sys_Printf ("Reserved entity remove\n");
		} else // == 2
			PR_RunError (pr, "Reserved entity remove\n");
	}
	if (ed->free && pr_double_remove->int_val) {
		if (pr_double_remove->int_val == 1) {
			PR_DumpState (pr);
			Sys_Printf ("Double entity remove\n");
		} else // == 2
			PR_RunError (pr, "Double entity remove\n");
	}
	ED_Free (pr, ed);
}

static void
PR_CheckEmptyString (progs_t *pr, const char *s)
{
	if (s[0] <= ' ')
		PR_RunError (pr, "Bad string");
}

static void
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

	for (i = 0; i < max; i++) {
		if (!cache[i]) {
			char *c = Hunk_Alloc (strlen (s) + 1);
			strcpy (c, s);
			cache[i] = c; // blah, const
			Sys_MaskPrintf (SYS_dev, "%s: %3d %s\n", func, i, s);
			return;
		}
		if (!strcmp (cache[i], s))
			return;
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
	do_precache (pr, sv.model_precache, MAX_MODELS, P_GSTRING (pr, 0),
				 "precache_model");
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

	for (j = 0, cl = svs.clients; j < MAX_CLIENTS; j++, cl++)
		if (cl->state == cs_spawned) {
			MSG_ReliableWrite_Begin (&cl->backbuf, svc_lightstyle,
									   strlen (val) + 3);
			MSG_ReliableWrite_Char (&cl->backbuf, style);
			MSG_ReliableWrite_String (&cl->backbuf, val);
		}
	if (sv.recorders) {
		sizebuf_t  *dbuf;
		dbuf = SVR_WriteBegin (dem_all, 0, strlen (val) + 3);
		MSG_WriteByte (dbuf, svc_lightstyle);
		MSG_WriteByte (dbuf, style);
		MSG_WriteString (dbuf, val);
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
	const char *noaim;
	edict_t    *ent, *check, *bestent;
	float       dist, bestdist, speed;
	float      *mins, *maxs, *org;
	int         i, j;
	trace_t     tr;
	vec3_t      start, dir, end, bestdir;

	if (sv_aim->value >= 1.0) {
		VectorCopy (*sv_globals.v_forward, R_VECTOR (pr));
		return;
	}

	ent = P_EDICT (pr, 0);
	// noaim option
	i = NUM_FOR_EDICT (pr, ent);
	if (i > 0 && i < MAX_CLIENTS) {
		noaim = Info_ValueForKey (svs.clients[i - 1].userinfo, "noaim");
		if (atoi (noaim) > 0) {
			VectorCopy (*sv_globals.v_forward, R_VECTOR (pr));
			return;
		}
	}

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
#define	MSG_MULTICAST	4				// for multicast ()

static __attribute__((pure)) sizebuf_t *
WriteDest (progs_t *pr)
{
	int         dest;

	dest = P_FLOAT (pr, 0);
	switch (dest) {
		case MSG_BROADCAST:
			return &sv.datagram;

		case MSG_ONE:
			Sys_Error ("Shouldn't be at MSG_ONE");

		case MSG_ALL:
			return &sv.reliable_datagram;

		case MSG_INIT:
			if (sv.state != ss_loading)
				PR_RunError (pr, "PF_Write_*: MSG_INIT can be written in only "
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

static __attribute__((pure)) client_t *
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

// void (float to, ...) WriteBytes
static void
PF_WriteBytes (progs_t *pr)
{
	int         i, p;
	int         count = pr->pr_argc - 1;
	byte        buf[MAX_PARMS];

	for (i = 0; i < count; i++) {
		p = P_FLOAT (pr, i + 1);
		buf[i] = p;
	}

	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, count);
			MSG_ReliableWrite_SZ (&cl->backbuf, buf, count);
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, count);
			SZ_Write (dbuf, buf, count);
		}
	} else {
		sizebuf_t  *msg = WriteDest (pr);
		SZ_Write (msg, buf, count);
	}
}

// void (float to, float f) WriteByte
static void
PF_WriteByte (progs_t *pr)
{
	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, 1);
			MSG_ReliableWrite_Byte (&cl->backbuf, P_FLOAT (pr, 1));
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, 1);
			MSG_WriteByte (dbuf, P_FLOAT (pr, 1));
		}
	} else
		MSG_WriteByte (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, float f) WriteChar
static void
PF_WriteChar (progs_t *pr)
{
	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, 1);
			MSG_ReliableWrite_Char (&cl->backbuf, P_FLOAT (pr, 1));
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, 1);
			MSG_WriteByte (dbuf, P_FLOAT (pr, 1));
		}
	} else
		MSG_WriteByte (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, float f) WriteShort
static void
PF_WriteShort (progs_t *pr)
{
	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, 2);
			MSG_ReliableWrite_Short (&cl->backbuf, P_FLOAT (pr, 1));
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, 2);
			MSG_WriteShort (dbuf, P_FLOAT (pr, 1));
		}
	} else
		MSG_WriteShort (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, float f) WriteLong
static void
PF_WriteLong (progs_t *pr)
{
	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, 4);
			MSG_ReliableWrite_Long (&cl->backbuf, P_FLOAT (pr, 1));
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, 4);
			MSG_WriteLong (dbuf, P_FLOAT (pr, 1));
		}
	} else
		MSG_WriteLong (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, float f) WriteAngle
static void
PF_WriteAngle (progs_t *pr)
{
	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, 1);
			MSG_ReliableWrite_Angle (&cl->backbuf, P_FLOAT (pr, 1));
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, 1);
			MSG_WriteAngle (dbuf, P_FLOAT (pr, 1));
		}
	} else
		MSG_WriteAngle (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, float f) WriteCoord
static void
PF_WriteCoord (progs_t *pr)
{
	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, 2);
			MSG_ReliableWrite_Coord (&cl->backbuf, P_FLOAT (pr, 1));
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, 2);
			MSG_WriteCoord (dbuf, P_FLOAT (pr, 1));
		}
	} else
		MSG_WriteCoord (WriteDest (pr), P_FLOAT (pr, 1));
}

// void (float to, vector v) WriteAngleV
static void
PF_WriteAngleV (progs_t *pr)
{
	float      *ang = P_VECTOR (pr, 1);

	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, 1);
			MSG_ReliableWrite_AngleV (&cl->backbuf, ang);
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, 1);
			MSG_WriteAngleV (dbuf, ang);
		}
	} else
		MSG_WriteAngleV (WriteDest (pr), ang);
}

// void (float to, vector v) WriteCoordV
static void
PF_WriteCoordV (progs_t *pr)
{
	float      *coord = P_VECTOR (pr, 1);

	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, 2);
			MSG_ReliableWrite_CoordV (&cl->backbuf, coord);
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, 2);
			MSG_WriteCoordV (dbuf, coord);
		}
	} else
		MSG_WriteCoordV (WriteDest (pr), coord);
}

// void (float to, string s) WriteString
static void
PF_WriteString (progs_t *pr)
{
	const char *str = P_GSTRING (pr, 1);

	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, 1 + strlen (str));
			MSG_ReliableWrite_String (&cl->backbuf, str);
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, 1 + strlen (str));
			MSG_WriteString (dbuf, str);
		}
	} else
		MSG_WriteString (WriteDest (pr), str);
}

// void (float to, entity s) WriteEntity
static void
PF_WriteEntity (progs_t *pr)
{
	int         ent = P_EDICTNUM (pr, 1);

	if (P_FLOAT (pr, 0) == MSG_ONE) {
		client_t   *cl = Write_GetClient (pr);

		if (cl->state != cs_server) {
			MSG_ReliableCheckBlock (&cl->backbuf, 2);
			MSG_ReliableWrite_Short (&cl->backbuf, ent);
		}
		if (sv.recorders) {
			sizebuf_t  *dbuf;
			dbuf = SVR_WriteBegin (dem_single, cl - svs.clients, 2);
			MSG_WriteShort (dbuf, ent);
		}
	} else
		MSG_WriteShort (WriteDest (pr), ent);
}

// void (entity e) makestatic
static void
PF_makestatic (progs_t *pr)
{
	const char *model;
	edict_t    *ent;

	ent = P_EDICT (pr, 0);

	// flush the signon message out to a separate buffer if nearly full
	SV_FlushSignon ();

	MSG_WriteByte (&sv.signon, svc_spawnstatic);

	model = PR_GetString (pr, SVstring (ent, model));
	MSG_WriteByte (&sv.signon, SV_ModelIndex (model));

	MSG_WriteByte (&sv.signon, SVfloat (ent, frame));
	MSG_WriteByte (&sv.signon, SVfloat (ent, colormap));
	MSG_WriteByte (&sv.signon, SVfloat (ent, skin));

	MSG_WriteCoordAngleV (&sv.signon, SVvector (ent, origin),
						  SVvector (ent, angles));

	// throw the entity away now
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
	if (i < 1 || i > MAX_CLIENTS)
		PR_RunError (pr, "Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i - 1);

	if (client->state == cs_server)
		return;

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		sv_globals.parms[i] = client->spawn_parms[i];
}

// void (string s) changelevel
static void
PF_changelevel (progs_t *pr)
{
	const char *s;
	static int  last_spawncount;

	// make sure we don't issue two changelevels
	if (svs.spawncount == last_spawncount)
		return;
	last_spawncount = svs.spawncount;

	s = P_GSTRING (pr, 0);
	Cbuf_AddText (sv_cbuf, va (0, "map %s\n", s));
}

/*
	PF_logfrag

	logfrag (killer, killee)
	// void (entity killer, entity killee) logfrag
*/
static void
PF_logfrag (progs_t *pr)
{
	const char *s;
	edict_t    *ent1, *ent2;
	int         e1, e2;

	ent1 = P_EDICT (pr, 0);
	ent2 = P_EDICT (pr, 1);

	e1 = NUM_FOR_EDICT (pr, ent1);
	e2 = NUM_FOR_EDICT (pr, ent2);

	// do gib event callback
	if (sv_frag_e->func) {
		char buf[16];
		char type1[2], type2[2];
		int u1, u2;

		type1[1] = type2[1] = 0;
		if (e1 < 1 || e1 > MAX_CLIENTS ||
			svs.clients[e1 - 1].state == cs_server) {
			type1[0] = 'e';
			u1 = e1;
		} else {
			type1[0] = 'c';
			u1 = svs.clients[e1 - 1].userid;
		}

		if (e2 < 1 || e2 > MAX_CLIENTS ||
			svs.clients[e1 - 1].state == cs_server) {
			type2[0] = 'e';
			u2 = e2;
		} else {
			type2[0] = 'c';
			u2 = svs.clients[e2 - 1].userid;
		}

		snprintf(buf, sizeof(buf), "%d", u2);

		GIB_Event_Callback (sv_frag_e, 4, type1, va (0, "%d", u1), type2, buf);
	}

	if (e1 < 1 || e1 > MAX_CLIENTS || e2 < 1 || e2 > MAX_CLIENTS)
		return;

	s = va (0, "\\%s\\%s\\\n", svs.clients[e1 - 1].name,
			svs.clients[e2 - 1].name);

	SZ_Print (&svs.log[svs.logsequence & 1], s);
	if (sv_fraglogfile) {
		Qprintf (sv_fraglogfile, "%s", s);
		Qflush (sv_fraglogfile);
	}
}

/*
	PF_infokey

	string (entity e, string key) infokey
	// string (entity e, string key) infokey
*/
static void
PF_infokey (progs_t *pr)
{
	const char *key, *value;
	edict_t    *e;
	int         e1;

	e = P_EDICT (pr, 0);
	e1 = NUM_FOR_EDICT (pr, e);
	key = P_GSTRING (pr, 1);

	if (sv_hide_version_info->int_val
		&& (strequal (key, "*qf_version")
			|| strequal (key, "*qsg_version")
			|| strequal (key, "no_pogo_stick"))) {
		e1 = -1;
	}

	if (e1 == 0) {
		if ((value = Info_ValueForKey (svs.info, key)) == NULL || !*value)
			value = Info_ValueForKey (localinfo, key);
	} else if (e1 > 0 && e1 <= MAX_CLIENTS
			   && svs.clients[e1 - 1].userinfo) {
		if (!strcmp (key, "ip"))
			value = NET_BaseAdrToString (svs.clients[e1 - 1].netchan.
										 remote_address);
		else if (!strcmp (key, "ping")) {
			int         ping = SV_CalcPing (&svs.clients[e1 - 1]);

			value = va (0, "%d", ping);
		} else
			value = Info_ValueForKey (svs.clients[e1 - 1].userinfo, key);
	} else
		value = "";

	RETURN_STRING (pr, value);
}

/*
	PF_multicast

	void (vector where, float set) multicast
	// void (vector where, float set) multicast
*/
static void
PF_multicast (progs_t *pr)
{
	float      *o;
	int         to;

	o = P_VECTOR (pr, 0);
	to = P_FLOAT (pr, 1);

	SV_Multicast (o, to);
}

/*
	PF_cfopen

	float (string path, string mode) cfopen
	// float (string path, string mode) cfopen
*/
static void
PF_cfopen (progs_t *pr)
{
	R_FLOAT (pr) = CF_Open (P_GSTRING (pr, 0), P_GSTRING (pr, 1));
}

/*
	PF_cfclose

	void (float desc) cfclose
	// void (float desc) cfclose
*/
static void
PF_cfclose (progs_t *pr)
{
	CF_Close ((int) P_FLOAT (pr, 0));
}

/*
	PF_cfread

	string (float desc) cfread
	// string (float desc) cfread
*/
static void
PF_cfread (progs_t *pr)
{
	RETURN_STRING (pr, CF_Read ((int) P_FLOAT (pr, 0)));
}

/*
	PF_cfwrite

	float (float desc, string buf) cfwrite
	// float (float desc, string buf) cfwrite
*/
static void
PF_cfwrite (progs_t *pr)
{
	R_FLOAT (pr) = CF_Write ((int) P_FLOAT (pr, 0), P_GSTRING (pr, 1));
}

/*
	PF_cfeof

	float () cfeof
	// float (float desc) cfeof
*/
static void
PF_cfeof (progs_t *pr)
{
	R_FLOAT (pr) = CF_EOF ((int) P_FLOAT (pr, 0));
}

/*
	PF_cfquota

	float () cfquota
	// float () cfquota
*/
static void
PF_cfquota (progs_t *pr)
{
	R_FLOAT (pr) = CF_Quota ();
}

// void (entity ent, string key, string value) setinfokey
static void
PF_setinfokey (progs_t *pr)
{
	edict_t    *edict = P_EDICT (pr, 0);
	int         e1 = NUM_FOR_EDICT (pr, edict);
	const char *key = P_GSTRING (pr, 1);
	const char *value = P_GSTRING (pr, 2);

	if (e1 == 0) {
		SV_SetLocalinfo (key, value);
	} else if (e1 <= MAX_CLIENTS) {
		SV_SetUserinfo (&svs.clients[e1 - 1], key, value);
	}
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
	R_FLOAT (pr) = 0;	 // FIXME: make this function actually useful :P
}

static void
PF_sv_cvar (progs_t *pr)
{
	const char      *str;

	str = P_GSTRING (pr, 0);

	if (sv_hide_version_info->int_val
		&& strequal (str, "sv_hide_version_info")) {
		R_FLOAT (pr) = 0;
	} else {
		R_FLOAT (pr) = Cvar_VariableValue (str);
	}
}

// entity () SV_AllocClient
static void
PF_SV_AllocClient (progs_t *pr)
{
	client_t   *cl = SV_AllocClient (0, 1);

	if (!cl) {
		R_var (pr, entity) = 0;
		return;
	}

	//XXX netchan? Netchan_Setup (&newcl->netchan, adr, qport, NC_READ_QPORT);
	cl->state = cs_server;
	cl->spectator = 0;
	cl->connection_started = realtime;
	RETURN_EDICT (pr, cl->edict);
}

// void (entity cl) SV_FreeClient
static void
PF_SV_FreeClient (progs_t *pr)
{
	int         entnum = P_EDICTNUM (pr, 0);
	client_t   *cl = svs.clients + entnum - 1;

	if (entnum < 1 || entnum > MAX_CLIENTS || cl->state != cs_server)
		PR_RunError (pr, "not a server client");

	if (cl->userinfo)
		Info_Destroy (cl->userinfo);
	cl->userinfo = 0;
	SV_FullClientUpdate (cl, &sv.reliable_datagram);
	cl->state = cs_free;

	//if (sv_client_disconnect_e->func)
	//	GIB_Event_Callback (sv_client_disconnect_e, 2, va (0, "%u", cl->userid),
	//						"server");
}

// void (entity cl, string userinfo) SV_SetUserinfo
static void
PF_SV_SetUserinfo (progs_t *pr)
{
	int         entnum = P_EDICTNUM (pr, 0);
	client_t   *cl = svs.clients + entnum - 1;
	const char *str = P_GSTRING (pr, 1);

	if (entnum < 1 || entnum > MAX_CLIENTS || cl->state != cs_server)
		PR_RunError (pr, "not a server client");

	cl->userinfo = Info_ParseString (str, 1023, !sv_highchars->int_val);
	cl->sendinfo = true;
	SV_ExtractFromUserinfo (cl);
}

// void (entity cl, integer ping) SV_SetPing
static void
PR_SV_SetPing (progs_t *pr)
{
	int         entnum = P_EDICTNUM (pr, 0);
	client_t   *cl = svs.clients + entnum - 1;

	if (entnum < 1 || entnum > MAX_CLIENTS || cl->state != cs_server)
		PR_RunError (pr, "not a server client");
	cl->ping = P_INT (pr, 1);
}

// void (entity cl, float secs, vector angles, vector move, integer buttons, integer impulse) SV_UserCmd
static void
PR_SV_UserCmd (progs_t *pr)
{
	usercmd_t   ucmd;
	int         entnum = P_EDICTNUM (pr, 0);
	client_t   *cl = svs.clients + entnum - 1;

	if (entnum < 1 || entnum > MAX_CLIENTS || cl->state != cs_server)
		PR_RunError (pr, "not a server client");

	host_client = cl;
	sv_player = host_client->edict;
	ucmd.msec = 1000 * P_FLOAT (pr, 1);
	VectorCopy (P_VECTOR (pr, 2), ucmd.angles);
	VectorCopy (P_VECTOR (pr, 3), &ucmd.forwardmove); //FIXME right order?
	ucmd.buttons = P_INT (pr, 4);
	ucmd.impulse = P_INT (pr, 5);
	cl->localtime = sv.time;
	SV_PreRunCmd ();
	SV_RunCmd (&ucmd, 0);
	SV_PostRunCmd ();
	cl->lastcmd = ucmd;
	cl->lastcmd.buttons = 0;	// avoid multiple fires on lag
}

// void (entity cl) SV_Spawn
static void
PR_SV_Spawn (progs_t *pr)
{
	int         entnum = P_EDICTNUM (pr, 0);
	client_t   *cl = svs.clients + entnum - 1;

	if (entnum < 1 || entnum > MAX_CLIENTS || cl->state != cs_server)
		PR_RunError (pr, "not a server client");

	SV_Spawn (cl);
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

	{"logfrag",				PF_logfrag,				79},
	{"infokey",				PF_infokey,				80},
	{"multicast",			PF_multicast,			82},

	{"testentitypos",		PF_testentitypos,		QF 92},
	{"hullpointcontents",	PF_hullpointcontents,	QF 93},
	{"getboxbounds",		PF_getboxbounds,		QF 94},
	{"getboxhull",			PF_getboxhull,			QF 95},
	{"freeboxhull",			PF_freeboxhull,			QF 96},
	{"rotate_bbox",			PF_rotate_bbox,			QF 97},
	{"tracebox",			PF_tracebox,			QF 98},
	{"checkextension",		PF_checkextension,		QF 99},
	{"setinfokey",			PF_setinfokey,			QF 102},
	{"cfopen",				PF_cfopen,				QF 103},
	{"cfclose",				PF_cfclose,				QF 104},
	{"cfread",				PF_cfread,				QF 105},
	{"cfwrite",				PF_cfwrite,				QF 106},
	{"cfeof",				PF_cfeof,				QF 107},
	{"cfquota",				PF_cfquota,				QF 108},

	{"SV_AllocClient",		PF_SV_AllocClient,		-1},
	{"SV_FreeClient",		PF_SV_FreeClient,		-1},
	{"SV_SetUserinfo",		PF_SV_SetUserinfo,		-1},
	{"SV_SetPing",			PR_SV_SetPing,			-1},
	{"SV_UserCmd",			PR_SV_UserCmd,			-1},
	{"SV_Spawn",			PR_SV_Spawn,			-1},

	{"EntityParseFunction", ED_EntityParseFunction,	-1},

	{0}
};

void
SV_PR_Cmds_Init ()
{
	builtin_t  *bi;

	RUA_Init (&sv_pr_state, 1);

	PR_Cmds_Init (&sv_pr_state);
	// (override standard builtin)
	// float (string s) cvar
	bi = PR_FindBuiltin (&sv_pr_state, "cvar");
	bi->proc = PF_sv_cvar;

	PR_RegisterBuiltins (&sv_pr_state, builtins);
}
