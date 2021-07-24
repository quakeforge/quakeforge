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

#include "QF/msg.h"
#include "QF/progs.h"	// for PR_RESMAP
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sound.h"

#include "QF/plugin/vid_render.h"	//FIXME
#include "QF/scene/entity.h"

#include "client/entities.h"
#include "client/temp_entities.h"

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

static void
CL_TEnts_Precache (int phase)
{
	if (!phase) {
		return;
	}
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
}

void
CL_TEnts_Init (void)
{
	QFS_GamedirCallback (CL_TEnts_Precache);
	CL_TEnts_Precache (1);
	for (int i = 0; i < 360; i++) {
		float       ang = i * M_PI / 360;
		beam_rolls[i] = (vec4f_t) { sin (ang), 0, 0, cos (ang) };
	}
}

void
CL_Init_Entity (entity_t *ent)
{
	if (ent->transform) {
		Transform_Delete (ent->transform);
	}
	memset (ent, 0, sizeof (*ent));

	ent->transform = Transform_New (0);
	ent->renderer.skin = 0;
	QuatSet (1.0, 1.0, 1.0, 1.0, ent->renderer.colormod);
	ent->animation.pose1 = ent->animation.pose2 = -1;
}

static tent_t *
new_temp_entity (void)
{
	tent_t     *tent = PR_RESNEW_NC (temp_entities);
	tent->ent.transform = 0;
	tent->next = 0;
	CL_Init_Entity (&tent->ent);
	return tent;
}

static void
free_temp_entities (tent_t *tents)
{
	tent_t    **t = &tents;

	while (*t) {
		Transform_Delete ((*t)->ent.transform);//FIXME reuse?
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
		tent_t     *t;

		for (t = b->tents; t; t = t->next) {
			r_funcs->R_RemoveEfrags (&t->ent);
			t->ent.visibility.efrag = 0;
		}
		free_temp_entities (b->tents);
		b->tents = 0;
	}
}

static inline void
beam_setup (beam_t *b, qboolean transform, double time, TEntContext_t *ctx)
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
		tent->ent.renderer.model = b->model;
		if (transform) {
			seed = seed * BEAM_SEED_PRIME;
			Transform_SetLocalTransform (tent->ent.transform, scale,
										 qmulf (rotation,
												beam_rolls[seed % 360]),
										 position);
		} else {
			Transform_SetLocalPosition (tent->ent.transform, position);
		}
		r_funcs->R_AddEfrags (&ctx->worldModel->brush, &tent->ent);
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

	MSG_ReadCoordV (net_message, &start[0]);//FIXME
	MSG_ReadCoordV (net_message, &end[0]);//FIXME
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
parse_tent (qmsg_t *net_message, double time, TEntContext_t *ctx,
			TE_Effect type)
{
	dlight_t   *dl;
	tent_obj_t *to;
	explosion_t *ex;
	int         colorStart, colorLength;
	quat_t      color;
	vec3_t      position;
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
			MSG_ReadCoordV (net_message, position);
			r_funcs->particles->R_BloodPuffEffect (position, count);
			break;
		case TE_Explosion:
			MSG_ReadCoordV (net_message, position);

			// particles
			r_funcs->particles->R_ParticleExplosion (position);

			// light
			dl = r_funcs->R_AllocDlight (0);
			if (dl) {
				VectorCopy (position, dl->origin);
				dl->radius = 350;
				dl->die = time + 0.5;
				dl->decay = 300;
				QuatSet (0.86, 0.31, 0.24, 0.7, dl->color);
				//FIXME? nq: QuatSet (1.0, 0.5, 0.25, 0.7, dl->color);
			}

			// sound
			S_StartSound (-1, 0, cl_sfx_r_exp3, position, 1, 1);

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
			ex->tent->ent.renderer.model = cl_spr_explod;
			Transform_SetLocalPosition (ex->tent->ent.transform,//FIXME
										(vec4f_t) {VectorExpand (position), 1});
			break;
		case TE_Explosion2:
			MSG_ReadCoordV (net_message, position);
			colorStart = MSG_ReadByte (net_message);
			colorLength = MSG_ReadByte (net_message);
			S_StartSound (-1, 0, cl_sfx_r_exp3, position, 1, 1);
			r_funcs->particles->R_ParticleExplosion2 (position, colorStart,
													  colorLength);
			dl = r_funcs->R_AllocDlight (0);
			if (!dl)
				break;
			VectorCopy (position, dl->origin);
			dl->radius = 350;
			dl->die = time + 0.5;
			dl->decay = 300;
			colorStart = (colorStart + (rand () % colorLength)) * 3;
			VectorScale (&r_data->vid->palette[colorStart], 1.0 / 255.0,
						 dl->color);
			dl->color[3] = 0.7;
			break;
		case TE_Explosion3:
			MSG_ReadCoordV (net_message, position);
			MSG_ReadCoordV (net_message, color);		// OUCH!
			color[3] = 0.7;
			r_funcs->particles->R_ParticleExplosion (position);
			S_StartSound (-1, 0, cl_sfx_r_exp3, position, 1, 1);
			dl = r_funcs->R_AllocDlight (0);
			if (dl) {
				VectorCopy (position, dl->origin);
				dl->radius = 350;
				dl->die = time + 0.5;
				dl->decay = 300;
				QuatCopy (color, dl->color);
			}
			break;
		case TE_Gunshot1:
			MSG_ReadCoordV (net_message, position);
			r_funcs->particles->R_GunshotEffect (position, 20);
			break;
		case TE_Gunshot2:
			count = MSG_ReadByte (net_message) * 20;
			MSG_ReadCoordV (net_message, position);
			r_funcs->particles->R_GunshotEffect (position, count);
			break;
		case TE_KnightSpike:
			MSG_ReadCoordV (net_message, position);
			r_funcs->particles->R_KnightSpikeEffect (position);
			S_StartSound (-1, 0, cl_sfx_knighthit, position, 1, 1);
			break;
		case TE_LavaSplash:
			MSG_ReadCoordV (net_message, position);
			r_funcs->particles->R_LavaSplash (position);
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
			MSG_ReadCoordV (net_message, position);

			// light
			dl = r_funcs->R_AllocDlight (0);
			if (dl) {
				VectorCopy (position, dl->origin);
				dl->radius = 150;
				dl->die = time + 0.1;
				dl->decay = 200;
				QuatSet (0.25, 0.40, 0.65, 1, dl->color);
			}

			r_funcs->particles->R_LightningBloodEffect (position);
			break;
		case TE_Spike:
			MSG_ReadCoordV (net_message, position);
			r_funcs->particles->R_SpikeEffect (position);
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
			MSG_ReadCoordV (net_message, position);
			r_funcs->particles->R_SuperSpikeEffect (position);
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
			MSG_ReadCoordV (net_message, position);
			r_funcs->particles->R_BlobExplosion (position);

			S_StartSound (-1, 0, cl_sfx_r_exp3, position, 1, 1);
			break;
		case TE_Teleport:
			MSG_ReadCoordV (net_message, position);
			r_funcs->particles->R_TeleportSplash (position);
			break;
		case TE_WizSpike:
			MSG_ReadCoordV (net_message, position);
			r_funcs->particles->R_WizSpikeEffect (position);
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
	byte        type = MSG_ReadByte (net_message);
	parse_tent (net_message, time, ctx, nqEffects[type]);
}

