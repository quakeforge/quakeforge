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

	$Id$
*/

#ifndef __sv_progs_h
#define __sv_progs_h

#include "QF/progs.h"

typedef struct {
    int        *self;
    int        *other;
    int        *world;
    float      *time;
    float      *frametime;
    int        *newmis;
    float      *force_retouch;
    string_t   *mapname;
    float      *serverflags;
    float      *total_secrets;
    float      *total_monsters;
    float      *found_secrets;
    float      *killed_monsters;
    float      *parms;			// an actual array
    vec3_t     *v_forward;
    vec3_t     *v_up;
    vec3_t     *v_right;
    float      *trace_allsolid;
    float      *trace_startsolid;
    float      *trace_fraction;
    vec3_t     *trace_endpos;
    vec3_t     *trace_plane_normal;
    float      *trace_plane_dist;
    int        *trace_ent;
    float      *trace_inopen;
    float      *trace_inwater;
    int        *msg_entity;
    float      *current_skill;
} sv_globals_t;

extern sv_globals_t sv_globals;

typedef struct {
    func_t     main;
    func_t     StartFrame;
    func_t     PlayerPreThink;
    func_t     PlayerPostThink;
    func_t     ClientKill;
    func_t     ClientConnect;
    func_t     PutClientInServer;
    func_t     ClientDisconnect;
    func_t     SetNewParms;
    func_t     SetChangeParms;
} sv_funcs_t;

extern sv_funcs_t sv_funcs;

typedef struct
{
	int			modelindex;		//float
	int			absmin;			//vec3_t
	int			absmax;			//vec3_t
	int			ltime;			//float
	int			lastruntime;	//float
	int			movetype;		//float
	int			solid;			//float
	int			origin;			//vec3_t
	int			oldorigin;		//vec3_t
	int			velocity;		//vec3_t
	int			angles;			//vec3_t
	int			avelocity;		//vec3_t
	int			classname;		//string_t
	int			model;			//string_t
	int			frame;			//float
	int			skin;			//float
	int			effects;		//float
	int			mins;			//vec3_t
	int			maxs;			//vec3_t
	int			size;			//vec3_t
	int			touch;			//func_t
	int			think;			//func_t
	int			blocked;		//func_t
	int			nextthink;		//float
	int			groundentity;	//int
	int			health;			//float
	int			frags;			//float
	int			weapon;			//float
	int			weaponmodel;	//string_t
	int			weaponframe;	//float
	int			currentammo;	//float
	int			ammo_shells;	//float
	int			ammo_nails;		//float
	int			ammo_rockets;	//float
	int			ammo_cells;		//float
	int			items;			//float
	int			takedamage;		//float
	int			chain;			//int
	int			view_ofs;		//vec3_t
	int			button0;		//float
	int			button1;		//float
	int			button2;		//float
	int			impulse;		//float
	int			fixangle;		//float
	int			v_angle;		//vec3_t
	int			netname;		//string_t
	int			enemy;			//int
	int			flags;			//float
	int			colormap;		//float
	int			team;			//float
	int			teleport_time;	//float
	int			armorvalue;		//float
	int			waterlevel;		//float
	int			watertype;		//float
	int			ideal_yaw;		//float
	int			yaw_speed;		//float
	int			goalentity;		//int
	int			spawnflags;		//float
	int			dmg_take;		//float
	int			dmg_save;		//float
	int			dmg_inflictor;	//int
	int			owner;			//int
	int			message;		//string_t
	int			sounds;			//float

	int			alpha;
	int			scale;
	int			glow_size;
	int			glow_color;
	int			colormod;
} sv_fields_t;

extern sv_fields_t sv_fields;

#if TYPECHECK_PROGS
#define SVFIELD(e,f,t) E_var (e, PR_AccessField (&sv_pr_state, #f, ev_##t, __FILE__, __LINE__), t)
#else
#define SVFIELD(e,f,t) E_var (e, sv_fields.f, t)
#endif

#define PROGHEADER_CRC 54730

extern func_t EndFrame;
extern func_t SpectatorConnect;
extern func_t SpectatorThink;
extern func_t SpectatorDisconnect;
extern func_t UserInfoCallback;

#endif // __sv_progs_h
