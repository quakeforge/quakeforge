/*
	pr_cmds.c

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
#include <stdlib.h>

#include "QF/clip_hull.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/va.h"
#include "QF/progs.h"
#include "QF/zone.h"
#include "QF/mathlib.h"
#include "QF/console.h"

#define	RETURN_EDICT(p, e) ((p)->pr_globals[OFS_RETURN].integer_var = EDICT_TO_PROG(p, e))
#define	RETURN_STRING(p, s) ((p)->pr_globals[OFS_RETURN].integer_var = PR_SetString((p), s))

/*
						BUILT-IN FUNCTIONS
*/

// FIXME: Hunk_TempAlloc, Con_Printf, Cvar_*, PR_SetString, PR_RunError, ED_PrintEdicts, PF_traceon, PF_traceoff, ED_PrintNum, PR_FindBuiltin isn't threadsafe/reentrant

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
	PF_break

	break()
*/
void
PF_break (progs_t *pr)
{
	Con_Printf ("break statement\n");
	*(int *) -4 = 0;					// dump to debugger
//  PR_RunError (pr, "break statement");
}

#if 0
/*
	PF_localcmd

	Sends text to the host's execution buffer

	localcmd (string)
*/
void
PF_localcmd (progs_t *pr)
{
	const char       *str;

	str = G_STRING (pr, OFS_PARM0);
	Cbuf_AddText (str);
}
#endif

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
		Con_Printf ("PF_cvar_set: variable %s not found\n", var_name);
		return;
	}

	Cvar_Set (var, val);
}

#ifdef FLT_MAX_10_EXP
# define MAX_FLOAT_STRING (FLT_MAX_10_EXP + 8)
#else
# define MAX_FLOAT_STRING 128
#endif

void
PF_ftos (progs_t *pr)
{
	float       v;
	int         i;						// 1999-07-25 FTOS fix by Maddes
	char        string[MAX_FLOAT_STRING];

	v = G_FLOAT (pr, OFS_PARM0);

	if (v == (int) v)
		snprintf (string, sizeof (string), "%d", (int) v);
	else
// 1999-07-25 FTOS fix by Maddes  start
	{
		snprintf (string, sizeof (string), "%1.6f", v);
		for (i = strlen (string) - 1;
			 i > 0 && string[i] == '0' && string[i - 1] != '.';
			 i--) {
			string[i] = 0;
		}
	}
// 1999-07-25 FTOS fix by Maddes  end
	G_INT (pr, OFS_RETURN) = PR_SetString (pr, string);
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
	char string[MAX_FLOAT_STRING * 3 + 5];
	snprintf (string, sizeof (string), "'%5.1f %5.1f %5.1f'",
			  G_VECTOR (pr, OFS_PARM0)[0],
			  G_VECTOR (pr, OFS_PARM0)[1],
			  G_VECTOR (pr, OFS_PARM0)[2]);
	G_INT (pr, OFS_RETURN) = PR_SetString (pr, string);
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

	for (e++; e < *pr->num_edicts; e++) {
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

	RETURN_EDICT (pr, *pr->edicts);
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
	PF_dprint
*/
void
PF_dprint (progs_t *pr)
{
	Con_Printf ("%s", PF_VarString (pr, 0));
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
		if (i == *pr->num_edicts) {
			RETURN_EDICT (pr, *pr->edicts);
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

void
PF_checkbuiltin (progs_t *pr)
{
	G_FUNCTION (pr, OFS_RETURN) = -PR_FindBuiltin (pr, G_STRING (pr, OFS_PARM0));
}

void
PF_getbuiltin (progs_t *pr)
{
	int i;
	const char *name;

	name = G_STRING (pr, OFS_PARM0);
	i = PR_FindBuiltin (pr, name);
	if (!i)
		PR_RunError (pr, "PF_getfunction: function '%s' not found!\n", name);
	G_FUNCTION (pr, OFS_RETURN) = -i;
}

void
PR_Cmds_Init (progs_t *pr)
{
	PR_AddBuiltin (pr, "break", PF_break, 6); // void() break = #6
	PR_AddBuiltin (pr, "random", PF_random, 7);	// float() random = #7
	PR_AddBuiltin (pr, "normalize", PF_normalize, 9);	// vector(vector v) normalize = #9
	PR_AddBuiltin (pr, "vlen", PF_vlen, 12);	// float(vector v) vlen = #12
	PR_AddBuiltin (pr, "vectoyaw", PF_vectoyaw, 13);	// float(vector v) vectoyaw = #13
	PR_AddBuiltin (pr, "find", PF_Find, 18);	// entity(entity start, .string fld, string match) find = #18
	PR_AddBuiltin (pr, "dprint", PF_dprint, 25);  // void(string s) dprint = #25
	PR_AddBuiltin (pr, "ftos", PF_ftos, 26);	// void(string s) ftos = #26
	PR_AddBuiltin (pr, "vtos", PF_vtos, 27);	// void(string s) vtos = #27
	PR_AddBuiltin (pr, "coredump", PF_coredump, 28);	// void() coredump = #28
	PR_AddBuiltin (pr, "traceon", PF_traceon, 29);	// void() traceon = #29
	PR_AddBuiltin (pr, "traceoff", PF_traceoff, 30);	// void() traceoff = #30
	PR_AddBuiltin (pr, "eprint", PF_eprint, 31);	// void(entity e) = #31 debug print an entire entity
	PR_AddBuiltin (pr, "rint", PF_rint, 36);	// float(float v) rint = #36
	PR_AddBuiltin (pr, "floor", PF_floor, 37);	// float(float v) floor = #37
	PR_AddBuiltin (pr, "ceil", PF_ceil, 38);	// float(float v) ceil = #38
	PR_AddBuiltin (pr, "fabs", PF_fabs, 43);	// float(float f) fabs = #43
	PR_AddBuiltin (pr, "cvar", PF_cvar, 45);	// float(string s) cvar = #45
#if 0
	PR_AddBuiltin (pr, "localcmd", PF_localcmd, 46);	// void(string s) localcmd = #46
#endif
	PR_AddBuiltin (pr, "nextent", PF_nextent, 47);	// entity(entity e) nextent = #47
	PR_AddBuiltin (pr, "vectoangles", PF_vectoangles, 51);	// vector(vector v) vectoangles = #51
	PR_AddBuiltin (pr, "cvar_set", PF_cvar_set, 72);	// void(string var, string val) cvar_set = #72

	PR_AddBuiltin (pr, "stof", PF_stof, 81);	// float(string s) stof = #81
	PR_AddBuiltin (pr, "strlen", PF_strlen, 100);	// = #100
	PR_AddBuiltin (pr, "charcount", PF_charcount, 101);	// = #101
#if 0
	PR_AddBuiltin (pr, "cfopen", PF_cfopen, 103);	// float (string path, string mode) cfopen = #103
	PR_AddBuiltin (pr, "cfclose", PF_cfclose, 104);	// void (float desc) cfclose = #104
	PR_AddBuiltin (pr, "cfread", PF_cfread, 105);	// string (float desc) cfread = #105
	PR_AddBuiltin (pr, "cfwrite", PF_cfwrite, 106);	// float (float desc, string buf) cfwrite = #106
	PR_AddBuiltin (pr, "cfeof", PF_cfeof, 107);	// float (float desc) cfeof = #107
	PR_AddBuiltin (pr, "cfquota", PF_cfquota, 108);	// float () cfquota = #108
#endif
};
