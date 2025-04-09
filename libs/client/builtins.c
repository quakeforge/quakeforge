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

#include <string.h>

#include "QF/mathlib.h"
#include "QF/sys.h"

#include "csqc.h"

#include "r_internal.h"

#define CSQC_API_VERSION 1.0f

cl_globals_t cl_globals;
cl_fields_t  cl_fields;
cl_funcs_t   cl_funcs;

typedef struct {
	etype_t     type;
	const char *name;
	void       *field;
} csqc_def_t;

#define CSQC_DEF(t,n) {ev_##t, #n, &cl_globals.n}
static csqc_def_t csqc_defs[] = {
	CSQC_DEF (entity,	self),
	CSQC_DEF (entity,	other),
	CSQC_DEF (entity,	world),
	CSQC_DEF (float,	time),
	CSQC_DEF (float,	cltime),
	CSQC_DEF (float,	player_localentnum),
	CSQC_DEF (float,	player_localnum),
	CSQC_DEF (float,	maxclients),
	CSQC_DEF (float,	clientcommandframe),
	CSQC_DEF (float,	servercommandframe),
	CSQC_DEF (string,	mapname),
	CSQC_DEF (float,	intermission),
	CSQC_DEF (vector,	v_forward),
	CSQC_DEF (vector,	v_up),
	CSQC_DEF (vector,	v_right),
	CSQC_DEF (vector,	view_angles),
	CSQC_DEF (float,	trace_allsolid),
	CSQC_DEF (float,	trace_startsolid),
	CSQC_DEF (float,	trace_fraction),
	CSQC_DEF (vector,	trace_endpos),
	CSQC_DEF (vector,	trace_plane_normal),
	CSQC_DEF (float,	trace_plane_dist),
	CSQC_DEF (entity,	trace_ent),
	CSQC_DEF (float,	trace_inopen),
	CSQC_DEF (float,	trace_inwater),
	CSQC_DEF (float,	input_timelength),
	CSQC_DEF (vector,	input_angles),
	CSQC_DEF (vector,	input_movevalues),
	CSQC_DEF (float,	input_buttons),
	CSQC_DEF (float,	input_impulse),
};

#define CSQC_FIELD(t,n) {ev_##t, #n, &cl_fields.n}
static csqc_def_t csqc_fields[] = {
	CSQC_FIELD (float,	modelindex),
	CSQC_FIELD (vector,	absmin),
	CSQC_FIELD (vector,	absmax),
	CSQC_FIELD (float,	entnum),
	CSQC_FIELD (float,	drawmask),
	CSQC_FIELD (func,	predraw),
	CSQC_FIELD (float,	movetype),
	CSQC_FIELD (float,	solid),
	CSQC_FIELD (vector,	origin),
	CSQC_FIELD (vector,	oldorigin),
	CSQC_FIELD (vector,	velocity),
	CSQC_FIELD (vector,	angles),
	CSQC_FIELD (vector,	avelocity),
	CSQC_FIELD (float,	pmove_flags),
	CSQC_FIELD (string,	classname),
	CSQC_FIELD (float,	renderflags),
	CSQC_FIELD (string,	model),
	CSQC_FIELD (float,	frame),
	CSQC_FIELD (float,	frame1time),
	CSQC_FIELD (float,	frame2),
	CSQC_FIELD (float,	frame2time),
	CSQC_FIELD (float,	lerpfrac),
	CSQC_FIELD (float,	skin),
	CSQC_FIELD (float,	effects),
	CSQC_FIELD (vector,	mins),
	CSQC_FIELD (vector,	maxs),
	CSQC_FIELD (vector,	size),
	CSQC_FIELD (func,	touch),
	CSQC_FIELD (func,	think),
	CSQC_FIELD (func,	blocked),
	CSQC_FIELD (float,	nextthink),
	CSQC_FIELD (entity,	chain),
	CSQC_FIELD (entity,	enemy),
	CSQC_FIELD (float,	flags),
	CSQC_FIELD (float,	colormap),
	CSQC_FIELD (entity,	owner),
};

