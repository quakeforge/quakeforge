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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <math.h>
#include <stdlib.h>

#include "QF/console.h"
#include "QF/model.h"
#include "QF/msg.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "cl_ents.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "cl_tent.h"
#include "client.h"
#include "compat.h"
#include "r_dynamic.h"

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
static tent_t *cl_projectiles;

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
	memset (ent, 0, sizeof (*ent));

	ent->colormap = vid.colormap8;
	VectorSet (1.0, 1.0, 1.0, ent->colormod);
	ent->colormod[3] = 1.0;
	ent->scale = 1.0;
	ent->lerpflags |= LERP_RESETMOVE | LERP_RESETANIM;
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
		}
		temp_entities[i].next = 0;
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

		tent_objects = malloc (TEMP_BATCH * sizeof (tent_t));
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
			t->ent.efrag = 0;
		free_temp_entities (to->to.beam.tents);
	}
	free_tent_objects (cl_beams);
	cl_beams = 0;

	for (to = cl_explosions; to; to = to->next) {
		for (t = to->to.ex.tent; t; t = t->next)
			t->ent.efrag = 0;
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
			R_RemoveEfrags (&t->ent);
			t->ent.efrag = 0;
		}
		free_temp_entities (b->tents);
		b->tents = 0;
	}
}

