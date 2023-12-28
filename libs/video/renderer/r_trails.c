/*
	r_trails.c

	Interface for trails

	Copyright (C) 2023		Bill Currie <bill@taniwha.org>

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

#include "QF/cvar.h"
#include "QF/ecs.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "QF/simd/vec4f.h"

#include "r_dynamic.h"

psystem_t   r_tsystem; //FIXME singleton
static ecs_system_t trails_system;
static trailpnt_t *trail_point_buffer;
static uint32_t trail_point_count;
static uint32_t trail_point_buffer_size;
static uint32_t allocated_blocks;	// bitmask of 64-point blocks in use

typedef struct {
	uint32_t    base;
	uint32_t    count;
} pointset_t;

enum trails_components {
	trails_pointset,

	trails_comp_count
};

static void
destroy_pointset (void *comp)
{
	pointset_t *pointset = comp;
	int base = pointset->base / 64;
	int count = pointset->count / 64;
	uint32_t mask = ((1 << count) - 1) << base;
	allocated_blocks &= ~mask;
	r_tsystem.numparticles = allocated_blocks;
}

static const component_t trails_components[trails_comp_count] = {
	[trails_pointset] = {
		.size = sizeof (pointset_t),
		.create = 0,
		.name = "pointset",
		.destroy = destroy_pointset,
	},
};

bool
R_Trail_Valid (psystem_t *system, uint32_t trailid)
{
	return ECS_EntValid (trailid, trails_system.reg);
}

uint32_t
R_Trail_Create (psystem_t *system, int num_points, vec4f_t start)
{
	num_points += 2;	// need an extra point at either end
	num_points = (num_points + 31) & ~31; // want a multiple of 32
	num_points *= 2;	// each point needs two verts

	int blocks = num_points / 64;
	uint32_t mask = (1 << blocks) - 1;
	int block_ind;
	for (block_ind = 0; block_ind <= 32 - blocks; block_ind++) {
		if (!(allocated_blocks & mask)) {
			break;
		}
		mask <<= 1;
	}
	if (allocated_blocks & mask) {
		return nullent;
	}
	allocated_blocks |= mask;
	r_tsystem.numparticles = allocated_blocks;

	uint32_t trail = ECS_NewEntity (trails_system.reg);
	pointset_t pointset = {
		.base = block_ind * 64,
		.count = num_points,
	};
	Ent_SetComponent (trail, trails_pointset, trails_system.reg, &pointset);
	for (uint32_t i = 0; i < pointset.count; i++) {
		static float bary[] = {0, 0, 1, 0, 0, 1, 0, 0};
		auto p = trail_point_buffer + pointset.base + i;
		*p = (trailpnt_t) {
			.pos = start,
			.colora = { 0xc0, 0xa0, 0x60, 0x80 },
			.colorb = { 0x30, 0x30, 0x20, 0x00 },
			.trailid = trail,
			.live = 10,
		};
		p->pos[3] = i & 1 ? 1 : -1;
		VectorCopy (bary + i % 3, p->bary);
	};
	return trail;
}

void
R_Trail_Update (psystem_t *system, uint32_t trailid, vec4f_t pos)
{
	pointset_t *p = Ent_GetComponent (trailid, trails_pointset,
									  trails_system.reg);

	trailpnt_t *points = trail_point_buffer + p->base;

	pos[3] = -1;
	vec4f_t dist = pos - points[4].pos;
	vec4f_t prev1 = points[2].pos;
	vec4f_t prev2 = points[3].pos;
	points[2].pos = pos;
	pos[3] = 1;
	points[3].pos = pos;

	points[0].pos = points[2].pos + dist;
	points[1].pos = points[3].pos + dist;

	float len = sqrt (dotf (dist, dist)[0]);
	if (len > 16) {
		for (uint32_t i = 4; i < p->count; i += 2) {
			vec4f_t t1 = points[i + 0].pos;
			vec4f_t t2 = points[i + 1].pos;
			points[i + 0].pos = prev1;
			points[i + 1].pos = prev2;
			prev1 = t1;
			prev2 = t2;

			points[i + 0].pathoffset = len;
			points[i + 1].pathoffset = len;
			if (i < p->count - 2) {
				dist = points[i + 2].pos - points[i + 0].pos;
				len = sqrt (dotf (dist, dist)[0]);
			}
		}
	} else {
		for (uint32_t i = 4; i < p->count; i += 2) {
			points[i + 0].pathoffset = len;
			points[i + 1].pathoffset = len;
			if (i < p->count - 2) {
				dist = points[i + 2].pos - points[i + 0].pos;
				len = sqrt (dotf (dist, dist)[0]);
			}
		}
	}
}

void
R_Trail_Destroy (psystem_t *system, uint32_t trailid)
{
	ECS_DelEntity (trails_system.reg, trailid);
}


void
R_ClearTrails (void)
{
	auto reg = trails_system.reg;
	for (uint32_t i = 0; i < reg->num_entities; i++) {
		uint32_t    ent = reg->entities[i];
		uint32_t    ind = Ent_Index (ent);
		if (ind == i) {
			ECS_DelEntity (reg, ent);
		}
	}
}

void
R_RunTrails (float dT)
{
}

void
R_Trails_Init_Cvars (void)
{
	Sys_Printf ("R_Trails_Init_Cvars\n");
}

static void
trails_shutdown (void *data)
{
	Sys_Printf ("trails_shutdown\n");
	ECS_DelRegistry (trails_system.reg);
	Sys_Free (trail_point_buffer, trail_point_buffer_size);
}

void
R_Trails_Init (void)
{
	Sys_Printf ("R_Trails_Init\n");
	Sys_RegisterShutdown (trails_shutdown, 0);
	auto reg = ECS_NewRegistry ("r_trails");
	trails_system.reg = reg;
	trails_system.base = ECS_RegisterComponents (reg, trails_components,
												 trails_comp_count);
	ECS_CreateComponentPools (reg);

	trail_point_count = 32*64;
	trail_point_buffer_size = trail_point_count * sizeof (trailpnt_t);
	trail_point_buffer = Sys_Alloc (trail_point_buffer_size);

	r_tsystem = (psystem_t) {
		.maxparticles = trail_point_count,
		.particles = (particle_t *) trail_point_buffer,
		.partparams = 0,//FIXME
		.partramps = 0,//FIXME
	};
}
