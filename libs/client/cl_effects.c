/*
	cl_effect.c

	Client side effect management

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/3/11

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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/ecs.h"
#include "QF/render.h"

#include "QF/plugin/vid_render.h"	//FIXME
#include "QF/scene/entity.h"
#include "QF/scene/light.h"
#include "QF/scene/scene.h"

#include "client/entities.h"
#include "client/effects.h"
#include "client/particles.h"
#include "client/world.h"

ecs_system_t effect_system;

static psystem_t *cl_tsystem;

static void
cl_destroy_trail (void *comp, ecs_registry_t *reg)
{
	auto trail = *(uint32_t *) comp;
	R_Trail_Destroy (cl_tsystem, trail);
}

const component_t effect_components[effect_comp_count] = {
	[effect_light] = {
		.size = sizeof (uint32_t),
		.name = "effect light",
	},
	[effect_muzzleflash] = {
		.size = sizeof (uint32_t),
		.name = "muzzle flash",
	},
	[effect_trail] = {
		.size = sizeof (uint32_t),
		.name = "effect trail",
		.destroy = cl_destroy_trail,
	},
};

#define c_light (effect_system.base + effect_light)
#define c_trail (effect_system.base + effect_trail)

static bool
has_light (entity_t ent)
{
	return Ent_HasComponent (ent.id, c_light, ent.reg);
}

static uint32_t
get_light (entity_t ent)
{
	return *(uint32_t *) Ent_GetComponent (ent.id, c_light, ent.reg);
}

static void
set_light (entity_t ent, uint32_t light)
{
	Ent_SetComponent (ent.id, c_light, ent.reg, &light);
}

static uint32_t
attach_light_ent (entity_t ent)
{
	uint32_t light = nullent;
	if (has_light (ent)) {
		light = get_light (ent);
	}
	if (!ECS_EntValid (light, ent.reg)) {
		light = ECS_NewEntity (ent.reg);
		set_light (ent, light);
	}
	return light;
}

static bool
has_trail (entity_t ent)
{
	return Ent_HasComponent (ent.id, c_trail, ent.reg);
}

static uint32_t
get_trail (entity_t ent)
{
	if (!has_trail (ent)) {
		return nullent;
	}
	return *(uint32_t *) Ent_GetComponent (ent.id, c_trail, ent.reg);
}

static void
set_trail (entity_t ent, uint32_t trail)
{
	Ent_SetComponent (ent.id, c_trail, ent.reg, &trail);
}

void
CL_NewDlight (entity_t ent, vec4f_t org, int effects, byte glow_size,
			  byte glow_color, double time)
{
	static vec4f_t normal = {0.4, 0.2, 0.05, 0.7};
	static vec4f_t red = {0.5, 0.05, 0.05, 0.7};
	static vec4f_t blue = {0.05, 0.05, 0.5, 0.7};
	static vec4f_t purple = {0.5, 0.05, 0.5, 0.7};

	effects &= EF_BLUE | EF_RED | EF_BRIGHTLIGHT | EF_DIMLIGHT;
	if (!effects) {
		if (!glow_size)
			return;
	}

	float       radius = 0;
	float       die = time + 0.1;
	vec4f_t color = normal;

	if (effects & (EF_BLUE | EF_RED | EF_BRIGHTLIGHT | EF_DIMLIGHT)) {
		radius = 200 + (rand () & 31);
		if (effects & EF_BRIGHTLIGHT) {
			radius += 200;
			org[2] += 16;
		}
		if (effects & EF_DIMLIGHT)
			if (effects & ~EF_DIMLIGHT)
				radius -= 100;

		switch (effects & (EF_RED | EF_BLUE)) {
			case EF_RED | EF_BLUE:  color = purple; break;
			case EF_RED:            color = red; break;
			case EF_BLUE:           color = blue; break;
			default:                color = normal; break;
		}
	}

	if (glow_size) {
		radius += glow_size < 128 ? glow_size * 8.0 : (glow_size - 256) * 8.0;
		if (glow_color) {
			if (glow_color == 255) {
				color = (vec4f_t) { 1, 1, 1, 0.7 };
			} else {
				byte        *tempcolor;

				tempcolor = (byte *) &d_8to24table[glow_color];
				VectorScale (tempcolor, 1 / 255.0, color);
				color[3] = 0.7;
			}
		}
	}

	uint32_t light = attach_light_ent (ent);
	Ent_SetComponent (light, ent.base + scene_dynlight, ent.reg, &(dlight_t) {
		.origin = org,
		.color = color,
		.radius = radius,
		.die = die,
	});
	Light_LinkLight (cl_world.scene->lights, light);
}

void
CL_ModelEffects (entity_t ent, int glow_color, double time)
{
	transform_t transform = Entity_Transform (ent);
	auto renderer = Entity_GetRenderer (ent);
	model_t    *model = renderer->model;
	vec4f_t    *old_origin = Ent_GetComponent (ent.id, + ent.base + scene_old_origin, ent.reg);
	vec4f_t     ent_origin = Transform_GetWorldPosition (transform);

	// add automatic particle trails
	if (model->effects & ME_ROCKET) {
		uint32_t light = attach_light_ent (ent);
		Ent_SetComponent (light, ent.base + scene_dynlight, ent.reg, &(dlight_t) {
			.origin = ent_origin,
			//FIXME VectorCopy (r_firecolor, dl->color);
			.color = { 0.9, 0.7, 0.0, 0.7 },
			.radius = 200,
			.die = time + 0.1,
		});
		Light_LinkLight (cl_world.scene->lights, light);
		clp_funcs->RocketTrail (*old_origin, ent_origin);
		if (cl_tsystem) {
			uint32_t trail = get_trail (ent);
			if (R_Trail_Valid (cl_tsystem, trail)) {
				R_Trail_Update (cl_tsystem, trail, ent_origin);
			} else {
				set_trail (ent, R_Trail_Create (cl_tsystem, 30, ent_origin));
			}
		}
		renderer->noshadows = 1;
	} else if (model->effects & ME_GRENADE)
		clp_funcs->GrenadeTrail (*old_origin, ent_origin);
	else if (model->effects & ME_GIB)
		clp_funcs->BloodTrail (*old_origin, ent_origin);
	else if (model->effects & ME_ZOMGIB)
		clp_funcs->SlightBloodTrail (*old_origin, ent_origin);
	else if (model->effects & ME_TRACER)
		clp_funcs->WizTrail (*old_origin, ent_origin);
	else if (model->effects & ME_TRACER2)
		clp_funcs->FlameTrail (*old_origin, ent_origin);
	else if (model->effects & ME_TRACER3)
		clp_funcs->VoorTrail (*old_origin, ent_origin);
	else if (model->effects & ME_GLOWTRAIL)
		clp_funcs->GlowTrail (*old_origin, ent_origin, glow_color);
}

void
CL_EntityEffects (entity_t ent, entity_state_t *state, double time)
{
	qfZoneScoped (true);
	transform_t transform = Entity_Transform (ent);
	vec4f_t     position = Transform_GetWorldPosition (transform);
	if (state->effects & EF_BRIGHTFIELD)
		clp_funcs->EntityParticles (position);
	if (state->effects & EF_MUZZLEFLASH) {
		vec4f_t     fv = Transform_Forward (transform);
		CL_MuzzleFlash (ent, position, fv, 16, time);
	}
}

void
CL_Effects_Init (void)
{
	qfZoneScoped (true);
	if (r_funcs->TrailSystem) {
		cl_tsystem = r_funcs->TrailSystem ();
	}
}
