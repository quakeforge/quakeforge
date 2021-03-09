/*
	cl_tent.c

	client side temporary entities

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include <math.h>
#include <stdlib.h>

#include "QF/entity.h"
#include "QF/model.h"
#include "QF/msg.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "QF/plugin/vid_render.h"

#include "compat.h"

#include "nq/include/client.h"

typedef struct tent_s {
	struct tent_s *next;
	entity_t    ent;
} tent_t;

#define TEMP_BATCH 64
static tent_t *temp_entities = 0;

typedef struct {
	int         entity;
	struct model_s *model;
	float       endtime;
	vec3_t      start, end;
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

static tent_obj_t *tent_objects;
static tent_obj_t *cl_beams;
static tent_obj_t *cl_explosions;

static sfx_t *cl_sfx_wizhit;
static sfx_t *cl_sfx_knighthit;
static sfx_t *cl_sfx_tink1;
static sfx_t *cl_sfx_ric1;
static sfx_t *cl_sfx_ric2;
static sfx_t *cl_sfx_ric3;
static sfx_t *cl_sfx_r_exp3;

static model_t *cl_mod_beam;
static model_t *cl_mod_bolt;
static model_t *cl_mod_bolt2;
static model_t *cl_mod_bolt3;
static model_t *cl_spr_explod;

static void
CL_TEnts_Precache (int phase)
{
	if (!phase)
		return;
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric1 = S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric2 = S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric3 = S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav");

	cl_mod_bolt = Mod_ForName ("progs/bolt.mdl", true);
	cl_mod_bolt2 = Mod_ForName ("progs/bolt2.mdl", true);
	cl_mod_bolt3 = Mod_ForName ("progs/bolt3.mdl", true);
	cl_spr_explod = Mod_ForName ("progs/s_explod.spr", true);
	cl_mod_beam = Mod_ForName ("progs/beam.mdl", false);
	if (!cl_mod_beam)
		cl_mod_beam = cl_mod_bolt;
}

void
CL_TEnts_Init (void)
{
	QFS_GamedirCallback (CL_TEnts_Precache);
	CL_TEnts_Precache (1);
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
	ent->scale = 1.0;
	ent->animation.pose1 = ent->animation.pose2 = -1;
}

static tent_t *
new_temp_entity (void)
{
	tent_t     *tent;
	if (!temp_entities) {
		int         i;

		temp_entities = malloc (TEMP_BATCH * sizeof (tent_t));
		for (i = 0; i < TEMP_BATCH - 1; i++) {
			temp_entities[i].next = &temp_entities[i + 1];
			temp_entities[i].ent.transform = 0;
		}
		temp_entities[i].next = 0;
		temp_entities[i].ent.transform = 0;
	}
	tent = temp_entities;
	temp_entities = tent->next;
	tent->next = 0;
	CL_Init_Entity (&tent->ent);
	return tent;
}

static void
free_temp_entities (tent_t *tents)
{
	tent_t    **t = &tents;

	while (*t)
		t = &(*t)->next;
	*t = temp_entities;
	temp_entities = tents;
}

static tent_obj_t *
new_tent_object (void)
{
	tent_obj_t *tobj;
	if (!tent_objects) {
		int         i;

		tent_objects = malloc (TEMP_BATCH * sizeof (tent_obj_t));
		for (i = 0; i < TEMP_BATCH - 1; i++)
			tent_objects[i].next = &tent_objects[i + 1];
		tent_objects[i].next = 0;
	}
	tobj = tent_objects;
	tent_objects = tobj->next;
	tobj->next = 0;
	return tobj;
}

static void
free_tent_objects (tent_obj_t *tobjs)
{
	tent_obj_t **t = &tobjs;

	while (*t)
		t = &(*t)->next;
	*t = tent_objects;
	tent_objects = tobjs;
}

void
CL_ClearTEnts (void)
{
	tent_t     *t;
	tent_obj_t *to;

	for (to = cl_beams; to; to = to->next) {
		for (t = to->to.beam.tents; t; t = t->next)
			t->ent.visibility.efrag = 0;
		free_temp_entities (to->to.beam.tents);
	}
	free_tent_objects (cl_beams);
	cl_beams = 0;

	for (to = cl_explosions; to; to = to->next) {
		for (t = to->to.ex.tent; t; t = t->next)
			t->ent.visibility.efrag = 0;
		free_temp_entities (to->to.ex.tent);
	}
	free_tent_objects (cl_explosions);
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
beam_setup (beam_t *b, qboolean transform)
{
	tent_t     *tent;
	float       forward, pitch, yaw, d;
	int         ent_count;
	vec3_t      dist, org, ang;
	unsigned    seed;

	// calculate pitch and yaw
	VectorSubtract (b->end, b->start, dist);

	if (dist[1] == 0 && dist[0] == 0) {
		yaw = 0;
		if (dist[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	} else {
		yaw = (int) (atan2 (dist[1], dist[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (dist[0] * dist[0] + dist[1] * dist[1]);
		pitch = (int) (atan2 (dist[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	// add new entities for the lightning
	VectorCopy (b->start, org);
	d = VectorNormalize (dist);
	VectorScale (dist, 30, dist);
	ent_count = ceil (d / 30);
	d = 0;

	seed = b->seed + ((int) (cl.time * BEAM_SEED_INTERVAL) %
					  BEAM_SEED_INTERVAL);

	ang[ROLL] = 0;
	while (ent_count--) {
		tent = new_temp_entity ();
		tent->next = b->tents;
		b->tents = tent;

		VectorMultAdd (org, d, dist, tent->ent.origin);
		d += 1.0;
		tent->ent.renderer.model = b->model;
		ang[PITCH] = pitch;
		ang[YAW] = yaw;
		if (transform) {
			seed = seed * BEAM_SEED_PRIME;
			ang[ROLL] = seed % 360;
			CL_TransformEntity (&tent->ent, ang, true);
		}
		VectorCopy (ang, tent->ent.angles);
		r_funcs->R_AddEfrags (&cl.worldmodel->brush, &tent->ent);
	}
}

static void
CL_ParseBeam (model_t *m)
{
	tent_obj_t *to;
	beam_t     *b;
	int         ent;
	vec3_t      start, end;

	ent = MSG_ReadShort (net_message);

	MSG_ReadCoordV (net_message, start);
	MSG_ReadCoordV (net_message, end);

	to = 0;
	if (ent) {
		for (to = cl_beams; to; to = to->next)
			if (to->to.beam.entity == ent)
				break;
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
	b->endtime = cl.time + 0.2;
	b->seed = rand ();
	VectorCopy (end, b->end);
	if (b->entity != cl.viewentity) {
		// this will be done in CL_UpdateBeams
		VectorCopy (start, b->start);
		beam_setup (b, true);
	}
}

void
CL_ParseTEnt (void)
{
	byte        type;
	dlight_t   *dl;
	tent_obj_t *to;
	explosion_t *ex;
	int         colorStart, colorLength;
	quat_t      col;
	vec3_t      pos;
	sfx_t      *spike_sound[] = {
		cl_sfx_ric3, cl_sfx_ric3, cl_sfx_ric2, cl_sfx_ric1,
	};

	type = MSG_ReadByte (net_message);
	switch (type) {
		case TE_WIZSPIKE:				// spike hitting wall
			MSG_ReadCoordV (net_message, pos);
			r_funcs->particles->R_WizSpikeEffect (pos);
			S_StartSound (-1, 0, cl_sfx_wizhit, pos, 1, 1);
			break;

		case TE_KNIGHTSPIKE:			// spike hitting wall
			MSG_ReadCoordV (net_message, pos);
			r_funcs->particles->R_KnightSpikeEffect (pos);
			S_StartSound (-1, 0, cl_sfx_knighthit, pos, 1, 1);
			break;

		case TE_SPIKE:					// spike hitting wall
			MSG_ReadCoordV (net_message, pos);
			r_funcs->particles->R_SpikeEffect (pos);
			{
				int		i;
				sfx_t  *sound;

				i = (rand () % 20) - 16;
				if (i >= 0)
					sound = spike_sound[i];
				else
					sound = cl_sfx_tink1;
				S_StartSound (-1, 0, sound, pos, 1, 1);
			}
			break;

		case TE_SUPERSPIKE:				// super spike hitting wall
			MSG_ReadCoordV (net_message, pos);
			r_funcs->particles->R_SuperSpikeEffect (pos);
			{
				int		i;
				sfx_t  *sound;

				i = (rand () % 20) - 16;
				if (i >= 0)
					sound = spike_sound[i];
				else
					sound = cl_sfx_tink1;
				S_StartSound (-1, 0, sound, pos, 1, 1);
			}
			break;

		case TE_EXPLOSION:				// rocket explosion
			// particles
			MSG_ReadCoordV (net_message, pos);
			r_funcs->particles->R_ParticleExplosion (pos);

			// light
			dl = r_funcs->R_AllocDlight (0);
			if (dl) {
				VectorCopy (pos, dl->origin);
				dl->radius = 350;
				dl->die = cl.time + 0.5;
				dl->decay = 300;
				QuatSet (1.0, 0.5, 0.25, 0.7, dl->color);
			}

			// sound
			S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);

			// sprite
			to = new_tent_object ();
			to->next = cl_explosions;
			cl_explosions = to;
			ex = &to->to.ex;
			ex->tent = new_temp_entity ();

			VectorCopy (pos, ex->tent->ent.origin);
			ex->start = cl.time;
			//FIXME need better model management
			if (!cl_spr_explod->cache.data)
				cl_spr_explod = Mod_ForName ("progs/s_explod.spr", true);
			ex->tent->ent.renderer.model = cl_spr_explod;
			CL_TransformEntity (&ex->tent->ent, ex->tent->ent.angles, true);
			break;

		case TE_TAREXPLOSION:			// tarbaby explosion
			MSG_ReadCoordV (net_message, pos);
			r_funcs->particles->R_BlobExplosion (pos);

			S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
			break;

		case TE_LIGHTNING1:				// lightning bolts
			CL_ParseBeam (cl_mod_bolt);
			break;

		case TE_LIGHTNING2:				// lightning bolts
			CL_ParseBeam (cl_mod_bolt2);
			break;

		case TE_LIGHTNING3:				// lightning bolts
			CL_ParseBeam (cl_mod_bolt3);
			break;

		case TE_LIGHTNING4NEH:			// Nehahra lightning
			CL_ParseBeam (Mod_ForName (MSG_ReadString (net_message), true));
			break;

		// PGM 01/21/97
		case TE_BEAM:					// grappling hook beam
			CL_ParseBeam (cl_mod_beam);
			break;
		// PGM 01/21/97

		case TE_LAVASPLASH:
			MSG_ReadCoordV (net_message, pos);
			r_funcs->particles->R_LavaSplash (pos);
			break;

		case TE_TELEPORT:
			MSG_ReadCoordV (net_message, pos);
			r_funcs->particles->R_TeleportSplash (pos);
			break;

		case TE_EXPLOSION2:				// color mapped explosion
			MSG_ReadCoordV (net_message, pos);
			colorStart = MSG_ReadByte (net_message);
			colorLength = MSG_ReadByte (net_message);
			S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
			r_funcs->particles->R_ParticleExplosion2 (pos, colorStart,
													  colorLength);
			dl = r_funcs->R_AllocDlight (0);
			if (!dl)
				break;
			VectorCopy (pos, dl->origin);
			dl->radius = 350;
			dl->die = cl.time + 0.5;
			dl->decay = 300;
			colorStart = (colorStart + (rand () % colorLength)) * 3;
			VectorScale (&r_data->vid->palette[colorStart], 1.0 / 255.0,
						 dl->color);
			dl->color[3] = 0.7;
			break;

		case TE_EXPLOSION3:				// Nehahra colored light explosion
			MSG_ReadCoordV (net_message, pos);
			MSG_ReadCoordV (net_message, col);			// OUCH!
			col[3] = 0.7;
			r_funcs->particles->R_ParticleExplosion (pos);
			S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
			dl = r_funcs->R_AllocDlight (0);
			if (dl) {
				VectorCopy (pos, dl->origin);
				dl->radius = 350;
				dl->die = cl.time + 0.5;
				dl->decay = 300;
				QuatCopy (col, dl->color);
			}
			break;

		case TE_GUNSHOT:				// bullet hitting wall
			MSG_ReadCoordV (net_message, pos);
			r_funcs->particles->R_GunshotEffect (pos, 20);
			break;

		default:
			Sys_Error ("CL_ParseTEnt: bad type %d", type);
	}
}

static void
CL_UpdateBeams (void)
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
		if (!b->model || b->endtime < cl.time) {
			tent_obj_t *_to;
			b->endtime = 0;
			beam_clear (b);
			_to = *to;
			*to = _to->next;
			_to->next = tent_objects;
			tent_objects = _to;
			continue;
		}
		to = &(*to)->next;

		// if coming from the player, update the start position
		if (b->entity == cl.viewentity) {
			beam_clear (b);
			VectorCopy (cl_entities[cl.viewentity].origin, b->start);
			beam_setup (b, false);
		}

		seed = b->seed + ((int) (cl.time * BEAM_SEED_INTERVAL) %
						  BEAM_SEED_INTERVAL);

		// add new entities for the lightning
		for (t = b->tents; t; t = t->next) {
			seed = seed * BEAM_SEED_PRIME;
			t->ent.angles[ROLL] = seed % 360;
			CL_TransformEntity (&t->ent, t->ent.angles, true);
		}
	}
}

static void
CL_UpdateExplosions (void)
{
	int         f;
	tent_obj_t **to;
	explosion_t *ex;
	entity_t   *ent;

	for (to = &cl_explosions; *to; ) {
		ex = &(*to)->to.ex;
		ent = &ex->tent->ent;
		f = 10 * (cl.time - ex->start);
		if (f >= ent->renderer.model->numframes) {
			tent_obj_t *_to;
			r_funcs->R_RemoveEfrags (ent);
			ent->visibility.efrag = 0;
			free_temp_entities (ex->tent);
			_to = *to;
			*to = _to->next;
			_to->next = tent_objects;
			tent_objects = _to;
			continue;
		}
		to = &(*to)->next;

		ent->animation.frame = f;
		if (!ent->visibility.efrag)
			r_funcs->R_AddEfrags (&cl.worldmodel->brush, ent);
	}
}

void
CL_UpdateTEnts (void)
{
	CL_UpdateBeams ();
	CL_UpdateExplosions ();
}

/*
	CL_ParseParticleEffect

	Parse an effect out of the server message
*/
void
CL_ParseParticleEffect (void)
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
