/*
	builtins.c

	CSQC builtins

	Copyright (C) 2011 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/06/15

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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";
#include "QF/mathlib.h"

#include "csqc.h"

static struct {
	vec3_t     *v_forward;
	vec3_t     *v_up;
	vec3_t     *v_right;
} csqc_globals;

static struct {
	int         origin;
} csqc_fields;

static progs_t csqc_pr_state;

#if TYPECHECK_PROGS
#define CSQCFIELD(e,f,t) E_var (e, PR_AccessField (&csqc_pr_state, #f, ev_##t, __FILE__, __LINE__), t)
#else
#define CSQCFIELD(e,f,t) E_var (e, csqc_fields.f, t)
#endif

#define CSQCfloat(e,f)      CSQCFIELD (e, f, float)
#define CSQCstring(e,f)     CSQCFIELD (e, f, string)
#define CSQCfunc(e,f)       CSQCFIELD (e, f, func)
#define CSQCentity(e,f)     CSQCFIELD (e, f, entity)
#define CSQCvector(e,f)     CSQCFIELD (e, f, vector)
#define CSQCinteger(e,f)    CSQCFIELD (e, f, integer)


static void
CSQC_makevectors (progs_t *pr)
{
	AngleVectors (P_VECTOR (pr, 0), *csqc_globals.v_forward,
				  *csqc_globals.v_right, *csqc_globals.v_up);
}

static void
CSQC_setorigin (progs_t *pr)
{
	edict_t    *e;
	vec_t      *org;

	e = P_EDICT (pr, 0);
	org = P_VECTOR (pr, 1);
	VectorCopy (org, CSQCvector (e, origin));

	//XXX CS_LinkEdict (e, false);
}

static void
CSQC_setmodel (progs_t *pr)
{
}

static void
CSQC_setsize (progs_t *pr)
{
}

static void
CSQC_sound (progs_t *pr)
{
}

static void
CSQC_error (progs_t *pr)
{
}

static void
CSQC_objerror (progs_t *pr)
{
}

static void
CSQC_spawn (progs_t *pr)
{
}

static void
CSQC_remove (progs_t *pr)
{
}

static void
CSQC_traceline (progs_t *pr)
{
}

static void
CSQC_precache_sound (progs_t *pr)
{
}

static void
CSQC_precache_model (progs_t *pr)
{
}

static void
CSQC_findradius (progs_t *pr)
{
}

static void
CSQC_walkmove (progs_t *pr)
{
}

static void
CSQC_droptofloor (progs_t *pr)
{
}

static void
CSQC_lightstyle (progs_t *pr)
{
}

static void
CSQC_checkbottom (progs_t *pr)
{
}

static void
CSQC_pointcontents (progs_t *pr)
{
}

static void
CSQC_localcmd (progs_t *pr)
{
}

static void
CSQC_changeyaw (progs_t *pr)
{
}

static void
CSQC_makestatic (progs_t *pr)
{
}

static void
CSQC_ambientsound (progs_t *pr)
{
}

#define CSQC_BUILTIN(name, number) {#name, CSQC_##name, number}
static builtin_t builtins[] = {
	CSQC_BUILTIN (makevectors, 1),
	CSQC_BUILTIN (setorigin, 2),
	CSQC_BUILTIN (setmodel, 3),
	CSQC_BUILTIN (setsize, 4),

	CSQC_BUILTIN (sound, 8),
	CSQC_BUILTIN (error, 10),
	CSQC_BUILTIN (objerror, 11),
	CSQC_BUILTIN (spawn, 14),
	CSQC_BUILTIN (remove, 15),
	CSQC_BUILTIN (traceline, 16),
//	CSQC_BUILTIN (checkclient, 17),

	CSQC_BUILTIN (precache_sound, 19),
	CSQC_BUILTIN (precache_model, 20),
//	CSQC_BUILTIN (stuffcmd, 21),
	CSQC_BUILTIN (findradius, 22),
//	CSQC_BUILTIN (bprint, 23),
//	CSQC_BUILTIN (sprint, 24),

	CSQC_BUILTIN (walkmove, 32),

	CSQC_BUILTIN (droptofloor, 34),
	CSQC_BUILTIN (lightstyle, 35),

	CSQC_BUILTIN (checkbottom, 40),
	CSQC_BUILTIN (pointcontents, 41),

//	CSQC_BUILTIN (aim, 44),

	CSQC_BUILTIN (localcmd, 46),
	CSQC_BUILTIN (changeyaw, 49),

//	CSQC_BUILTIN (writebyte, 52),
//	{"WriteBytes",			CSQC_no,				-1},
//	CSQC_BUILTIN (writechar, 53),
//	CSQC_BUILTIN (writeshort, 54),
//	CSQC_BUILTIN (writelong, 55),
//	CSQC_BUILTIN (writecoord, 56),
//	CSQC_BUILTIN (writeangle, 57),
//	{"WriteCoordV",			CSQC_no,				-1},
//	{"WriteAngleV",			CSQC_no,				-1},
//	CSQC_BUILTIN (writestring, 58),
//	CSQC_BUILTIN (writeentity, 59),

//	CSQC_BUILTIN (movetogoal, 67),
//	CSQC_BUILTIN (precache_file, 68),
	CSQC_BUILTIN (makestatic, 69),
//	CSQC_BUILTIN (changelevel, 70),

//	CSQC_BUILTIN (centerprint, 73),XXX
	CSQC_BUILTIN (ambientsound, 74),
	{"precache_model2", CSQC_precache_model, 75},
	{"precache_sound2", CSQC_precache_sound, 76},
//	CSQC_BUILTIN (precache_file2, 77),
//	CSQC_BUILTIN (setspawnparms, 78),XXX

//	CSQC_BUILTIN (logfrag, 79),
//	CSQC_BUILTIN (infokey, 80),
//	CSQC_BUILTIN (multicast, 82),
};

void
CSQC_Cmds_Init (void)
{
	PR_Cmds_Init (&csqc_pr_state);
	PR_RegisterBuiltins (&csqc_pr_state, builtins);
}