// the effect type is a byte so a max of 256 values
static const TE_Effect qwEffects[256] = {
	[TE_qwSpike]          = TE_Spike,
	[TE_qwSuperSpike]     = TE_SuperSpike,
	[TE_qwGunshot]        = TE_Gunshot2,
	[TE_qwExplosion]      = TE_Explosion,
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
			Transform_SetLocalRotation (t->ent.transform,
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
	entity_t   *ent;

	for (to = &cl_explosions; *to; ) {
		ex = &(*to)->to.ex;
		ent = &ex->tent->ent;
		f = 10 * (time - ex->start);
		if (f >= ent->renderer.model->numframes) {
			tent_obj_t *_to;
			r_funcs->R_RemoveEfrags (ent);
			ent->visibility.efrag = 0;
			free_temp_entities (ex->tent);
			_to = *to;
			*to = _to->next;
			free_tent_object (_to);
			continue;
		}
		to = &(*to)->next;

		ent->animation.frame = f;
		if (!ent->visibility.efrag) {
			r_funcs->R_AddEfrags (&ctx->worldModel->brush, ent);
		}
	}
}

void
CL_UpdateTEnts (double time, TEntContext_t *ctx)
{
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
	int         i, count, color;
	vec3_t      org, dir;

	MSG_ReadCoordV (net_message, org);
	for (i = 0; i < 3; i++)
		dir[i] = ((signed char) MSG_ReadByte (net_message)) * (15.0 / 16.0);
	count = MSG_ReadByte (net_message);
	color = MSG_ReadByte (net_message);

	if (count == 255)
		r_funcs->particles->R_ParticleExplosion (org);
	else
		r_funcs->particles->R_RunParticleEffect (org, dir, color, count);
}

void
CL_ClearProjectiles (void)
{
	tent_t     *tent;

	for (tent = cl_projectiles; tent; tent = tent->next) {
		r_funcs->R_RemoveEfrags (&tent->ent);
		tent->ent.visibility.efrag = 0;
	}
	free_temp_entities (cl_projectiles);
	cl_projectiles = 0;
}

/*
	Nails are passed as efficient temporary entities
*/
void
CL_ParseProjectiles (qmsg_t *net_message, qboolean nail2, TEntContext_t *ctx)
{
	tent_t     *tent;
	tent_t     *head = 0, **tail = &head;
	byte		bits[6];
	int			i, c, j, num;
	entity_t   *pr;
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

		pr = &tent->ent;
		pr->renderer.model = cl_spike;
		pr->renderer.skin = 0;
		position[0] = ((bits[0] + ((bits[1] & 15) << 8)) << 1) - 4096;
		position[1] = (((bits[1] >> 4) + (bits[2] << 4)) << 1) - 4096;
		position[2] = ((bits[3] + ((bits[4] & 15) << 8)) << 1) - 4096;
		angles[0] = (bits[4] >> 4) * (360.0 / 16.0);
		angles[1] = bits[5] * (360.0 / 256.0);
		angles[2] = 0;
		CL_TransformEntity (&tent->ent, 1, angles, position);

		r_funcs->R_AddEfrags (&ctx->worldModel->brush, &tent->ent);
	}

	*tail = cl_projectiles;
	cl_projectiles = head;
}