static inline void
beam_setup (beam_t *b)
{
	tent_t     *tent;
	float       forward, pitch, yaw, d;
	int         ent_count;
	vec3_t      dist, org;

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
	while (ent_count--) {
		tent = new_temp_entity ();
		tent->next = b->tents;
		b->tents = tent;

		VectorMultAdd (org, d, dist, tent->ent.origin);
		d += 1.0;
		tent->ent.model = b->model;
		tent->ent.angles[0] = pitch;
		tent->ent.angles[1] = yaw;
		R_AddEfrags (&tent->ent);
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

	to = new_tent_object ();
	to->next = cl_beams;
	cl_beams = to;
	b = &to->to.beam;
	b->tents = 0;

	beam_clear (b);
	b->entity = ent;
	b->model = m;
	b->endtime = cl.time + 0.2;
	b->seed = rand ();
	VectorCopy (end, b->end);
	if (b->entity != cl.viewentity) {
		// this will be done in CL_UpdateBeams
		VectorCopy (start, b->start);
		beam_setup (b);
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
	int         cnt = -1;
	vec3_t      pos;
	sfx_t      *spike_sound[] = {
		cl_sfx_ric3, cl_sfx_ric3, cl_sfx_ric2, cl_sfx_ric1,
	};

	type = MSG_ReadByte (net_message);
	switch (type) {
		case TE_WIZSPIKE:				// spike hitting wall
			MSG_ReadCoordV (net_message, pos);
			R_WizSpikeEffect (pos);
			S_StartSound (-1, 0, cl_sfx_wizhit, pos, 1, 1);
			break;

		case TE_KNIGHTSPIKE:			// spike hitting wall
			MSG_ReadCoordV (net_message, pos);
			R_KnightSpikeEffect (pos);
			S_StartSound (-1, 0, cl_sfx_knighthit, pos, 1, 1);
			break;

		case TE_SPIKE:					// spike hitting wall
			MSG_ReadCoordV (net_message, pos);
			R_SpikeEffect (pos);
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
			R_SuperSpikeEffect (pos);
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
			R_ParticleExplosion (pos);

			// light
			dl = R_AllocDlight (0);
			if (dl) {
				VectorCopy (pos, dl->origin);
				dl->radius = 350;
				dl->die = cl.time + 0.5;
				dl->decay = 300;
				dl->color[0] = 0.86;
				dl->color[1] = 0.31;
				dl->color[2] = 0.24;
				dl->color[3] = 0.7;
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
			ex->tent->ent.model = cl_spr_explod;
			break;

		case TE_TAREXPLOSION:			// tarbaby explosion
			MSG_ReadCoordV (net_message, pos);
			R_BlobExplosion (pos);

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

		// PGM 01/21/97
		case TE_BEAM:					// grappling hook beam
			CL_ParseBeam (cl_mod_beam);
			break;
		// PGM 01/21/97

		case TE_LAVASPLASH:
			MSG_ReadCoordV (net_message, pos);
			R_LavaSplash (pos);
			break;

		case TE_TELEPORT:
			MSG_ReadCoordV (net_message, pos);
			R_TeleportSplash (pos);
			break;

		case TE_EXPLOSION2:				// color mapped explosion
			MSG_ReadCoordV (net_message, pos);
			colorStart = MSG_ReadByte (net_message);
			colorLength = MSG_ReadByte (net_message);
			S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
			R_ParticleExplosion2 (pos, colorStart, colorLength);
			dl = R_AllocDlight (0);
			if (!dl)
				break;
			VectorCopy (pos, dl->origin);
			dl->radius = 350;
			dl->die = cl.time + 0.5;
			dl->decay = 300;
			colorStart = (colorStart + (rand () % colorLength)) * 3;
			dl->color[0] = vid.palette[colorStart] * (1.0 / 255.0);
			dl->color[1] = vid.palette[colorStart + 1] * (1.0 / 255.0);
			dl->color[2] = vid.palette[colorStart + 2] * (1.0 / 255.0);
			dl->color[3] = 0.7;
			break;

		case TE_GUNSHOT:				// bullet hitting wall
			cnt = MSG_ReadByte (net_message) * 20;
			MSG_ReadCoordV (net_message, pos);
			R_GunshotEffect (pos, cnt);
			break;

		case TE_BLOOD:					// bullet hitting body
			cnt = MSG_ReadByte (net_message) * 20;
			MSG_ReadCoordV (net_message, pos);
			R_BloodPuffEffect (pos, cnt);
			break;

		case TE_LIGHTNINGBLOOD:			// lightning hitting body
			MSG_ReadCoordV (net_message, pos);

			// light
			dl = R_AllocDlight (0);
			if (dl) {
				VectorCopy (pos, dl->origin);
				dl->radius = 150;
				dl->die = cl.time + 0.1;
				dl->decay = 200;
				dl->color[0] = 0.25;
				dl->color[1] = 0.40;
				dl->color[2] = 0.65;
				dl->color[3] = 1;
			}

			R_LightningBloodEffect (pos);
			break;

		default:
			Sys_Error ("CL_ParseTEnt: bad type %d", type);
	}
}

#define BEAM_SEED_INTERVAL 72
#define BEAM_SEED_PRIME 3191

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
			VectorCopy (cl.simorg, b->start);
			beam_setup (b);
		}

		seed = b->seed + ((int) (cl.time * BEAM_SEED_INTERVAL) %
						  BEAM_SEED_INTERVAL);

		// add new entities for the lightning
		for (t = b->tents; t; t = t->next) {
			seed = seed * BEAM_SEED_PRIME;
			t->ent.angles[2] = seed % 360;
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
		if (f >= ent->model->numframes) {
			tent_obj_t *_to;
			R_RemoveEfrags (ent);
			ent->efrag = 0;
			free_temp_entities (ex->tent);
			_to = *to;
			*to = _to->next;
			_to->next = tent_objects;
			tent_objects = _to;
			continue;
		}
		to = &(*to)->next;

		ent->frame = f;
		if (!ent->efrag)
			R_AddEfrags (ent);
	}
}

void
CL_UpdateTEnts (void)
{
	CL_UpdateBeams ();
	CL_UpdateExplosions ();
}

void
CL_ClearProjectiles (void)
{
	tent_t     *tent;

	for (tent = cl_projectiles; tent; tent = tent->next) {
		R_RemoveEfrags (&tent->ent);
		tent->ent.efrag = 0;
	}
	free_temp_entities (cl_projectiles);
	cl_projectiles = 0;
}

/*
	Nails are passed as efficient temporary entities
*/
void
CL_ParseProjectiles (qboolean nail2)
{
	tent_t     *tent;
	tent_t     *head = 0, **tail = &head;
	byte		bits[6];
	int			i, c, j, num;
	entity_t   *pr;

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
		pr->model = cl.model_precache[cl_spikeindex];
		pr->colormap = vid.colormap8;
		pr->origin[0] = ((bits[0] + ((bits[1] & 15) << 8)) << 1) - 4096;
		pr->origin[1] = (((bits[1] >> 4) + (bits[2] << 4)) << 1) - 4096;
		pr->origin[2] = ((bits[3] + ((bits[4] & 15) << 8)) << 1) - 4096;
		pr->angles[0] = (bits[4] >> 4) * (360.0 / 16.0);
		pr->angles[1] = bits[5] * (360.0 / 256.0);
		pr->angles[2] = 0;

		R_AddEfrags (&tent->ent);
	}

	*tail = cl_projectiles;
	cl_projectiles = head;
}