static struct {
	const char *name;
	pr_func_t  *field;
} csqc_func_list[] = {
	{"CSQC_Init",				&cl_funcs.Init},
	{"CSQC_Shutdown",			&cl_funcs.Shutdown},
	{"CSQC_UpdateView",			&cl_funcs.UpdateView},
	{"CSQC_WorldLoaded",		&cl_funcs.WorldLoaded},
	{"CSQC_Parse_StuffCmd",		&cl_funcs.Parse_StuffCmd},
	{"CSQC_Parse_CenterPrint",	&cl_funcs.Parse_CenterPrint},
	{"CSQC_Parse_Print",		&cl_funcs.Parse_Print},
	{"CSQC_InputEvent",			&cl_funcs.InputEvent},
	{"CSQC_ConsoleCommand",		&cl_funcs.ConsoleCommand},
	{"CSQC_Ent_Update",			&cl_funcs.Ent_Update},
	{"CSQC_Event_Sound",		&cl_funcs.Event_Sound},
	{"CSQC_Remove",				&cl_funcs.Remove},
};

progs_t csqc_pr_state;

#define bi(x) static void CSQC_##x (progs_t *pr, void *_res)

bi(makevectors)
{
	AngleVectors (P_VECTOR (pr, 0), *cl_globals.v_forward,
				  *cl_globals.v_right, *cl_globals.v_up);
}

bi(setorigin)
{
	edict_t    *e;
	vec_t      *org;

	e = P_EDICT (pr, 0);
	org = P_VECTOR (pr, 1);
	VectorCopy (org, CSQCvector (e, origin));

	//XXX CS_LinkEdict (e, false);
}

bi(setmodel)
{
}

bi(setsize)
{
}

bi(sound)
{
}

bi(error)
{
}

bi(objerror)
{
}

bi(spawn)
{
}

bi(remove)
{
}

bi(traceline)
{
}

bi(precache_sound)
{
}

bi(precache_model)
{
}

bi(findradius)
{
}

bi(walkmove)
{
}

bi(droptofloor)
{
}

bi(lightstyle)
{
}

bi(checkbottom)
{
}

bi(pointcontents)
{
}

bi(localcmd)
{
}

bi(changeyaw)
{
}

bi(makestatic)
{
}

bi(ambientsound)
{
}

bi(clearscene)
{
}

bi(addentities)
{
}

bi(addentity)
{
}

bi(setviewprop)
{
}

bi(getviewprop)
{
}

bi(adddynamiclight)
{
}

bi(renderscene)
{
}

bi(unproject)
{
}

bi(project)
{
}

bi(is_cached_pic)
{
}

bi(precache_pic)
{
}

bi(drawgetimagesize)
{
}

bi(free_pic)
{
}

bi(drawcharacter)
{
}

bi(drawrawstring)
{
}

bi(drawpic)
{
}

bi(drawfill)
{
	float      *pos = P_VECTOR (pr, 0);
	float      *size = P_VECTOR (pr, 1);
	quat_t      rgba;

	VectorCopy (P_VECTOR (pr, 2), rgba);
	rgba[3] = P_FLOAT (pr, 3);

	r_funcs->Draw_FillRGBA (pos[0], pos[1], size[0], size[1], rgba);
	R_FLOAT (pr) = 1;
}

bi(drawcolorocodedstring)
{
}

bi(setmodelindex)
{
}

bi(modelnameforindex)
{
}

bi(setsensitivityscaler)
{
}

bi(cprint)
{
}

bi(print)
{
}

bi(pointparticles)
{
}

bi(trailparticles)
{
}

bi(particleeffectnum)
{
}

bi(getinputstate)
{
}

bi(runplayerphysics)
{
}

bi(isdemo)
{
}

bi(isserver)
{
}

bi(keynumtostring)
{
}

bi(stringtokeynum)
{
}

bi(getkeybind)
{
}

bi(setlistener)
{
}

bi(deltalisten)
{
}

bi(readbyte)
{
}

bi(readchar)
{
}

bi(readshort)
{
}

bi(readlong)
{
}

bi(readcoord)
{
}

bi(readangle)
{
}

bi(readstring)
{
}

