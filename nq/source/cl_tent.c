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
static const char rcsid[] = 
	"$Id$";

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

#include "QF/console.h"
#include "QF/model.h"
#include "QF/msg.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "client.h"
#include "r_dynamic.h"

#define	MAX_BEAMS	8
#define MAX_BEAM_ENTS 20

typedef struct {
	int         entity;
	struct model_s *model;
	float       endtime;
	vec3_t      start, end;
	entity_t    ent_list[MAX_BEAM_ENTS];
	int         ent_count;
} beam_t;

beam_t      cl_beams[MAX_BEAMS];

#define	MAX_EXPLOSIONS	8

typedef struct {
	float       start;
	entity_t    ent;
} explosion_t;

explosion_t cl_explosions[MAX_EXPLOSIONS];

sfx_t      *cl_sfx_wizhit;
sfx_t      *cl_sfx_knighthit;
sfx_t      *cl_sfx_tink1;
sfx_t      *cl_sfx_ric1;
sfx_t      *cl_sfx_ric2;
sfx_t      *cl_sfx_ric3;
sfx_t      *cl_sfx_r_exp3;

model_t    *cl_mod_bolt;
model_t    *cl_mod_bolt2;
model_t    *cl_mod_bolt3;
model_t    *cl_spr_explod;

static const particle_effect_t prot_to_rend[]={
	PE_SPIKE,				// TE_SPIKE
	PE_SUPERSPIKE,			// TE_SUPERSPIKE
	PE_GUNSHOT,				// TE_GUNSHOT
	PE_UNKNOWN,				// TE_EXPLOSION
	PE_UNKNOWN,				// TE_TAREXPLOSION
	PE_UNKNOWN,				// TE_LIGHTNING1
	PE_UNKNOWN,				// TE_LIGHTNING2
	PE_WIZSPIKE,			// TE_WIZSPIKE
	PE_KNIGHTSPIKE,			// TE_KNIGHTSPIKE
	PE_UNKNOWN,				// TE_LIGHTNING3
	PE_UNKNOWN,				// TE_LAVASPLASH
	PE_UNKNOWN,				// TE_TELEPORT
	PE_BLOOD,				// TE_BLOOD
	PE_LIGHTNINGBLOOD,		// TE_LIGHTNINGBLOOD
	PE_UNKNOWN,				// TE_IMPLOSION
	PE_UNKNOWN,				// TE_RAILTRAIL
	PE_UNKNOWN,				// TE_EXPLOSION2
	PE_UNKNOWN,				// TE_BEAM
};


void
CL_TEnts_Init (void)
{
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
}

void
CL_Init_Entity (entity_t *ent)
{
	memset (ent, 0, sizeof (*ent));

	ent->colormap = vid.colormap8;
	ent->colormod[0] = ent->colormod[1] = ent->colormod[2] =
		ent->colormod[3] = 1.0;
	ent->scale = 1.0;
	ent->pose1 = ent->pose2 = -1;
}

void
CL_ClearTEnts (void)
{
	int i;

	memset (&cl_beams, 0, sizeof (cl_beams));
	memset (&cl_explosions, 0, sizeof (cl_explosions));
	for (i = 0; i < MAX_BEAMS; i++) {
		int j;
		for (j = 0; j < MAX_BEAM_ENTS; j++) {
			CL_Init_Entity(&cl_beams[i].ent_list[j]);
		}
	}
	for (i = 0; i < MAX_EXPLOSIONS; i++) {
		CL_Init_Entity(&cl_explosions[i].ent);
	}
}

explosion_t *
CL_AllocExplosion (void)
{
	float       time;
	int         index, i;

	for (i = 0; i < MAX_EXPLOSIONS; i++)
		if (!cl_explosions[i].ent.model)
			return &cl_explosions[i];
	// find the oldest explosion
	time = cl.time;
	index = 0;

	for (i = 0; i < MAX_EXPLOSIONS; i++)
		if (cl_explosions[i].start < time) {
			time = cl_explosions[i].start;
			index = i;
		}
	return &cl_explosions[index];
}

void
CL_ParseBeam (model_t *m)
{
	beam_t     *b;
	int         ent, i;
	vec3_t      start, end;

	ent = MSG_ReadShort (net_message);

	MSG_ReadCoordV (net_message, start);
	MSG_ReadCoordV (net_message, end);

	// override any beam with the same entity
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)
		if (b->entity == ent) {
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	// find a free beam
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
		if (!b->model || b->endtime < cl.time) {
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}
	Con_Printf ("beam list overflow!\n");
}

