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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif

#include "QF/cmd.h"
#include "compat.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/va.h"

#include "server.h"
#include "sv_pr_cmds.h"
#include "sv_progs.h"
#include "world.h"
#include "crudefile.h"

#define	RETURN_EDICT(p, e) ((p)->pr_globals[OFS_RETURN].integer_var = EDICT_TO_PROG(p, e))
#define	RETURN_STRING(p, s) ((p)->pr_globals[OFS_RETURN].integer_var = PR_SetString((p), s))

/*
						BUILT-IN FUNCTIONS
*/

const char *
PF_VarString (progs_t *pr, int first)
{
	int         i;
	int         len;
	char       *out;

	for (len = 0, i = first; i < pr->pr_argc; i++)
		len += strlen (G_STRING (pr, (OFS_PARM0 + i * 3)));
	out = Hunk_TempAlloc (len + 1);
	for (i = first; i < pr->pr_argc; i++)
		strcat (out, G_STRING (pr, (OFS_PARM0 + i * 3)));
	return out;
}


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
	VectorCopy (org, SVFIELD (e, origin, vector));
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
	VectorCopy (min, SVFIELD (e, mins, vector));
	VectorCopy (max, SVFIELD (e, maxs, vector));
	VectorSubtract (max, min, SVFIELD (e, size, vector));
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

	SVFIELD (e, model, string) = PR_SetString (pr, m);
	SVFIELD (e, modelindex, float) = i;

