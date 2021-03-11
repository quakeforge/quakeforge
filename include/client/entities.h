/*
	entities.h

	Entity management

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/6/28

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

#ifndef __client_entities_h
#define __client_entities_h

#include "QF/qtypes.h"
#include "QF/simd/types.h"

// entity_state_t is the information conveyed from the server
// in an update message
typedef struct entity_state_s {
	int         number;			// edict index
	unsigned    flags;			// nolerp, etc

	vec4f_t     origin;
	vec4f_t     velocity;
	vec3_t      angles;
	uint16_t    modelindex;
	uint16_t    frame;
	int         weaponframe;
	int         effects;
	byte        colormap;
	byte        skinnum;

	// QSG 2
	byte        alpha;
	byte        scale;
	byte        glow_size;
	byte        glow_color;
	byte        colormod;
} entity_state_t;

typedef struct {
	entity_state_t * const baseline;
	entity_state_t * const * const frame;
	const int   num_frames;
	const int   num_entities;
} entstates_t;

extern entstates_t nq_entstates;
extern entstates_t qw_entstates;

extern vec3_t ent_colormod[256];

struct entity_s;
void CL_TransformEntity (struct entity_s *ent, const vec3_t angles);

#endif//__client_entities_h
