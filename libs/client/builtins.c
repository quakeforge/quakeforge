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
	
	CS_LinkEdict (e, false);
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

static builtin_t builtins[] = {
	{"makevectors",			CSQC_makevectors,		1},
	{"setorigin",			CSQC_setorigin,			2},
	{"setmodel",			CSQC_setmodel,			3},
	{"setsize",				CSQC_setsize,			4},

	{"sound",				CSQC_sound,				8},
	{"error",				CSQC_error,				10},
	{"objerror",			CSQC_objerror,			11},
	{"spawn",				CSQC_spawn,				14},
	{"remove",				CSQC_remove,			15},
	{"traceline",			CSQC_traceline,			16},
//	{"checkclient",			CSQC_no,				17},

	{"precache_sound",		CSQC_precache_sound,	19},
	{"precache_model",		CSQC_precache_model,	20},
//	{"stuffcmd",			CSQC_no,				21},
	{"findradius",			CSQC_findradius,		22},
//	{"bprint",				CSQC_no,				23},
//	{"sprint",				CSQC_no,				24},

	{"walkmove",			CSQC_walkmove,			32},

	{"droptofloor",			CSQC_droptofloor,		34},
	{"lightstyle",			CSQC_lightstyle,		35},

	{"checkbottom",			CSQC_checkbottom,		40},
	{"pointcontents",		CSQC_pointcontents,		41},

//	{"aim",					CSQC_no,				44},

	{"localcmd",			CSQC_localcmd,			46},
	{"changeyaw",			CSQC_changeyaw,			49},

//	{"writebyte",			CSQC_no,				52},
//	{"WriteBytes",			CSQC_no,				-1},
//	{"writechar",			CSQC_no,				53},
//	{"writeshort",			CSQC_no,				54},
//	{"writelong",			CSQC_no,				55},
//	{"writecoord",			CSQC_no,				56},
//	{"writeangle",			CSQC_no,				57},
//	{"WriteCoordV",			CSQC_no,				-1},
//	{"WriteAngleV",			CSQC_no,				-1},
//	{"writestring",			CSQC_no,				58},
//	{"writeentity",			CSQC_no,				59},

//	{"movetogoal",			SV_MoveToGoal,			67},
//	{"precache_file",		CSQC_no,				68},
	{"makestatic",			CSQC_makestatic,		69},
//	{"changelevel",			CSQC_no,				70},

//	{"centerprint",			CSQC_no,				73},
	{"ambientsound",		CSQC_ambientsound,		74},
	{"precache_model2",		CSQC_precache_model,	75},
	{"precache_sound2",		CSQC_precache_sound,	76},
//	{"precache_file2",		CSQC_no,				77},
//	{"setspawnparms",		CSQC_no,				78},

//	{"logfrag",				CSQC_no,				79},
//	{"infokey",				CSQC_no,				80},
//	{"multicast",			CSQC_no,				82},
};

void
CSQC_Cmds_Init (void)
{
	PR_Cmds_Init (&csqc_pr_state);
	PR_RegisterBuiltins (&csqc_pr_state, builtins);
}