bi(readfloat)
{
}

bi(readentitynum)
{
}

bi(getstatf)
{
}

bi(getstati)
{
}

bi(getstats)
{
}

bi(getplayerkey)
{
}

bi(serverkey)
{
}

bi(getentitytoken)
{
}

bi(registercommand)
{
}

bi(wasfreed)
{
	edict_t    *ed = P_EDICT (pr, 0);
	R_FLOAT (pr) = ed->free;
}

bi(sendevent)
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

	// 3D scene management
	CSQC_BUILTIN (clearscene, 300),
	CSQC_BUILTIN (addentities, 301),
	CSQC_BUILTIN (addentity, 302),
	CSQC_BUILTIN (setviewprop, 303),
	CSQC_BUILTIN (getviewprop, 309),
	CSQC_BUILTIN (adddynamiclight, 305),
	CSQC_BUILTIN (renderscene, 304),
	CSQC_BUILTIN (unproject, 310),
	CSQC_BUILTIN (project, 311),

	// 2D display
	//CSQC_BUILTIN (drawfillpal, 314),
	CSQC_BUILTIN (is_cached_pic, 316),
	CSQC_BUILTIN (precache_pic, 317),
	CSQC_BUILTIN (drawgetimagesize, 318),
	CSQC_BUILTIN (free_pic, 319),
	CSQC_BUILTIN (drawcharacter, 320),
	CSQC_BUILTIN (drawrawstring, 321),
	CSQC_BUILTIN (drawpic, 322),
	CSQC_BUILTIN (drawfill, 323),
	CSQC_BUILTIN (drawcolorocodedstring, 326),

	CSQC_BUILTIN (setmodelindex, 333),
	CSQC_BUILTIN (modelnameforindex, 334),
	CSQC_BUILTIN (setsensitivityscaler, 346),
	CSQC_BUILTIN (cprint, 338),
	CSQC_BUILTIN (print, 339),
	CSQC_BUILTIN (pointparticles, 337),
	CSQC_BUILTIN (trailparticles, 336),
	CSQC_BUILTIN (particleeffectnum, 335),

	CSQC_BUILTIN (getinputstate, 345),
	CSQC_BUILTIN (runplayerphysics, 347),
	CSQC_BUILTIN (isdemo, 349),
	CSQC_BUILTIN (isserver, 350),
	CSQC_BUILTIN (keynumtostring, 340),
	CSQC_BUILTIN (stringtokeynum, 341),
	CSQC_BUILTIN (getkeybind, 342),

	CSQC_BUILTIN (setlistener, 351),

	CSQC_BUILTIN (deltalisten, 371),

	CSQC_BUILTIN (readbyte, 360),
	CSQC_BUILTIN (readchar, 361),
	CSQC_BUILTIN (readshort, 362),
	CSQC_BUILTIN (readlong, 363),
	CSQC_BUILTIN (readcoord, 364),
	CSQC_BUILTIN (readangle, 365),
	CSQC_BUILTIN (readstring, 366),
	CSQC_BUILTIN (readfloat, 367),
	CSQC_BUILTIN (readentitynum, 368),

	CSQC_BUILTIN (getstatf, 330),
	CSQC_BUILTIN (getstati, 331),
	CSQC_BUILTIN (getstats, 332),
	CSQC_BUILTIN (getplayerkey, 348),
	CSQC_BUILTIN (serverkey, 354),
	CSQC_BUILTIN (getentitytoken, 355),
	CSQC_BUILTIN (registercommand, 352),

	CSQC_BUILTIN (wasfreed, 353),
	CSQC_BUILTIN (sendevent, 359),
};

