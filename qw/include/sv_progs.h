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

#include "progs.h"
#include "progdefs.h"

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

#endif // __sv_progs_h
