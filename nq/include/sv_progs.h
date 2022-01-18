/*
	sv_progs.h

	server specific progs definitions

	Copyright (C) 2000       Bill Currie

	Author: Bill Currie
	Date: 28 Feb 2001

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

#ifndef __sv_progs_h
#define __sv_progs_h

#include "QF/link.h"
#include "QF/progs.h"

#include "protocol.h"
#include "sv_pr_cmds.h"

typedef struct {
	pr_uint_t  *self;
	pr_uint_t  *other;
	pr_uint_t  *world;
	float      *time;
	float      *frametime;
	float      *force_retouch;
	pr_string_t *mapname;
	pr_string_t *startspot;
	float      *deathmatch;
	float      *coop;
	float      *teamplay;
	float      *serverflags;
	float      *total_secrets;
	float      *total_monsters;
	float      *found_secrets;
	float      *killed_monsters;
	float      *parms;
	vec3_t     *v_forward;
	vec3_t     *v_up;
	vec3_t     *v_right;
	float      *trace_allsolid;
	float      *trace_startsolid;
	float      *trace_fraction;
	vec3_t     *trace_endpos;
	vec3_t     *trace_plane_normal;
	float      *trace_plane_dist;
	pr_uint_t  *trace_ent;
	float      *trace_inopen;
	float      *trace_inwater;
	pr_uint_t  *msg_entity;
	pr_string_t *null;

	pr_uint_t  *newmis;
} sv_globals_t;

extern sv_globals_t sv_globals;

typedef struct {
	pr_func_t   main;
	pr_func_t   StartFrame;
	pr_func_t   PlayerPreThink;
	pr_func_t   PlayerPostThink;
	pr_func_t   ClientKill;
	pr_func_t   ClientConnect;
	pr_func_t   PutClientInServer;
	pr_func_t   ClientDisconnect;
	pr_func_t   SetNewParms;
	pr_func_t   SetChangeParms;

	pr_func_t   EndFrame;
} sv_funcs_t;

extern sv_funcs_t sv_funcs;

typedef struct
{
	pr_int_t    modelindex;			//float
	pr_int_t    absmin;				//vec3_t
	pr_int_t    absmax;				//vec3_t
	pr_int_t    ltime;				//float
	pr_int_t    movetype;			//float
	pr_int_t    solid;				//float
	pr_int_t    origin;				//vec3_t
	pr_int_t    oldorigin;			//vec3_t
	pr_int_t    velocity;			//vec3_t
	pr_int_t    angles;				//vec3_t
	pr_int_t    avelocity;			//vec3_t
	pr_int_t    punchangle;			//vec3_t
	pr_int_t    classname;			//pr_string_t
	pr_int_t    model;				//pr_string_t
	pr_int_t    frame;				//float
	pr_int_t    skin;				//float
	pr_int_t    effects;			//float
	pr_int_t    gravity;			//float
	pr_int_t    mins;				//vec3_t
	pr_int_t    maxs;				//vec3_t
	pr_int_t    size;				//vec3_t
	pr_int_t    touch;				//pr_func_t
	pr_int_t    think;				//pr_func_t
	pr_int_t    blocked;			//pr_func_t
	pr_int_t    nextthink;			//float
	pr_int_t    groundentity;		//int
	pr_int_t    health;				//float
	pr_int_t    frags;				//float
	pr_int_t    weapon;				//float
	pr_int_t    weaponmodel;		//pr_string_t
	pr_int_t    weaponframe;		//float
	pr_int_t    currentammo;		//float
	pr_int_t    ammo_shells;		//float
	pr_int_t    ammo_nails;			//float
	pr_int_t    ammo_rockets;		//float
	pr_int_t    ammo_cells;			//float
	pr_int_t    items;				//float
	pr_int_t    items2;				//float
	pr_int_t    takedamage;			//float
	pr_int_t    chain;				//int
	pr_int_t    view_ofs;			//vec3_t
	pr_int_t    button0;			//float
	pr_int_t    button1;			//float
	pr_int_t    button2;			//float
	pr_int_t    impulse;			//float
	pr_int_t    fixangle;			//float
	pr_int_t    v_angle;			//vec3_t
	pr_int_t    idealpitch;			//float
	pr_int_t    netname;			//pr_string_t
	pr_int_t    enemy;				//int
	pr_int_t    flags;				//float
	pr_int_t    colormap;			//float
	pr_int_t    team;				//float
	pr_int_t    teleport_time;		//float
	pr_int_t    armorvalue;			//float
	pr_int_t    waterlevel;			//float
	pr_int_t    watertype;			//float
	pr_int_t    ideal_yaw;			//float
	pr_int_t    yaw_speed;			//float
	pr_int_t    goalentity;			//int
	pr_int_t    spawnflags;			//float
	pr_int_t    dmg_take;			//float
	pr_int_t    dmg_save;			//float
	pr_int_t    dmg_inflictor;		//int
	pr_int_t    owner;				//int
	pr_int_t    movedir;			//vec3_t
	pr_int_t    message;			//pr_string_t
	pr_int_t    sounds;				//float

	pr_int_t    rotated_bbox;		//int
	pr_int_t    alpha;				//float

	pr_int_t    lastruntime;		//float
} sv_fields_t;

extern sv_fields_t sv_fields;

extern progs_t sv_pr_state;

#define PR_RANGE_ID		0x0000
#define PR_RANGE_ID_MAX	82

#if TYPECHECK_PROGS
#define SVFIELD(e,f,t) E_var (e, PR_AccessField (&sv_pr_state, #f, ev_##t, __FILE__, __LINE__), t)
#else
#define SVFIELD(e,f,t) E_var (e, sv_fields.f, t)
#endif

#define SVfloat(e,f)	SVFIELD (e, f, float)
#define SVstring(e,f)	SVFIELD (e, f, string)
#define SVfunc(e,f)		SVFIELD (e, f, func)
#define SVentity(e,f)	SVFIELD (e, f, entity)
#define SVvector(e,f)	(&SVFIELD (e, f, vector))
#define SVint(e,f)		SVFIELD (e, f, int)
#if TYPECHECK_PROGS
#define SVdouble(e,f) E_DOUBLE (e, PR_AccessField (&sv_pr_state, #f, ev_##t, __FILE__, __LINE__))
#else
#define SVdouble(e,f) E_DOUBLE (e, sv_fields.f)
#endif

typedef struct edict_leaf_s {
	struct edict_leaf_s *next;
	unsigned    leafnum;
} edict_leaf_t;

typedef struct sv_data_s {
	edict_t    *edict;
	link_t      area;			///< linked to a division node or leaf
	edict_leaf_t *leafs;
	entity_state_t state;
	byte        alpha;
	qboolean    sendinterval;
	qboolean    add_grav;
} sv_data_t;

#define SVdata(e)		((sv_data_t *) ((e)->edata))
#define EDICT_FROM_AREA(l) (STRUCT_FROM_LINK(l,sv_data_t,area)->edict)

static inline void
sv_pr_touch (edict_t *self, edict_t *other)
{
	pr_int_t    this;

	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, self);
	*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, other);
	if ((this = sv_pr_state.fields.this) != -1) {
		PR_RESET_PARAMS (&sv_pr_state);
		P_INT (&sv_pr_state, 0) = E_POINTER (self, this);
		P_INT (&sv_pr_state, 1) = 0;
		P_INT (&sv_pr_state, 2) = E_POINTER (other, this);
	}
	PR_ExecuteProgram (&sv_pr_state, SVfunc (self, touch));
}

static inline void
sv_pr_use (edict_t *self, edict_t *other)
{
}

static inline void
sv_pr_think (edict_t *self)
{
	pr_int_t    this;

	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, self);
	*sv_globals.other = 0;
	if ((this = sv_pr_state.fields.this) != -1) {
		PR_RESET_PARAMS (&sv_pr_state);
		P_INT (&sv_pr_state, 0) = E_POINTER (self, this);
		P_INT (&sv_pr_state, 1) = 0;
		P_INT (&sv_pr_state, 2) = 0;
	}
	PR_ExecuteProgram (&sv_pr_state, SVfunc (self, think));
}

static inline void
sv_pr_blocked (edict_t *self, edict_t *other)
{
	pr_int_t    this;

	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, self);
	*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, other);
	if ((this = sv_pr_state.fields.this) != -1) {
		PR_RESET_PARAMS (&sv_pr_state);
		P_INT (&sv_pr_state, 0) = E_POINTER (self, this);
		P_INT (&sv_pr_state, 1) = 0;
		P_INT (&sv_pr_state, 2) = E_POINTER (other, this);
	}
	PR_ExecuteProgram (&sv_pr_state, SVfunc (self, blocked));
}

#endif // __sv_progs_h