static float
version_number (void)
{
	union {
		float       f;
		int         i;
	}           version;
	int         ver[4];

	memset (ver, 0, sizeof (ver));
	sscanf (VERSION, "%d.%d.%d.%d", &ver[0], &ver[1], &ver[2], &ver[3]);
	// allow 1 digit for patch, 2 for minor and N for major.
	version.f = ver[2] + ver[1] * 10 + ver[0] * 1000;
	// hack in the git revision count. for the same count, the fraction of the
	// version will move around depending on the last release version, but
	// for the same last release version, newer revisions are guaranteed to
	// compare greater than older revisions.
	//
	// Of course, this will break if the bits needed to represent the last
	// release version ever overlap with the bits needed for the git revision
	// count, but that's not likely any time soon as major is still 0 and minor
	// is still single digit thus only 7 bits are needed for the version
	// leaving 17 for the revision count. There better not be 128k revisions
	// between releases :P
	version.i |= ver[3];
	Sys_MaskPrintf (SYS_csqc, "version: %.8e\n", version.f);
	return version.f;
}

static int
check_type (const csqc_def_t *def, const pr_def_t *ddef)
{
	etype_t     ddef_type = ddef->type & ~DEF_SAVEGLOBAL;

	if (ddef_type != def->type) {
		Sys_Printf ("def type mismatch: %s %s\n", pr_type_name[ddef_type],
					pr_type_name[def->type]);
		return 0;
	}
	return 1;
}

static void
set_address (const csqc_def_t *def, void *address)
{
	switch (def->type) {
		case ev_void:
		case ev_short:
		case ev_ushort:
		case ev_invalid:
		case ev_type_count:
			break;
		case ev_float:
		case ev_vector:
		case ev_quaternion:
			*(float **)def->field = (float *) address;
			break;
		case ev_double:
			*(double **)def->field = (double *) address;
			break;
		case ev_string:
		case ev_entity:
		case ev_field:
		case ev_func:
		case ev_ptr:
		case ev_int:
		case ev_uint:
			*(int **)def->field = (int *) address;
			break;
		case ev_long:
		case ev_ulong:
			*(pr_long_t **)def->field = (pr_long_t *) address;
			break;
	}
}

static int
resolve_globals (progs_t *pr, csqc_def_t *def)
{
	pr_def_t   *ddef;
	int         ret = 1;

	for (; def->name; def++) {
		ddef = PR_FindGlobal (pr, def->name);
		if (ddef) {
			ret = check_type (def, ddef) && ret;
			set_address (def, &G_FLOAT (pr, ddef->ofs));
		} else {
			PR_Undefined (pr, "global", def->name);
			ret = 0;
		}
	}
	return ret;
}

static int
resolve_fields (progs_t *pr, csqc_def_t *def)
{
	pr_def_t   *ddef;
	int         ret = 1;

	for (; def->name; def++) {
		*(int *) def->field = -1;
		ddef = PR_FindField (pr, def->name);
		if (ddef) {
			ret = check_type (def, ddef) && ret;
			*(int *) def->field = ddef->ofs;
		} else {
			PR_Undefined (pr, "field", def->name);
			ret = 0;
		}
	}
	return ret;
}

static int
csqc_load (progs_t *pr)
{
	size_t      i;

	resolve_globals (pr, csqc_defs);
	resolve_fields (pr, csqc_fields);
	for (i = 0;
		 i < sizeof (csqc_func_list) / sizeof (csqc_func_list[0]); i++) {
		dfunction_t *f = PR_FindFunction (pr, csqc_func_list[i].name);

		*csqc_func_list[i].field = 0;
		if (f)
			*csqc_func_list[i].field = (pr_func_t) (f - pr->pr_functions);
	}
	if (cl_funcs.Init) {
		auto params = PR_SaveParams (pr);
		PR_SetupParams (pr, 3, 1);

		P_FLOAT (&csqc_pr_state, 0) = CSQC_API_VERSION;
		P_STRING (&csqc_pr_state, 1) = PR_SetTempString (&csqc_pr_state,
														 PACKAGE_NAME);
		P_FLOAT (&csqc_pr_state, 2) = version_number ();
		PR_ExecuteProgram (&csqc_pr_state, cl_funcs.Init);

		PR_RestoreParams (pr, params);
	}
	return 1;
}

void
CSQC_Cmds_Init (void)
{
	version_number();
	PR_Cmds_Init (&csqc_pr_state);
	PR_RegisterBuiltins (&csqc_pr_state, builtins, nullptr);
	PR_AddLoadFunc (&csqc_pr_state, csqc_load);
}
