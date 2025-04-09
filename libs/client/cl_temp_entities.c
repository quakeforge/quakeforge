/*
	cl_temp_entities.c

	Client side temporary entity management

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/3/10

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

#include "QF/darray.h"
#include "QF/msg.h"
#include "QF/progs.h"	// for PR_RESMAP
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sound.h"

#include "QF/plugin/vid_render.h"	//FIXME
									//
#include "QF/scene/entity.h"
#include "QF/scene/light.h"
#include "QF/scene/scene.h"

#include "client/effects.h"
#include "client/entities.h"
#include "client/particles.h"
#include "client/temp_entities.h"
#include "client/world.h"

typedef struct tent_s {
	struct tent_s *next;
	entity_t    ent;
} tent_t;

typedef struct {
	int         entity;
	struct model_s *model;
	float       endtime;
	vec4f_t     start, end;
	vec4f_t     rotation;
	tent_t     *tents;
	int         seed;
} beam_t;

#define BEAM_SEED_INTERVAL 72
#define BEAM_SEED_PRIME 3191

typedef struct {
	float       start;
	tent_t     *tent;
} explosion_t;

typedef struct tent_obj_s {
	struct tent_obj_s *next;
	union {
		beam_t      beam;
		explosion_t ex;
	} to;
} tent_obj_t;

static PR_RESMAP (tent_t) temp_entities;
static PR_RESMAP (tent_obj_t) tent_objects;
static tent_obj_t *cl_beams;
static tent_obj_t *cl_explosions;

static tent_t *cl_projectiles;

static sfx_t *cl_sfx_wizhit;
static sfx_t *cl_sfx_knighthit;
static sfx_t *cl_sfx_tink1;
static sfx_t *cl_sfx_r_exp3;
static sfx_t *cl_sfx_ric[4];

static model_t *cl_mod_beam;
static model_t *cl_mod_bolt;
static model_t *cl_mod_bolt2;
static model_t *cl_mod_bolt3;
static model_t *cl_spr_explod;
static model_t *cl_spike;

static vec4f_t beam_rolls[360];

typedef struct tempent_s DARRAY_TYPE (entity_t) tempent_t;
static tempent_t light_entities = DARRAY_STATIC_INIT (32);

void
CL_TEnts_Precache (void)
{
	qfZoneScoped (true);
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric[3] = S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric[2] = S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric[1] = S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_ric[0] = cl_sfx_ric[1];
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav");

	cl_mod_bolt = Mod_ForName ("progs/bolt.mdl", true);
	cl_mod_bolt2 = Mod_ForName ("progs/bolt2.mdl", true);
	cl_mod_bolt3 = Mod_ForName ("progs/bolt3.mdl", true);
	cl_spr_explod = Mod_ForName ("progs/s_explod.spr", true);
	cl_mod_beam = Mod_ForName ("progs/beam.mdl", false);
	cl_spike = Mod_ForName ("progs/spike.mdl", false);

	if (!cl_mod_beam) {
		cl_mod_beam = cl_mod_bolt;
	}
	S_SetAmbient (AMBIENT_WATER, S_PrecacheSound ("ambience/water1.wav"));
	S_SetAmbient (AMBIENT_SKY, S_PrecacheSound ("ambience/wind2.wav"));
}

static void
cl_tents_precache (int phase, void *data)
{
	if (!phase) {
		return;
	}
	CL_TEnts_Precache ();
}

static void
cl_tents_shutdown (void *data)
{
	free (light_entities.a);
}

void
CL_TEnts_Init (void)
{
	qfZoneScoped (true);
	Sys_RegisterShutdown (cl_tents_shutdown, 0);
	QFS_GamedirCallback (cl_tents_precache, 0);
	for (int i = 0; i < 360; i++) {
		float       ang = i * M_PI / 360;
		beam_rolls[i] = (vec4f_t) { sin (ang), 0, 0, cos (ang) };
	}
}

void
CL_Init_Entity (entity_t ent)
{
	qfZoneScoped (true);
	auto renderer = Entity_GetRenderer (ent);
	auto animation = Entity_GetAnimation (ent);
	byte       *active = Ent_GetComponent (ent.id, ent.base + scene_active, ent.reg);
	vec4f_t    *old_origin = Ent_GetComponent (ent.id, ent.base + scene_old_origin, ent.reg);
	memset (animation, 0, sizeof (*animation));
	memset (renderer, 0, sizeof (*renderer));
	*active = 1;
	*old_origin = (vec4f_t) {0, 0, 0, 1};

	QuatSet (1.0, 1.0, 1.0, 1.0, renderer->colormod);
	animation->pose1 = animation->pose2 = -1;
}

static tent_t *
new_temp_entity (void)
{
	tent_t     *tent = PR_RESNEW_NC (temp_entities);
	tent->ent = Scene_CreateEntity (cl_world.scene);
	tent->next = 0;
	CL_Init_Entity (tent->ent);
	return tent;
}

static void
free_temp_entities (tent_t *tents)
{
	tent_t    **t = &tents;

	while (*t) {
		Scene_DestroyEntity (cl_world.scene, (*t)->ent);//FIXME reuse?
		t = &(*t)->next;
	}
	*t = temp_entities._free;
	temp_entities._free = tents;
}

static tent_obj_t *
new_tent_object (void)
{
	tent_obj_t *tobj = PR_RESNEW_NC (tent_objects);
	tobj->next = 0;
	return tobj;
}

static void
free_tent_object (tent_obj_t *tobj)
{
	tobj->next = tent_objects._free;
	tent_objects._free = tobj;
}

void
CL_ClearTEnts (void)
{
	PR_RESRESET (temp_entities);
	PR_RESRESET (tent_objects);
	cl_beams = 0;
	cl_explosions = 0;
}

static inline void
beam_clear (beam_t *b)
{
	if (b->tents) {
		free_temp_entities (b->tents);
		b->tents = 0;
	}
}

static inline void
beam_setup (beam_t *b, bool settransform, double time, TEntContext_t *ctx)
{
	tent_t     *tent;
	float       d;
	int         ent_count;
	vec4f_t     dist, org;
	vec4f_t     rotation;
	vec4f_t     scale = { 1, 1, 1, 1 };
	unsigned    seed;

	// calculate pitch and yaw
	dist = b->end - b->start;

	// FIXME interpolation may be off when passing through the -x axis
	if (dist[0] < 0 && dist[1] == 0 && dist[2] == 0) {
		// anti-parallel with the +x axis, so assome 180 degree rotation around
		// the z-axis
		rotation = (vec4f_t) { 0, 0, 1, 0 };
	} else {
		rotation = qrotf ((vec4f_t) { 1, 0, 0, 0 }, dist);
	}
	b->rotation = rotation;

	// add new entities for the lightning
	org = b->start;
	d = magnitudef (dist)[0];
	dist = normalf (dist) * 30;
	ent_count = ceil (d / 30);
	d = 0;

	seed = b->seed + ((int) (time * BEAM_SEED_INTERVAL) % BEAM_SEED_INTERVAL);

	while (ent_count--) {
		tent = new_temp_entity ();
		tent->next = b->tents;
		b->tents = tent;

		vec4f_t     position = org + d * dist;
		d += 1.0;
		transform_t transform = Entity_Transform (tent->ent);
		auto renderer = Entity_GetRenderer (tent->ent);
		renderer->model = b->model;
		if (settransform) {
			seed = seed * BEAM_SEED_PRIME;
			Transform_SetLocalTransform (transform, scale,
										 qmulf (rotation,
												beam_rolls[seed % 360]),
										 position);
		} else {
			Transform_SetLocalPosition (transform, position);
		}
		R_AddEfrags (&cl_world.scene->worldmodel->brush, tent->ent);
	}
}

static void
CL_ParseBeam (qmsg_t *net_message, model_t *m, double time, TEntContext_t *ctx)
{
	tent_obj_t *to;
	beam_t     *b;
	int         ent;
	vec4f_t     start, end;

	ent = MSG_ReadShort (net_message);

	MSG_ReadCoordV (net_message, (vec_t*)&start);//FIXME
	MSG_ReadCoordV (net_message, (vec_t*)&end);//FIXME
	start[3] = end[3] = 1;//FIXME

	to = 0;
	if (ent) {
		for (to = cl_beams; to; to = to->next) {
			if (to->to.beam.entity == ent) {
				break;
			}
		}
	}
	if (!to) {
		to = new_tent_object ();
		to->next = cl_beams;
		cl_beams = to;
		to->to.beam.tents = 0;
		to->to.beam.entity = ent;
	}
	b = &to->to.beam;

	beam_clear (b);
	b->model = m;
	b->endtime = time + 0.2;
	b->seed = rand ();
	b->end = end;
	if (b->entity != ctx->playerEntity) {
		// this will be done in CL_UpdateBeams
		b->start = start;
		beam_setup (b, true, time, ctx);
	}
}

static void
free_stale_entities (void)
{
	size_t i, j;
	for (i = 0, j = 0; i < light_entities.size; i++) {
		auto ent = light_entities.a[i];
		if (Ent_HasComponent (ent.id, ent.base + scene_dynlight, ent.reg)) {
			if (j != i) {
				light_entities.a[j] = ent;
			}
			j++;
		} else {
			ECS_DelEntity (ent.reg, ent.id);
		}
	}
	light_entities.size = j;
}

static void
spawn_light (vec4f_t position, vec4f_t color, float radius, float die,
			 float decay)
{
	entity_t    ent = {
		.reg = cl_world.scene->reg,
		.id = ECS_NewEntity (cl_world.scene->reg),
		.base = cl_world.scene->base,
	};
	DARRAY_APPEND (&light_entities, ent);

	Ent_SetComponent (ent.id, ent.base + scene_dynlight, ent.reg, &(dlight_t) {
		.origin = position,
		.color = color,
		.radius = radius,
		.die = die,
		.decay = decay,
	});
	Light_LinkLight (cl_world.scene->lights, ent.id);
}

static void
parse_tent (qmsg_t *net_message, double time, TEntContext_t *ctx,
			TE_Effect type)
{
	tent_obj_t *to;
	explosion_t *ex;
	int         colorStart, colorLength;
	vec4f_t     color;
	vec4f_t     position = {0, 0, 0, 1};
	int         count;
	const char *name;

	switch (type) {
		case TE_NoEffect:
			// invalid mapping, can't do anything
			break;
		case TE_Beam:
			CL_ParseBeam (net_message, cl_mod_beam, time, ctx);
			break;
		case TE_Blood:
			count = MSG_ReadByte (net_message) * 20;
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			clp_funcs->BloodPuffEffect (position, count);
			break;
		case TE_Explosion1:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME

			// sprite
			to = new_tent_object ();
			to->next = cl_explosions;
			cl_explosions = to;
			ex = &to->to.ex;
			ex->tent = new_temp_entity ();

			ex->start = time;
			//FIXME need better model management
			if (!cl_spr_explod->cache.data) {
				cl_spr_explod = Mod_ForName ("progs/s_explod.spr", true);
			}
			transform_t transform = Entity_Transform (ex->tent->ent);
			auto renderer = Entity_GetRenderer (ex->tent->ent);
			renderer->model = cl_spr_explod;
			Transform_SetLocalPosition (transform, position);
			color = (vec4f_t) {0.86, 0.31, 0.24, 0.7};
			goto TE_Explosion_no_sprite;
		case TE_Explosion:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			color = (vec4f_t) {1.0, 0.5, 0.25, 0.7};
TE_Explosion_no_sprite:
			// particles
			clp_funcs->ParticleExplosion (position);

			spawn_light (position, color, 250, time + 0.5, 300);

			// sound
			S_StartSound (-1, 0, cl_sfx_r_exp3, position, 1, 1);
			break;
		case TE_Explosion2:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			colorStart = MSG_ReadByte (net_message);
			colorLength = MSG_ReadByte (net_message);
			S_StartSound (-1, 0, cl_sfx_r_exp3, position, 1, 1);
			clp_funcs->ParticleExplosion2 (position, colorStart, colorLength);

			colorStart = (colorStart + (rand () % colorLength)) * 3;
			VectorScale (&r_data->vid->palette[colorStart], 1.0 / 255.0, color);
			color[3] = 0.7;
			spawn_light (position, color, 350, time + 0.5, 300);
			break;
		case TE_Explosion3:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			MSG_ReadCoordV (net_message, (vec_t*)&color);	//FIXME OUCH!
			color[3] = 0.7;
			clp_funcs->ParticleExplosion (position);
			S_StartSound (-1, 0, cl_sfx_r_exp3, position, 1, 1);

			spawn_light (position, color, 350, time + 0.5, 300);
			break;
		case TE_Gunshot1:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			clp_funcs->GunshotEffect (position, 20);
			break;
		case TE_Gunshot2:
			count = MSG_ReadByte (net_message) * 20;
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			clp_funcs->GunshotEffect (position, count);
			break;
		case TE_KnightSpike:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			clp_funcs->KnightSpikeEffect (position);
			S_StartSound (-1, 0, cl_sfx_knighthit, position, 1, 1);
			break;
		case TE_LavaSplash:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			clp_funcs->LavaSplash (position);
			break;
		case TE_Lightning1:
			CL_ParseBeam (net_message, cl_mod_bolt, time, ctx);
			break;
		case TE_Lightning2:
			CL_ParseBeam (net_message, cl_mod_bolt2, time, ctx);
			break;
		case TE_Lightning3:
			CL_ParseBeam (net_message, cl_mod_bolt3, time, ctx);
			break;
		case TE_Lightning4:
			name = MSG_ReadString (net_message);
			CL_ParseBeam (net_message, Mod_ForName (name, true), time, ctx);
			break;
		case TE_LightningBlood:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME

			color = (vec4f_t) {0.25, 0.40, 0.65, 1};
			spawn_light (position, color, 150, time + 0.1, 200);

			clp_funcs->LightningBloodEffect (position);
			break;
		case TE_Spike:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			clp_funcs->SpikeEffect (position);
			{
				int		i;
				sfx_t  *sound;

				i = (rand () % 20) - 16;
				if (i >= 0) {
					sound = cl_sfx_ric[i];
				} else {
					sound = cl_sfx_tink1;
				}
				S_StartSound (-1, 0, sound, position, 1, 1);
			}
			break;
		case TE_SuperSpike:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			clp_funcs->SuperSpikeEffect (position);
			{
				int		i;
				sfx_t  *sound;

				i = (rand () % 20) - 16;
				if (i >= 0) {
					sound = cl_sfx_ric[i];
				} else {
					sound = cl_sfx_tink1;
				}
				S_StartSound (-1, 0, sound, position, 1, 1);
			}
			break;
		case TE_TarExplosion:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			clp_funcs->BlobExplosion (position);

			S_StartSound (-1, 0, cl_sfx_r_exp3, position, 1, 1);
			break;
		case TE_Teleport:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			clp_funcs->TeleportSplash (position);
			break;
		case TE_WizSpike:
			MSG_ReadCoordV (net_message, (vec_t*)&position);//FIXME
			clp_funcs->WizSpikeEffect (position);
			S_StartSound (-1, 0, cl_sfx_wizhit, position, 1, 1);
			break;
	}
}

// the effect type is a byte so a max of 256 values
static const TE_Effect nqEffects[256] = {
	[TE_nqSpike]        = TE_Spike,
	[TE_nqSuperSpike]   = TE_SuperSpike,
	[TE_nqGunshot]      = TE_Gunshot1,
	[TE_nqExplosion]    = TE_Explosion,
	[TE_nqTarExplosion] = TE_TarExplosion,
	[TE_nqLightning1]   = TE_Lightning1,
	[TE_nqLightning2]   = TE_Lightning2,
	[TE_nqWizSpike]     = TE_WizSpike,
	[TE_nqKnightSpike]  = TE_KnightSpike,
	[TE_nqLightning3]   = TE_Lightning3,
	[TE_nqLavaSplash]   = TE_LavaSplash,
	[TE_nqTeleport]     = TE_Teleport,
	[TE_nqExplosion2]   = TE_Explosion2,
	[TE_nqBeam]         = TE_Beam,
	[TE_nqExplosion3]   = TE_Explosion3,
	[TE_nqLightning4]   = TE_Lightning4,
};

void
CL_ParseTEnt_nq (qmsg_t *net_message, double time, TEntContext_t *ctx)
{
	qfZoneScoped (true);
	byte        type = MSG_ReadByte (net_message);
	parse_tent (net_message, time, ctx, nqEffects[type]);
}

// the effect type is a byte so a max of 256 values
static const TE_Effect qwEffects[256] = {
	[TE_qwSpike]          = TE_Spike,
	[TE_qwSuperSpike]     = TE_SuperSpike,
	[TE_qwGunshot]        = TE_Gunshot2,
	[TE_qwExplosion]      = TE_Explosion1,
	[TE_qwTarExplosion]   = TE_TarExplosion,
	[TE_qwLightning1]     = TE_Lightning1,
	[TE_qwLightning2]     = TE_Lightning2,
	[TE_qwWizSpike]       = TE_WizSpike,
	[TE_qwKnightSpike]    = TE_KnightSpike,
	[TE_qwLightning3]     = TE_Lightning3,
	[TE_qwLavaSplash]     = TE_LavaSplash,
	[TE_qwTeleport]       = TE_Teleport,
	[TE_qwBlood]          = TE_Blood,
	[TE_qwLightningBlood] = TE_LightningBlood,
	[TE_qwExplosion2]     = TE_Explosion2,
	[TE_qwBeam]           = TE_Beam,
};

void
CL_ParseTEnt_qw (qmsg_t *net_message, double time, TEntContext_t *ctx)
{
	byte        type = MSG_ReadByte (net_message);
	parse_tent (net_message, time, ctx, qwEffects[type]);
}

static void
CL_UpdateBeams (double time, TEntContext_t *ctx)
{
	tent_obj_t **to;
	beam_t     *b;
	unsigned    seed;
	tent_t     *t;

	// update lightning
	for (to = &cl_beams; *to; ) {
		b = &(*to)->to.beam;
		if (!b->endtime)
			continue;
		if (!b->model || b->endtime < time) {
			tent_obj_t *_to;
			b->endtime = 0;
			beam_clear (b);
			_to = *to;
			*to = _to->next;
			free_tent_object (_to);
			continue;
		}
		to = &(*to)->next;

		// if coming from the player, update the start position
		if (b->entity == ctx->playerEntity) {
			beam_clear (b);
			b->start = ctx->simorg;
			beam_setup (b, false, time, ctx);
		}

		seed = b->seed + ((int) (time * BEAM_SEED_INTERVAL) %
						  BEAM_SEED_INTERVAL);

		// add new entities for the lightning
		for (t = b->tents; t; t = t->next) {
			seed = seed * BEAM_SEED_PRIME;
			transform_t transform = Entity_Transform (t->ent);
			Transform_SetLocalRotation (transform,
										qmulf (b->rotation,
											   beam_rolls[seed % 360]));
		}
	}
}

static void
CL_UpdateExplosions (double time, TEntContext_t *ctx)
{
	int         f;
	tent_obj_t **to;
	explosion_t *ex;
	entity_t    ent;

	for (to = &cl_explosions; *to; ) {
		ex = &(*to)->to.ex;
		ent = ex->tent->ent;
		f = 10 * (time - ex->start);
		auto renderer = Entity_GetRenderer (ent);
		auto animation = Entity_GetAnimation (ent);
		if (f >= renderer->model->numframes) {
			tent_obj_t *_to;
			free_temp_entities (ex->tent);
			_to = *to;
			*to = _to->next;
			free_tent_object (_to);
			continue;
		}
		to = &(*to)->next;

		animation->frame = f;
		R_AddEfrags (&cl_world.scene->worldmodel->brush, ent);
	}
}

void
CL_UpdateTEnts (double time, TEntContext_t *ctx)
{
	qfZoneNamedN (ut_zone, "CL_UpdateTEnts", true);
	free_stale_entities ();
	CL_UpdateBeams (time, ctx);
	CL_UpdateExplosions (time, ctx);
}

/*
	CL_ParseParticleEffect

	Parse an effect out of the server message
*/
void
CL_ParseParticleEffect (qmsg_t *net_message)
{
	qfZoneScoped (true);
	int         i, count, color;
	vec4f_t     org = {0, 0, 0, 1}, dir = {};

	MSG_ReadCoordV (net_message, (vec_t*)&org);//FIXME
	for (i = 0; i < 3; i++)
		dir[i] = ((signed char) MSG_ReadByte (net_message)) * (15.0 / 16.0);
	count = MSG_ReadByte (net_message);
	color = MSG_ReadByte (net_message);

	if (count == 255)
		clp_funcs->ParticleExplosion (org);
	else
		clp_funcs->RunParticleEffect (org, dir, color, count);
}