// if it is an inline model, get the size information for it
	if (m[0] == '*') {
		mod = Mod_ForName (m, true);
		VectorCopy (mod->mins, SVFIELD (e, mins, vector));
		VectorCopy (mod->maxs, SVFIELD (e, maxs, vector));
		VectorSubtract (mod->maxs, mod->mins, SVFIELD (e, size, vector));
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
	int         entnum;
	int         level;

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
	int         entnum;
	client_t   *cl;

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
	PF_normalize

	vector normalize(vector)
*/
void
PF_normalize (progs_t *pr)
{
	float      *value1;
	vec3_t      newvalue;
	float       new;

	value1 = G_VECTOR (pr, OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2] * value1[2];
	new = sqrt (new);

	if (new == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else {
		new = 1 / new;
		newvalue[0] = value1[0] * new;
		newvalue[1] = value1[1] * new;
		newvalue[2] = value1[2] * new;
	}

	VectorCopy (newvalue, G_VECTOR (pr, OFS_RETURN));
}

/*
	PF_vlen

	scalar vlen(vector)
*/
void
PF_vlen (progs_t *pr)
{
	float      *value1;
	float       new;

	value1 = G_VECTOR (pr, OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2] * value1[2];
	new = sqrt (new);

	G_FLOAT (pr, OFS_RETURN) = new;
}

/*
	PF_vectoyaw

	float vectoyaw(vector)
*/
void
PF_vectoyaw (progs_t *pr)
{
	float      *value1;
	float       yaw;

	value1 = G_VECTOR (pr, OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else {
		yaw = (int) (atan2 (value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT (pr, OFS_RETURN) = yaw;
}


/*
	PF_vectoangles

	vector vectoangles(vector)
*/
void
PF_vectoangles (progs_t *pr)
{
	float      *value1;
	float       forward;
	float       yaw, pitch;

	value1 = G_VECTOR (pr, OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0) {
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	} else {
		yaw = (int) (atan2 (value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0] * value1[0] + value1[1] * value1[1]);
		pitch = (int) (atan2 (value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	G_FLOAT (pr, OFS_RETURN + 0) = pitch;
	G_FLOAT (pr, OFS_RETURN + 1) = yaw;
	G_FLOAT (pr, OFS_RETURN + 2) = 0;
}

/*
	PF_Random

	Returns a number from 0<= num < 1

	random()
*/
void
PF_random (progs_t *pr)
{
	float       num;

	num = (rand () & 0x7fff) / ((float) 0x7fff);

	G_FLOAT (pr, OFS_RETURN) = num;
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
		SV_Printf ("no precache: %s\n", samp);
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

	SV_StartSound (entity, channel, sample, volume, attenuation);
}

/*
	PF_break

	break()
*/
void
PF_break (progs_t *pr)
{
	SV_Printf ("break statement\n");
	*(int *) -4 = 0;					// dump to debugger
//  PR_RunError (pr, "break statement");
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

/*
	PF_checkmove

	Wrapper around SV_Move, this makes PF_movetoground and PF_traceline
	redundant.

	checkmove (start, mins, maxs, end, type, passent)
*/
void
PF_checkmove (progs_t *pr)
{
	float	*start, *end, *mins, *maxs;
	trace_t	trace;
	int		type;
	edict_t	*ent;

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
		if (SVFIELD (ent, health, float) <= 0)
			continue;
		if ((int) SVFIELD (ent, flags, float) & FL_NOTARGET)
			continue;

		// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (SVFIELD (ent, origin, vector), SVFIELD (ent, view_ofs, vector), org);
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
	if (ent->free || SVFIELD (ent, health, float) <= 0) {
		RETURN_EDICT (pr, sv.edicts);
		return;
	}
// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT (pr, *sv_globals.self);
	VectorAdd (SVFIELD (self, origin, vector), SVFIELD (self, view_ofs, vector), view);
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
	client_t   *cl;
	char       *buf;
	char       *p;

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
	PF_cvar

	float cvar (string)
*/
void
PF_cvar (progs_t *pr)
{
	const char       *str;

	str = G_STRING (pr, OFS_PARM0);

	G_FLOAT (pr, OFS_RETURN) = Cvar_VariableValue (str);
}

/*
	PF_cvar_set

	float cvar (string)
*/
void
PF_cvar_set (progs_t *pr)
{
	const char       *var_name, *val;
	cvar_t     *var;

	var_name = G_STRING (pr, OFS_PARM0);
	val = G_STRING (pr, OFS_PARM1);
	var = Cvar_FindVar (var_name);
	if (!var)
		var = Cvar_FindAlias (var_name);
	if (!var) {
		// FIXME: make Con_DPrint?
		SV_Printf ("PF_cvar_set: variable %s not found\n", var_name);
		return;
	}

	Cvar_Set (var, val);
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
		if (SVFIELD (ent, solid, float) == SOLID_NOT)
			continue;
		for (j = 0; j < 3; j++)
			eorg[j] =
				org[j] - (SVFIELD (ent, origin, vector)[j] +
						  (SVFIELD (ent, mins, vector)[j] + SVFIELD (ent, maxs, vector)[j]) * 0.5);
		if (Length (eorg) > rad)
			continue;

		SVFIELD (ent, chain, entity) = EDICT_TO_PROG (pr, chain);
		chain = ent;
	}

	RETURN_EDICT (pr, chain);
}


/*
	PF_dprint
*/
void
PF_dprint (progs_t *pr)
{
	SV_Printf ("%s", PF_VarString (pr, 0));
}

char        pr_string_temp[128];

void
PF_ftos (progs_t *pr)
{
	float       v;
	int         i;						// 1999-07-25 FTOS fix by Maddes

	v = G_FLOAT (pr, OFS_PARM0);

	if (v == (int) v)
		snprintf (pr_string_temp, sizeof (pr_string_temp), "%d", (int) v);
	else
// 1999-07-25 FTOS fix by Maddes  start
	{
		snprintf (pr_string_temp, sizeof (pr_string_temp), "%1f", v);
		for (i = strlen (pr_string_temp) - 1;
			 i > 0 && pr_string_temp[i] == '0' && pr_string_temp[i - 1] != '.';
			 i--) {
			pr_string_temp[i] = 0;
		}
	}
// 1999-07-25 FTOS fix by Maddes  end
	G_INT (pr, OFS_RETURN) = PR_SetString (pr, pr_string_temp);
}

void
PF_fabs (progs_t *pr)
{
	float       v;

	v = G_FLOAT (pr, OFS_PARM0);
	G_FLOAT (pr, OFS_RETURN) = fabs (v);
}

void
PF_vtos (progs_t *pr)
{
	snprintf (pr_string_temp, sizeof (pr_string_temp), "'%5.1f %5.1f %5.1f'",
			  G_VECTOR (pr, OFS_PARM0)[0], G_VECTOR (pr, OFS_PARM0)[1],
			  G_VECTOR (pr, OFS_PARM0)[2]);
	G_INT (pr, OFS_RETURN) = PR_SetString (pr, pr_string_temp);
}

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


// entity (entity start, .string field, string match) find = #5;
void
PF_Find (progs_t *pr)
{
	int         e;
	int         f;
	const char       *s, *t;
	edict_t    *ed;

	e = G_EDICTNUM (pr, OFS_PARM0);
	f = G_INT (pr, OFS_PARM1);
	s = G_STRING (pr, OFS_PARM2);
	if (!s)
		PR_RunError (pr, "PF_Find: bad search string");

	for (e++; e < sv.num_edicts; e++) {
		ed = EDICT_NUM (pr, e);
		if (ed->free)
			continue;
		t = E_STRING (pr, ed, f);
		if (!t)
			continue;
		if (!strcmp (t, s)) {
			RETURN_EDICT (pr, ed);
			return;
		}
	}

	RETURN_EDICT (pr, sv.edicts);
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
			return;
		}
		if (!strcmp (sv.model_precache[i], s))
			return;
	}
	PR_RunError (pr, "PF_precache_model: overflow");
}


void
PF_coredump (progs_t *pr)
{
	ED_PrintEdicts (pr, "");
}

void
PF_traceon (progs_t *pr)
{
	pr->pr_trace = true;
}

void
PF_traceoff (progs_t *pr)
{
	pr->pr_trace = false;
}

void
PF_eprint (progs_t *pr)
{
	ED_PrintNum (pr, G_EDICTNUM (pr, OFS_PARM0));
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

	if (!((int) SVFIELD (ent, flags, float) & (FL_ONGROUND | FL_FLY | FL_SWIM))) {
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

	VectorCopy (SVFIELD (ent, origin, vector), end);
	end[2] -= 256;

	trace = SV_Move (SVFIELD (ent, origin, vector), SVFIELD (ent, mins, vector), SVFIELD (ent, maxs, vector), end, false, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT (pr, OFS_RETURN) = 0;
	else {
		VectorCopy (trace.endpos, SVFIELD (ent, origin, vector));
		SV_LinkEdict (ent, false);
		SVFIELD (ent, flags, float) = (int) SVFIELD (ent, flags, float) | FL_ONGROUND;
		SVFIELD (ent, groundentity, entity) = EDICT_TO_PROG (pr, trace.ent);
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
	const char       *val;
	client_t   *client;
	int         j;

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

void
PF_rint (progs_t *pr)
{
	float       f;

	f = G_FLOAT (pr, OFS_PARM0);
	if (f > 0)
		G_FLOAT (pr, OFS_RETURN) = (int) (f + 0.5);
	else
		G_FLOAT (pr, OFS_RETURN) = (int) (f - 0.5);
}

void
PF_floor (progs_t *pr)
{
	G_FLOAT (pr, OFS_RETURN) = floor (G_FLOAT (pr, OFS_PARM0));
}

void
PF_ceil (progs_t *pr)
{
	G_FLOAT (pr, OFS_RETURN) = ceil (G_FLOAT (pr, OFS_PARM0));
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
	PF_nextent

	entity nextent(entity)
*/
void
PF_nextent (progs_t *pr)
{
	int         i;
	edict_t    *ent;

	i = G_EDICTNUM (pr, OFS_PARM0);
	while (1) {
		i++;
		if (i == sv.num_edicts) {
			RETURN_EDICT (pr, sv.edicts);
			return;
		}
		ent = EDICT_NUM (pr, i);
		if (!ent->free) {
			RETURN_EDICT (pr, ent);
			return;
		}
	}
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
	const char       *noaim;

	ent = G_EDICT (pr, OFS_PARM0);
	speed = G_FLOAT (pr, OFS_PARM1);

	VectorCopy (SVFIELD (ent, origin, vector), start);
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
	if (tr.ent && SVFIELD (tr.ent, takedamage, float) == DAMAGE_AIM
		&& (!teamplay->int_val || SVFIELD (ent, team, float) <= 0
			|| SVFIELD (ent, team, float) != SVFIELD (tr.ent, team, float))) {
		VectorCopy (*sv_globals.v_forward, G_VECTOR (pr, OFS_RETURN));
		return;
	}

// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim->value;
	bestent = NULL;

	check = NEXT_EDICT (pr, sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, check = NEXT_EDICT (pr, check)) {
		if (SVFIELD (check, takedamage, float) != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay->int_val && SVFIELD (ent, team, float) > 0
			&& SVFIELD (ent, team, float) == SVFIELD (check, team, float)) continue;	// don't aim at
														// teammate
		for (j = 0; j < 3; j++)
			end[j] = SVFIELD (check, origin, vector)[j]
				+ 0.5 * (SVFIELD (check, mins, vector)[j] + SVFIELD (check, maxs, vector)[j]);
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
		VectorSubtract (SVFIELD (bestent, origin, vector), SVFIELD (ent, origin, vector), dir);
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
	current = anglemod (SVFIELD (ent, angles, vector)[1]);
	ideal = SVFIELD (ent, ideal_yaw, float);
	speed = SVFIELD (ent, yaw_speed, float);

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

	SVFIELD (ent, angles, vector)[1] = anglemod (current + move);
}

/*
	MESSAGE WRITING
*/

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
				PR_RunError
					(pr, "PF_Write_*: MSG_INIT can only be written in spawn functions");
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
	int         entnum;
	edict_t    *ent;

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

//=============================================================================

int         SV_ModelIndex (const char *name);

void
PF_makestatic (progs_t *pr)
{
	edict_t    *ent;
	int         i;
	const char       *model;

	ent = G_EDICT (pr, OFS_PARM0);

	MSG_WriteByte (&sv.signon, svc_spawnstatic);

	model = PR_GetString (pr, SVFIELD (ent, model, string));
//SV_Printf ("Model: %d %s\n", SVFIELD (ent, model, string), model);
	MSG_WriteByte (&sv.signon, SV_ModelIndex (model));

	MSG_WriteByte (&sv.signon, SVFIELD (ent, frame, float));
	MSG_WriteByte (&sv.signon, SVFIELD (ent, colormap, float));
	MSG_WriteByte (&sv.signon, SVFIELD (ent, skin, float));
	for (i = 0; i < 3; i++) {
		MSG_WriteCoord (&sv.signon, SVFIELD (ent, origin, vector)[i]);
		MSG_WriteAngle (&sv.signon, SVFIELD (ent, angles, vector)[i]);
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
	if (i < 1 || i > MAX_CLIENTS)
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
	edict_t    *ent1, *ent2;
	int         e1, e2;
	const char       *s;

	ent1 = G_EDICT (pr, OFS_PARM0);
	ent2 = G_EDICT (pr, OFS_PARM1);

	e1 = NUM_FOR_EDICT (pr, ent1);
	e2 = NUM_FOR_EDICT (pr, ent2);

	if (e1 < 1 || e1 > MAX_CLIENTS || e2 < 1 || e2 > MAX_CLIENTS)
		return;

	s = va ("\\%s\\%s\\\n", svs.clients[e1 - 1].name, svs.clients[e2 - 1].name);

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
	edict_t    *e;
	int         e1;
	const char       *value;
	const char       *key;
	static char ov[256];

	e = G_EDICT (pr, OFS_PARM0);
	e1 = NUM_FOR_EDICT (pr, e);
	key = G_STRING (pr, OFS_PARM1);

	if (e1 == 0) {
		if ((value = Info_ValueForKey (svs.info, key)) == NULL || !*value)
			value = Info_ValueForKey (localinfo, key);
	} else if (e1 <= MAX_CLIENTS) {
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
	PF_stof

	float(string s) stof
*/
void
PF_stof (progs_t *pr)
{
	const char       *s;

	s = G_STRING (pr, OFS_PARM0);

	G_FLOAT (pr, OFS_RETURN) = atof (s);
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
	PF_strlen

	float(string s) strlen
*/
void
PF_strlen (progs_t *pr)
{
	const char *s;

	s = G_STRING (pr, OFS_PARM0);
	G_FLOAT (pr, OFS_RETURN) = strlen(s);
}

/*
	PF_charcount

	float(string char, string s) charcount
*/
void
PF_charcount (progs_t *pr)
{
	const char *s;
	char goal;
	int count;

	goal = (G_STRING (pr, OFS_PARM0))[0];
	if (goal == '\0')
		G_FLOAT (pr, OFS_RETURN) = 0;

	count = 0;
	s = G_STRING (pr, OFS_PARM1);
	while (*s) {
		if (*s == goal)
			count++;
		s++;
	}
	G_FLOAT (pr, OFS_RETURN) = count;
}


/*
	PF_cfopen

	float(string path, string mode) cfopen
*/
void
PF_cfopen (progs_t *pr)
{
	G_FLOAT (pr, OFS_RETURN) = CF_Open (G_STRING (pr, OFS_PARM0), G_STRING (pr, OFS_PARM1));
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
	G_FLOAT (pr, OFS_RETURN) = CF_Write((int) G_FLOAT(pr, OFS_PARM0), G_STRING (pr, OFS_PARM1));
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
		Info_SetValueForKey (localinfo, key, value, MAX_LOCALINFO_STRING);
	} else if (e1 <= MAX_CLIENTS) {
		Info_SetValueForKey (svs.clients[e1 - 1].userinfo, key, value,
							 MAX_INFO_STRING);
		SV_ExtractFromUserinfo (&svs.clients[e1 - 1]);
		MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
		MSG_WriteByte (&sv.reliable_datagram, e1 - 1);
		MSG_WriteString (&sv.reliable_datagram, key);
		MSG_WriteString (&sv.reliable_datagram,
						 Info_ValueForKey (svs.clients[e1 - 1].userinfo, key));
	}
}


#define MAX_PF_HULLS 64		// FIXME make dynamic?
static int pf_hull_list_inited;
static hull_t *pf_free_hulls;
static dclipnode_t pf_clipnodes[MAX_PF_HULLS][6];
static mplane_t pf_planes[MAX_PF_HULLS][6];
hull_t pf_hull_list[MAX_PF_HULLS];

static void
PF_getboxhull (progs_t *pr)
{
	hull_t  *hull = pf_free_hulls;

	if (!pf_hull_list_inited) {
		int     i;
		pf_hull_list_inited = 1;
		for (i = 0; i < MAX_PF_HULLS - 1; i++)
			pf_hull_list[i].clipnodes = (dclipnode_t*)&pf_hull_list[i + 1];
		pf_hull_list[i].clipnodes = 0;
		pf_free_hulls = pf_hull_list;
	}

	if (hull) {
		pf_free_hulls = (hull_t*)hull->clipnodes;
		SV_InitHull (hull, pf_clipnodes[hull - pf_hull_list],
					 pf_planes[hull - pf_hull_list]);
	}
	G_INT (pr, OFS_RETURN) = (hull - pf_hull_list) + 1;
}

static void
PF_freeboxhull (progs_t *pr)
{
	int      h = G_INT (pr, OFS_PARM0) - 1;
	hull_t  *hull = &pf_hull_list[h];

	if (h < 0 || h > MAX_PF_HULLS - 1
		|| hull->clipnodes != pf_clipnodes[h]
		|| hull->planes != pf_planes[h])
		PR_RunError (pr, "PF_freeboxhull: invalid box hull handle\n");
	hull->clipnodes = (dclipnode_t*)pf_free_hulls;
	hull->planes = 0;
	pf_free_hulls = hull;
}

static void
PF_rotate_bbox (progs_t *pr)
{
	int      h = G_INT (pr, OFS_PARM0) - 1;
	float   *angles = G_VECTOR (pr, OFS_PARM1);
	float   *mins = G_VECTOR (pr, OFS_PARM2);
	float   *maxs = G_VECTOR (pr, OFS_PARM3);
	vec3_t   f, r, u;
	hull_t  *hull = &pf_hull_list[h];

	AngleVectors (angles, f, r, u);

	hull->planes[0].dist = DotProduct (f, maxs);
	hull->planes[0].type = 4;
	VectorCopy (f, hull->planes[0].normal);

	hull->planes[1].dist = DotProduct (f, mins);
	hull->planes[1].type = 4;
	VectorCopy (f, hull->planes[1].normal);

	hull->planes[2].dist = DotProduct (r, maxs);
	hull->planes[2].type = 4;
	VectorCopy (r, hull->planes[2].normal);

	hull->planes[3].dist = DotProduct (r, mins);
	hull->planes[3].type = 4;
	VectorCopy (r, hull->planes[3].normal);

	hull->planes[4].dist = DotProduct (u, maxs);
	hull->planes[4].type = 4;
	VectorCopy (u, hull->planes[4].normal);

	hull->planes[5].dist = DotProduct (u, mins);
	hull->planes[5].type = 4;
	VectorCopy (u, hull->planes[5].normal);
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

builtin_t   sv_builtins[] = {
	PF_Fixme,
	PF_makevectors,		// void(entity e) makevectors = #1;
	PF_setorigin,		// void(entity e, vector o) setorigin = #2;
	PF_setmodel,		// void(entity e, string m) setmodel = #3;
	PF_setsize,			// void(entity e, vector min, vector max) setsize = #4;
	PF_Fixme,			// void(entity e, vector min, vector max) setabssize = #5;
	PF_break,			// void() break = #6;
	PF_random,			// float() random = #7;
	PF_sound,			// void(entity e, float chan, string samp) sound = #8;
	PF_normalize,		// vector(vector v) normalize = #9;
	PF_error,			// void(string e) error = #10;
	PF_objerror,		// void(string e) objerror = #11;
	PF_vlen,			// float(vector v) vlen = #12;
	PF_vectoyaw,		// float(vector v) vectoyaw = #13;
	PF_Spawn,			// entity() spawn = #14;
	PF_Remove,			// void(entity e) remove = #15;
	PF_traceline,		// float(vector v1, vector v2, float tryents) traceline = #16;
	PF_checkclient,		// entity() clientlist = #17;
	PF_Find,			// entity(entity start, .string fld, string match) find = #18;
	PF_precache_sound,	// void(string s) precache_sound = #19;
	PF_precache_model,	// void(string s) precache_model = #20;
	PF_stuffcmd,		// void(entity client, string s)stuffcmd = #21;
	PF_findradius,		// entity(vector org, float rad) findradius = #22;
	PF_bprint,			// void(string s) bprint = #23;
	PF_sprint,			// void(entity client, string s) sprint = #24;
	PF_dprint,			// void(string s) dprint = #25;
	PF_ftos,			// void(string s) ftos = #26;
	PF_vtos,			// void(string s) vtos = #27;
	PF_coredump,		// void() coredump = #28
	PF_traceon,			// void() traceon = #29
	PF_traceoff,		// void() traceoff = #30
	PF_eprint,			// void(entity e) = #31 debug print an entire entity
	PF_walkmove,		// float(float yaw, float dist) walkmove = #32
	PF_Fixme,			// 33
	PF_droptofloor,		// float() droptofloor= #34
	PF_lightstyle,		// void(float style, string value) lightstyle = #35
	PF_rint,			// float(float v) rint = #36
	PF_floor,			// float(float v) floor = #37
	PF_ceil,			// float(float v) ceil = #38
	PF_Fixme,			// 39
	PF_checkbottom,		// float(entity e) checkbottom = #40
	PF_pointcontents,	// float(vector v) pointcontents = #41
	PF_Fixme,			// 42
	PF_fabs,			// float(float f) fabs = #43
	PF_aim,				// vector(entity e, float speed) aim = #44
	PF_cvar,			// float(string s) cvar = #45
	PF_localcmd,		// void(string s) localcmd = #46
	PF_nextent,			// entity(entity e) nextent = #47
	PF_Fixme,			// 48
	PF_changeyaw,		// void() ChangeYaw = #49
	PF_Fixme,			// 50
	PF_vectoangles,		// vector(vector v) vectoangles = #51

	PF_WriteByte,		// void(float to, float f) WriteByte = #52
	PF_WriteChar,		// void(float to, float f) WriteChar = #53
	PF_WriteShort,		// void(float to, float f) WriteShort = #54
	PF_WriteLong,		// void(float to, float f) WriteLong = #55
	PF_WriteCoord,		// void(float to, float f) WriteCoord = #56
	PF_WriteAngle,		// void(float to, float f) WriteAngle = #57
	PF_WriteString,		// void(float to, string s) WriteString = #58
	PF_WriteEntity,		// void(float to, entity s) WriteEntity = #59

	PF_Fixme,			// 60
	PF_Fixme,			// 61
	PF_Fixme,			// 62
	PF_Fixme,			// 63
	PF_Fixme,			// 64
	PF_Fixme,			// 65
	PF_Fixme,			// 66

	SV_MoveToGoal,		// void(float step) movetogoal = #67
	PF_precache_file,	// string(string s) precache_file = #68
	PF_makestatic,		// void(entity e) makestatic = #69

	PF_changelevel,		// void(string s) changelevel = #70
	PF_Fixme,			// 71

	PF_cvar_set,		// void(string var, string val) cvar_set = #72
	PF_centerprint,		// void(...) centerprint = #73

	PF_ambientsound,	// void(vector pos, string samp, float vol, float atten) ambientsound = #74

	PF_precache_model,	// string(string s) precache_model2 = #75
	PF_precache_sound,	// string(string s) precache_sound2 = #76 precache_sound2 is different only for qcc
	PF_precache_file,	// string(string s) precache_file2 = #77

	PF_setspawnparms,	// void(entity e) setspawnparms = #78

	PF_logfrag,			// void(entity killer, entity killee) logfrag = #79

	PF_infokey,			// string(entity e, string key) infokey = #80
	PF_stof,			// float(string s) stof = #81
	PF_multicast,		// void(vector where, float set) multicast = #82

	PF_Fixme,			// 83
	PF_Fixme,			// 84
	PF_Fixme,			// 85
	PF_Fixme,			// 86
	PF_Fixme,			// 87
	PF_Fixme,			// 88
	PF_Fixme,			// 89
	PF_Fixme,			// 90
	PF_Fixme,			// 91
	PF_Fixme,			// 92
	PF_Fixme,			// 93
	PF_Fixme,			// 94
	PF_getboxhull,		// integer () getboxhull = #95
	PF_freeboxhull,		// void (integer hull) freeboxhull = #96
	PF_rotate_bbox,		// void (integer hull, vector angles, vector mins, vector maxs) rotate_bbox = #97
	PF_checkmove,		// void (vector start, vector mins, vector maxs, vector end, float type, entity passent) checkmove = #98
	PF_Checkextension,	// = #99
	PF_strlen,			// = #100
	PF_charcount,		// = #101
	PF_setinfokey,		// void (entity ent, string key, string value) setinfokey = #102
	PF_cfopen,			// float (string path, string mode) cfopen = #103
	PF_cfclose,			// void (float desc) cfclose = #104
	PF_cfread,			// string (float desc) cfread = #105
	PF_cfwrite,			// float (float desc, string buf) cfwrite = #106
	PF_cfeof,			// float (float desc) cfeof = #107
	PF_cfquota,			// float () cfquota = #108
};

int         sv_numbuiltins = sizeof (sv_builtins) / sizeof (sv_builtins[0]);
