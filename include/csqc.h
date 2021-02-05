/*
	csqc.h

	CSQC stuff

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

	$Id$
*/

#ifndef __csqc_h
#define __csqc_h
#include "QF/progs.h"

typedef struct {
	pr_int_t   *self;
	pr_int_t   *other;
	pr_int_t   *world;
	float      *time;
	float      *cltime;
	float      *player_localentnum;
	float      *player_localnum;
	float      *maxclients;
	float      *clientcommandframe;
	float      *servercommandframe;
	string_t   *mapname;
	float      *intermission;
	vec3_t     *v_forward;
	vec3_t     *v_up;
	vec3_t     *v_right;
	vec3_t     *view_angles;
	float      *trace_allsolid;
	float      *trace_startsolid;
	float      *trace_fraction;
	vec3_t     *trace_endpos;
	vec3_t     *trace_plane_normal;
	float      *trace_plane_dist;
	pr_int_t   *trace_ent;
	float      *trace_inopen;
	float      *trace_inwater;
	float      *input_timelength;
	vec3_t     *input_angles;
	vec3_t     *input_movevalues;
	float      *input_buttons;
	float      *input_impulse;
} cl_globals_t;

typedef struct {
	int         modelindex;
	int         absmin;
	int         absmax;
	int         entnum;
	int         drawmask;
	int         predraw;
	int         movetype;
	int         solid;
	int         origin;
	int         oldorigin;
	int         velocity;
	int         angles;
	int         avelocity;
	int         pmove_flags;
	int         classname;
	int         renderflags;
	int         model;
	int         frame;
	int         frame1time;
	int         frame2;
	int         frame2time;
	int         lerpfrac;
	int         skin;
	int         effects;
	int         mins;
	int         maxs;
	int         size;
	int         touch;
	int         think;
	int         blocked;
	int         nextthink;
	int         chain;
	int         enemy;
	int         flags;
	int         colormap;
	int         owner;
} cl_fields_t;

typedef struct {
	func_t      Init;
	func_t      Shutdown;
	func_t      UpdateView;
	func_t      WorldLoaded;
	func_t      Parse_StuffCmd;
	func_t      Parse_CenterPrint;
	func_t      Parse_Print;
	func_t      InputEvent;
	func_t      ConsoleCommand;
	func_t      Ent_Update;
	func_t      Event_Sound;
	func_t      Remove;
} cl_funcs_t;

#if TYPECHECK_PROGS
#define CSQCFIELD(e,f,t) E_var (e, PR_AccessField (&csqc_pr_state, #f, ev_##t, __FILE__, __LINE__), t)
#else
#define CSQCFIELD(e,f,t) E_var (e, cl_fields.f, t)
#endif

#define CSQCfloat(e,f)      CSQCFIELD (e, f, float)
#define CSQCstring(e,f)     CSQCFIELD (e, f, string)
#define CSQCfunc(e,f)       CSQCFIELD (e, f, func)
#define CSQCentity(e,f)     CSQCFIELD (e, f, entity)
#define CSQCvector(e,f)     (&CSQCFIELD (e, f, vector))
#define CSQCinteger(e,f)    CSQCFIELD (e, f, integer)

extern progs_t csqc_pr_state;

void CSQC_Cmds_Init (void);

#endif//__csqc_h