void
CL_ClearProjectiles (void)
{
	free_temp_entities (cl_projectiles);
	cl_projectiles = 0;
}

/*
	Nails are passed as efficient temporary entities
*/
void
CL_ParseProjectiles (qmsg_t *net_message, bool nail2, TEntContext_t *ctx)
{
	tent_t     *tent;
	tent_t     *head = 0, **tail = &head;
	byte		bits[6];
	int			i, c, j, num;
	entity_t    pr;
	vec4f_t     position = { 0, 0, 0, 1 };
	vec3_t      angles;

	c = MSG_ReadByte (net_message);

	for (i = 0; i < c; i++) {
		if (nail2)
			num = MSG_ReadByte (net_message);
		else
			num = 0;
		(void) num;	//FIXME

		for (j = 0; j < 6; j++)
			bits[j] = MSG_ReadByte (net_message);

		tent = new_temp_entity ();
		*tail = tent;
		tail = &tent->next;

		pr = tent->ent;
		auto renderer = Entity_GetRenderer (pr);
		renderer->model = cl_spike;
		position[0] = ((bits[0] + ((bits[1] & 15) << 8)) << 1) - 4096;
		position[1] = (((bits[1] >> 4) + (bits[2] << 4)) << 1) - 4096;
		position[2] = ((bits[3] + ((bits[4] & 15) << 8)) << 1) - 4096;
		angles[0] = (bits[4] >> 4) * (360.0 / 16.0);
		angles[1] = bits[5] * (360.0 / 256.0);
		angles[2] = 0;
		CL_TransformEntity (tent->ent, 1, angles, position);

		R_AddEfrags (&cl_world.scene->worldmodel->brush, pr);
	}

	*tail = cl_projectiles;
	cl_projectiles = head;
}