void
CL_ParseTEnt (void)
{
	byte        type;
	dlight_t   *dl;
	int         colorStart, colorLength;
	int         cnt = -1;
	explosion_t *ex;
	vec3_t      pos;

	type = MSG_ReadByte (net_message);
	//XXX FIXME this is to get around an nq/qw protocol collision
	if (1) {
		if (type == TE_BLOOD)
			type = TE_EXPLOSION2;
		else if (type == TE_LIGHTNINGBLOOD)
			type = TE_BEAM;
	}
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

			switch (rand () % 20) {
				case 19:
					S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
					break;
				case 18:
					S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
					break;
				case 17:
				case 16:
					S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
					break;
				default:
					S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
			}
			break;

		case TE_SUPERSPIKE:			// super spike hitting wall
			MSG_ReadCoordV (net_message, pos);
			R_SuperSpikeEffect (pos);

			switch (rand () % 20) {
				case 19:
					S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
					break;
				case 18:
					S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
					break;
				case 17:
				case 16:
					S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
					break;
				default:
					S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
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
			}

			// sound
			S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);

			// sprite
			ex = CL_AllocExplosion ();
			VectorCopy (pos, ex->ent.origin);
			ex->start = cl.time;
			ex->ent.model = cl_spr_explod;
			break;

		case TE_TAREXPLOSION:			// tarbaby explosion
			MSG_ReadCoordV (net_message, pos);
			R_BlobExplosion (pos);

			S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
			break;

		case TE_LIGHTNING1:			// lightning bolts
			CL_ParseBeam (cl_mod_bolt);
			break;

		case TE_LIGHTNING2:			// lightning bolts
			CL_ParseBeam (cl_mod_bolt2);
			break;

		case TE_LIGHTNING3:			// lightning bolts
			CL_ParseBeam (cl_mod_bolt3);
			break;

		// PGM 01/21/97
		case TE_BEAM:				// grappling hook beam
			CL_ParseBeam (Mod_ForName ("progs/beam.mdl", true));
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

		case TE_EXPLOSION2:			// color mapped explosion
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
			dl->color[0] = vid_basepal[(colorStart + (rand() % colorLength)) *
									   3] * (1.0 / 255.0);
			dl->color[1] = vid_basepal[(colorStart + (rand() % colorLength)) *
									   3 + 1] * (1.0 / 255.0);
			dl->color[2] = vid_basepal[(colorStart + (rand() % colorLength)) *
									   3 + 2] * (1.0 / 255.0);
			break;

		case TE_GUNSHOT:				// bullet hitting wall
			MSG_ReadCoordV (net_message, pos);
			R_GunshotEffect (pos, 20);
			break;

		case TE_BLOOD:					// bullet hitting body
			cnt = MSG_ReadByte (net_message) * 20;
			MSG_ReadCoordV (net_message, pos);
			R_BloodPuffEffect (pos, cnt);
			break;

		case TE_LIGHTNINGBLOOD:		// lightning hitting body
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
			}

			R_LightningBloodEffect (pos);
			break;

		default:
			Sys_Error ("CL_ParseTEnt: bad type");
	}
}

void
CL_UpdateBeams (void)
{
	beam_t     *b;
	entity_t  **ent;
	float       forward, pitch, yaw, d;
	int         i;
	vec3_t      dist, org;

	// update lightning
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.viewentity) {
			VectorCopy (cl_entities[cl.viewentity].origin, b->start);
		}
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
		b->ent_count = 0;
		while (d > 0 && b->ent_count < MAX_BEAM_ENTS) {
			ent = R_NewEntity ();
			if (!ent)
				return;
			*ent = &b->ent_list[b->ent_count++];
			VectorCopy (org, (*ent)->origin);
			(*ent)->model = b->model;
			(*ent)->angles[0] = pitch;
			(*ent)->angles[1] = yaw;
			(*ent)->angles[2] = rand () % 360;

			VectorMA(org, 30, dist, org);
			d -= 30;
		}
	}
}

void
CL_UpdateExplosions (void)
{
	entity_t   **ent;
	explosion_t *ex;
	int         f, i;

	for (i = 0, ex = cl_explosions; i < MAX_EXPLOSIONS; i++, ex++) {
		if (!ex->ent.model)
			continue;
		f = 10 * (cl.time - ex->start);
		if (f >= ex->ent.model->numframes) {
			ex->ent.model = NULL;
			continue;
		}

		ent = R_NewEntity ();
		if (!ent)
			return;
		ex->ent.frame = f;
		*ent = &ex->ent;
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

	if (count == 255) {
		R_ParticleExplosion (org);
	} else {
		R_RunParticleEffect (org, dir, color, count);
	}
}
