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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"

VISIBLE const char *pr_gametype = "";

/* BUILT-IN FUNCTIONS */

VISIBLE char *
PF_VarString (progs_t *pr, int first, int argc)
{
	qfZoneScoped (true);
	char	   *out, *dst;
	const char *src;
	int			len, i;
	pr_type_t **argv = pr->pr_params;

	for (len = 0, i = first; i < argc; i++)
		len += strlen (PR_GetString (pr, *(pr_string_t *) argv[i]));
	dst = out = Hunk_TempAlloc (0, len + 1);
	for (i = first; i < argc; i++) {
		src = PR_GetString (pr, PR_PTR (string,  argv[i]));
		while (*src)
			*dst++ = *src++;
	}
	*dst = 0;
	return out;
}

/*
	vector (vector v) normalize
*/
static void
PF_normalize (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float		new;
	float	   *value1;
	vec3_t		newvalue;

	value1 = P_VECTOR (pr, 0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2] *
		  value1[2];
	new = sqrt (new);

	if (new == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else {
		new = 1 / new;
		newvalue[0] = value1[0] * new;
		newvalue[1] = value1[1] * new;
		newvalue[2] = value1[2] * new;
	}

	VectorCopy (newvalue, R_VECTOR (pr));
}

/*
	float (vector v) vlen
*/
static void
PF_vlen (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float		new;
	float	   *value1;

	value1 = P_VECTOR (pr, 0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2] *
		  value1[2];
	new = sqrt (new);

	R_FLOAT (pr) = new;
}

/*
	float (vector v) vectoyaw
*/
static void
PF_vectoyaw (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float		yaw;
	float	   *value1;

	value1 = P_VECTOR (pr, 0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else {
		yaw = (int) (atan2 (value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	R_FLOAT (pr) = yaw;
}

/*
	vector (vector v) vectoangles
*/
static void
PF_vectoangles (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float		forward, pitch, yaw;
	float	   *value1;

	value1 = P_VECTOR (pr, 0);

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

	R_VECTOR (pr)[0] = pitch;
	R_VECTOR (pr)[1] = yaw;
	R_VECTOR (pr)[2] = 0;
}

/*
	float () random

	Returns a number from 0<= num < 1
*/
static void
PF_random (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float		num;

	num = (rand () & 0x7fff) / ((float) 0x7fff);

	R_FLOAT (pr) = num;
}

/*
	void () break
*/
static void
PF_break (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	Sys_Printf ("break statement\n");
	PR_DumpState (pr);
}

/*
	float (string s) cvar
*/
static void
PF_cvar (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char		*str;

	str = P_GSTRING (pr, 0);

	R_FLOAT (pr) = Cvar_Value (str);
}

/*
	void (string var, string val) cvar_set
*/
static void
PF_cvar_set (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char	*var_name, *val;
	cvar_t		*var;

	var_name = P_GSTRING (pr, 0);
	val = P_GSTRING (pr, 1);
	var = Cvar_FindVar (var_name);
	if (!var)
		var = Cvar_FindAlias (var_name);
	if (!var) {
		Sys_Printf ("%cPF_cvar_set: variable %s not found\n", 3, var_name);
		return;
	}

	Cvar_SetVar (var, val);
}

/*
	float (float f) fabs
*/
static void
PF_fabs (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float		v;

	v = P_FLOAT (pr, 0);
	R_FLOAT (pr) = fabs (v);
}

/*
	entity (entity start, .(...) fld, ... match) find
*/
static void
PF_find (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *s = 0, *t;	// ev_string
	int			i;			// ev_vector
	pr_uint_t   e, f;
	etype_t		type;
	pr_def_t   *field_def;
	edict_t	   *ed;

	e = P_EDICTNUM (pr, 0);
	f = P_INT (pr, 1);
	field_def = PR_FieldAtOfs (pr, f);
	if (!field_def)
		PR_RunError (pr, "PF_Find: bad search field: %d", f);
	type = field_def->type & ~DEF_SAVEGLOBAL;

	if (type == ev_string) {
		s = P_GSTRING (pr, 2);
		if (!s)
			PR_RunError (pr, "PF_Find: bad search string");
	}

	for (e++; e < *pr->num_edicts; e++) {
		ed = EDICT_NUM (pr, e);
		if (ed->free)
			continue;
		switch (type) {
			case ev_string:
				t = E_GSTRING (pr, ed, f);
				if (!t)
					continue;
				if (strcmp (t, s))
					continue;
				RETURN_EDICT (pr, ed);
				return;
			case ev_float:
				if (P_FLOAT (pr, 2) != E_FLOAT (ed, f))
					continue;
				RETURN_EDICT (pr, ed);
				return;
			case ev_vector:
				for (i = 0; i <= 2; i++)
					if (P_FLOAT (pr, 2 + i) != E_FLOAT (ed, f + i))
						continue;
				RETURN_EDICT (pr, ed);
				return;
			case ev_int:
			case ev_entity:
				if (P_INT (pr, 2) != E_INT (ed, f))
					continue;
				RETURN_EDICT (pr, ed);
				return;
			default:
				PR_Error (pr, "PF_Find: unsupported search field");
		}
	}

	RETURN_EDICT (pr, EDICT_NUM (pr, 0));
}

/*
	void () coredump
*/
static void
PF_coredump (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	ED_PrintEdicts (pr, "");
}

/*
	void () traceon
*/
static void
PF_traceon (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	pr->pr_trace = true;
	pr->pr_trace_depth = pr->pr_depth;
}

/*
	void () traceoff
*/
static void
PF_traceoff (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	pr->pr_trace = false;
}

/*
	void (entity e) eprint
*/
static void
PF_eprint (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	ED_PrintNum (pr, P_EDICTNUM (pr, 0), 0);
}

/*
	void (string s) dprint
*/
static void
PF_dprint (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	Sys_Printf ("%c%s", 3, PF_VarString (pr, 0, 1));
}

/*
	float (float v) rint
*/
static void
PF_rint (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float		f;

	f = P_FLOAT (pr, 0);
	if (f > 0)
		R_FLOAT (pr) = (int) (f + 0.5);
	else
		R_FLOAT (pr) = (int) (f - 0.5);
}

/*
	float (float v) floor
*/
static void
PF_floor (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = floor (P_FLOAT (pr, 0));
}

/*
	float (float v) ceil
*/
static void
PF_ceil (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = ceil (P_FLOAT (pr, 0));
}

/*
	entity (entity e) nextent
*/
static void
PF_nextent (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	pr_uint_t   i;
	edict_t	   *ent;

	i = P_EDICTNUM (pr, 0);
	while (1) {
		i++;
		if (i == *pr->num_edicts) {
			RETURN_EDICT (pr, EDICT_NUM (pr, 0));
			return;
		}
		ent = EDICT_NUM (pr, i);
		if (!ent->free) {
			RETURN_EDICT (pr, ent);
			return;
		}
	}
}

// we assume that ints are smaller than floats
#ifdef FLT_MAX_10_EXP
# define STRING_BUF (FLT_MAX_10_EXP + 8)
#else
# define STRING_BUF 128
#endif

/*
	int (float f) ftoi
*/
static void
PF_ftoi (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_INT (pr) = P_FLOAT (pr, 0);
}

/*
	string (float f) ftos
*/
static void
PF_ftos (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	char	string[STRING_BUF];
	int		i;

	// trimming 0s idea thanks to Maddes
	i = snprintf (string, sizeof (string), "%1.6f", P_FLOAT (pr, 0)) - 1;
	for (; i > 0; i--) {
		if (string[i] == '0')
			string[i] = '\0';
		else if (string[i] == '.') {
			string[i] = '\0';
			break;
		} else
			break;
	}

	RETURN_STRING (pr, string);
}

/*
	float (int i) itof
*/
static void
PF_itof (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = P_INT (pr, 0);
}

/*
	string (int i) itos
*/
static void
PF_itos (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	char string[STRING_BUF];

	snprintf (string, sizeof (string), "%d", P_INT (pr, 0));

	RETURN_STRING (pr, string);
}

/*
	float (string s) stof
*/
static void
PF_stof (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = atof (P_GSTRING (pr, 0));
}

/*
	int (string s) stoi
*/
static void
PF_stoi (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_INT (pr) = atoi (P_GSTRING (pr, 0));
}

/*
	vector (string s) stov
*/
static void
PF_stov (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float v[3] = {0, 0, 0};

	sscanf (P_GSTRING (pr, 0), "'%f %f %f'", v, v + 1, v + 2);

	RETURN_VECTOR (pr, v);
}

/*
	string (vector v) vtos
*/
static void
PF_vtos (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	char string[STRING_BUF * 3 + 5];

	snprintf (string, sizeof (string), "'%5.1f %5.1f %5.1f'",
			  P_VECTOR (pr, 0)[0],
			  P_VECTOR (pr, 0)[1],
			  P_VECTOR (pr, 0)[2]);

	RETURN_STRING (pr, string);
}

/*
	float (string char, string s) charcount
*/
static void
PF_charcount (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	char		goal;
	const char *s;
	int			count;

	goal = (P_GSTRING (pr, 0))[0];
	if (goal == '\0') {
		R_FLOAT (pr) = 0;
		return;
	}

	count = 0;
	s = P_GSTRING (pr, 1);
	while (*s) {
		if (*s == goal)
			count++;
		s++;
	}
	R_FLOAT (pr) = count;
}

/*
	string () gametype
*/
static void
PF_gametype (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	RETURN_STRING (pr, pr_gametype);
}

static void
PF_PR_SetField (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	edict_t    *ent = P_EDICT (pr, 0);
	pr_def_t   *field = PR_FindField (pr, P_GSTRING (pr, 1));
	const char *value = P_GSTRING (pr, 2);

	R_INT (pr) = 0;
	if (field)
		R_INT (pr) = ED_ParseEpair (pr, &E_fld (ent, 0), field, value);
}

static void
PF_PR_FindFunction (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	dfunction_t *func = PR_FindFunction (pr, P_GSTRING (pr, 0));
	R_FUNCTION (pr) = 0;
	if (func)
		R_FUNCTION (pr) = func - pr->pr_functions;
}

#define QF (PR_RANGE_QF << PR_RANGE_SHIFT) |

#define bi(x,n,np,params...) {#x, PF_##x, n, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(break,            6, 0),
	bi(random,           7, 0),
	bi(normalize,        9, 1, p(vector)),
	bi(vlen,            12, 1, p(vector)),
	bi(vectoyaw,        13, 1, p(vector)),
	bi(find,            18, -3, p(entity), p(field)),
	bi(dprint,          25, -1),
	bi(ftos,            26, 1, p(float)),
	bi(vtos,            27, 1, p(vector)),
	bi(coredump,        28, 0),
	bi(traceon,         29, 0),
	bi(traceoff,        30, 0),
	bi(eprint,          31, 1, p(entity)),
	bi(rint,            36, 1, p(float)),
	bi(floor,           37, 1, p(float)),
	bi(ceil,            38, 1, p(float)),
	bi(fabs,            43, 1, p(float)),
	bi(cvar,            45, 1, p(string)),
	bi(nextent,         47, 1, p(entity)),
	bi(vectoangles,     51, 1, p(vector)),
	bi(cvar_set,        72, 2, p(string), p(string)),
	bi(stof,            81, 1, p(string)),


	bi(charcount,   QF 101, 2, p(string), p(string)),
	bi(ftoi,        QF 110, 1, p(float)),
	bi(itof,        QF 111, 1, p(int)),
	bi(itos,        QF 112, 1, p(int)),
	bi(stoi,        QF 113, 1, p(string)),
	bi(stov,        QF 114, 1, p(string)),
	bi(gametype,    QF 115, 0),

	bi(PR_SetField,     -1, 3, p(entity), p(string), p(string)),
	bi(PR_FindFunction, -1, 1, p(string)),
	{0}
};

VISIBLE void
PR_Cmds_Init (progs_t *pr)
{
	qfZoneScoped (true);
	PR_RegisterBuiltins (pr, builtins, 0);
}