#define c_muzzleflash (effect_system.base + effect_muzzleflash)

static bool
has_muzzleflash (entity_t ent)
{
	return Ent_HasComponent (ent.id, c_muzzleflash, ent.reg);
}

static uint32_t
get_muzzleflash (entity_t ent)
{
	return *(uint32_t *) Ent_GetComponent (ent.id, c_muzzleflash, ent.reg);
}

static void
set_muzzleflash (entity_t ent, uint32_t light)
{
	Ent_SetComponent (ent.id, c_muzzleflash, ent.reg, &light);
}

void
CL_MuzzleFlash (entity_t ent, vec4f_t position, vec4f_t fv, float zoffset,
				double time)
{
	qfZoneScoped (true);
	// spawn a new entity so the light doesn't mess with the owner
	uint32_t light = nullent;
	if (has_muzzleflash (ent)) {
		light = get_muzzleflash (ent);
	}
	if (!ECS_EntValid (light, ent.reg)) {
		light = ECS_NewEntity (ent.reg);
		set_muzzleflash (ent, light);
	}
	DARRAY_APPEND (&light_entities, ((entity_t) {
		.reg = ent.reg,
		.id = light,
		.base = ent.base,
	}));

	Ent_SetComponent (light, ent.base + scene_dynlight, ent.reg, &(dlight_t) {
		.origin = position + 18 * fv + zoffset * (vec4f_t) {0, 0, 1, 0},
		.color = { 0.2, 0.1, 0.05, 0.7 },
		.radius = 200 + (rand () & 31),
		.die = time + 0.1,
		.minlight = 32,
	});
	Light_LinkLight (cl_world.scene->lights, light);
}
