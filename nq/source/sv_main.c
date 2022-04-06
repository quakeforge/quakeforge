/*
	sv_main.c

	@description@

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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/mathlib.h"
#include "QF/set.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"
#include "world.h"

#include "nq/include/host.h"
#include "nq/include/server.h"
#include "nq/include/sv_progs.h"

server_t    sv;
server_static_t svs;
double      sv_frametime;

char        localmodels[MAX_MODELS][6];	// inline model names for precache

int sv_protocol = PROTOCOL_FITZQUAKE;

static void
SV_Protocol_f (void)
{
	int         i;

	switch (Cmd_Argc ()) {
		case 1:
			Sys_Printf ("\"sv_protocol\" is \"%i\"\n", sv_protocol);
			break;
		case 2:
			i = atoi (Cmd_Argv (1));
			if (i != PROTOCOL_NETQUAKE && i != PROTOCOL_FITZQUAKE) {
				Sys_Printf ("sv_protocol must be %i or %i\n",
							PROTOCOL_NETQUAKE, PROTOCOL_FITZQUAKE);
			} else {
				sv_protocol = i;
				if (sv.active)
					Sys_Printf ("changes will not take effect until the next "
								"level load.\n");
			}
			break;
		default:
			Sys_Printf ("usage: sv_protocol <protocol>\n");
			break;
	}
}

void
SV_Init (void)
{
	int         i;

	SV_Progs_Init ();

	sv_maxvelocity = Cvar_Get ("sv_maxvelocity", "2000", CVAR_NONE, NULL,
							   "None");
	sv_gravity = Cvar_Get ("sv_gravity", "800", CVAR_SERVERINFO, Cvar_Info,
						   "None");
	sv_jump_any = Cvar_Get ("sv_jump_any", "1", CVAR_NONE, NULL, "None");
	sv_friction = Cvar_Get ("sv_friction", "4", CVAR_SERVERINFO, Cvar_Info,
							"None");
	//NOTE: the cl/sv clash is deliberate: dedicated server will use the right
	//vars, but client/server combo will use the one.
	sv_rollspeed = Cvar_Get ("cl_rollspeed", "200", CVAR_NONE, NULL,
							 "How quickly you straighten out after strafing");
	sv_rollangle = Cvar_Get ("cl_rollangle", "2.0", CVAR_NONE, NULL,
							 "How much your screen tilts when strafing");
	sv_edgefriction = Cvar_Get ("edgefriction", "2", CVAR_NONE, NULL, "None");
	sv_stopspeed = Cvar_Get ("sv_stopspeed", "100", CVAR_NONE, NULL, "None");
	sv_maxspeed = Cvar_Get ("sv_maxspeed", "320", CVAR_SERVERINFO, Cvar_Info,
							"None");
	sv_accelerate = Cvar_Get ("sv_accelerate", "10", CVAR_NONE, NULL, "None");
	sv_idealpitchscale = Cvar_Get ("sv_idealpitchscale", "0.8", CVAR_NONE,
								   NULL, "None");
	sv_aim = Cvar_Get ("sv_aim", "0.93", CVAR_NONE, NULL, "None");
	sv_nostep = Cvar_Get ("sv_nostep", "0", CVAR_NONE, NULL, "None");

	Cmd_AddCommand ("sv_protocol", SV_Protocol_f, "set the protocol to be "
					"used after the next map load");

	for (i = 0; i < MAX_MODELS; i++)
		snprintf (localmodels[i], sizeof (localmodels[i]), "*%i", i);
}

// EVENT MESSAGES =============================================================

/*
	SV_StartParticle

	Make sure the event gets sent to all clients
*/
void
SV_StartParticle (const vec3_t org, const vec3_t dir, int color, int count)
{
	int         i, v;

	if (sv.datagram.cursize > MAX_DATAGRAM - 16)
		return;
	MSG_WriteByte (&sv.datagram, svc_particle);
	MSG_WriteCoordV (&sv.datagram, org);
	for (i = 0; i < 3; i++) {
		v = dir[i] * 16;
		if (v > 127)
			v = 127;
		else if (v < -128)
			v = -128;
		MSG_WriteByte (&sv.datagram, v);
	}
	MSG_WriteByte (&sv.datagram, count);
	MSG_WriteByte (&sv.datagram, color);
}

/*
	SV_StartSound

	Each entity can have eight independant sound sources, like voice,
	weapon, feet, etc.

	Channel 0 is an auto-allocate channel, the others override anything
	already running on that entity/channel pair.

	An attenuation of 0 will play full volume everywhere in the level.
	Larger attenuations will drop off.  (max 4 attenuation)
*/
void
SV_StartSound (edict_t *entity, int channel, const char *sample, int volume,
			   float attenuation)
{
	int         ent, field_mask, sound_num;
	vec3_t      v;

	if (volume < 0 || volume > 255)
		Sys_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Sys_Error ("SV_StartSound: channel = %i", channel);

	if (sv.datagram.cursize > MAX_DATAGRAM - 16)
		return;

	// find precache number for sound
	for (sound_num = 1; sound_num < MAX_SOUNDS
		 && sv.sound_precache[sound_num]; sound_num++)
		if (!strcmp (sample, sv.sound_precache[sound_num]))
			break;

	if (sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num]) {
		Sys_Printf ("SV_StartSound: %s not precacheed\n", sample);
		return;
	}

	ent = NUM_FOR_EDICT (&sv_pr_state, entity);

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_ATTENUATION;

	if (ent >= 8192) {
		if (sv.protocol == PROTOCOL_NETQUAKE)
			return; //don't send any info protocol can't support
		else
			field_mask |= SND_LARGEENTITY;
	}
	if (sound_num >= 256 || channel >= 8) {
		if (sv.protocol == PROTOCOL_NETQUAKE)
			return; //don't send any info protocol can't support
		else
			field_mask |= SND_LARGESOUND;
	}

	// directed messages go to only the entity on which they are targeted
	MSG_WriteByte (&sv.datagram, svc_sound);
	MSG_WriteByte (&sv.datagram, field_mask);
	if (field_mask & SND_VOLUME)
		MSG_WriteByte (&sv.datagram, volume);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteByte (&sv.datagram, attenuation * 64);

	if (field_mask & SND_LARGEENTITY) {
		MSG_WriteShort (&sv.datagram, ent);
		MSG_WriteByte (&sv.datagram, channel);
	} else {
		MSG_WriteShort (&sv.datagram, (ent << 3 | channel));
	}
	if (field_mask & SND_LARGESOUND)
		MSG_WriteShort (&sv.datagram, sound_num);
	else
		MSG_WriteByte (&sv.datagram, sound_num);

	VectorBlend (SVvector (entity, mins), SVvector (entity, maxs), 0.5, v);
	VectorAdd (v, SVvector (entity, origin), v);
	MSG_WriteCoordV (&sv.datagram, v);
}

// CLIENT SPAWNING ============================================================

/*
  SV_SendServerinfo

  Sends the first message from the server to a connected client.
  This will be sent on the initial connection and upon each server load.
*/
static void
SV_SendServerinfo (client_t *client)
{
	const char **s;
	char        message[2048];
	int         i;

	MSG_WriteByte (&client->message, svc_print);
	snprintf (message, sizeof (message), "%c\nVersion %s server (%i CRC)\n", 2,
			  NQ_VERSION, sv_pr_state.crc);
	MSG_WriteString (&client->message, message);

	MSG_WriteByte (&client->message, svc_serverinfo);
	MSG_WriteLong (&client->message, sv.protocol);
	MSG_WriteByte (&client->message, svs.maxclients);

	if (!coop->int_val && deathmatch->int_val)
		MSG_WriteByte (&client->message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&client->message, GAME_COOP);

	snprintf (message, sizeof (message), "%s",
			  PR_GetString (&sv_pr_state, SVstring (sv.edicts, message)));
	message[sizeof (message) - 1] = 0;

	MSG_WriteString (&client->message, message);

	// send only the first 256 model and sound precaches if protocol 15
	for (i = 0, s = sv.model_precache + 1; *s; s++, i++)
		if (sv.protocol != PROTOCOL_NETQUAKE || i < 256)
			MSG_WriteString (&client->message, *s);
	MSG_WriteByte (&client->message, 0);

	for (i = 0, s = sv.sound_precache + 1; *s; s++, i++)
		if (sv.protocol != PROTOCOL_NETQUAKE || i < 256)
			MSG_WriteString (&client->message, *s);
	MSG_WriteByte (&client->message, 0);

	// send music
	MSG_WriteByte (&client->message, svc_cdtrack);
	MSG_WriteByte (&client->message, SVfloat (sv.edicts, sounds));
	MSG_WriteByte (&client->message, SVfloat (sv.edicts, sounds));

	// set view
	MSG_WriteByte (&client->message, svc_setview);
	MSG_WriteShort (&client->message,
					NUM_FOR_EDICT (&sv_pr_state, client->edict));

	MSG_WriteByte (&client->message, svc_signonnum);
	MSG_WriteByte (&client->message, 1);

	client->sendsignon = true;
	client->spawned = false;			// need prespawn, spawn, etc
}

/*
  SV_ConnectClient

  Initializes a client_t for a new net connection.  This will be called only
  once for a player each game, not once for each level change.
*/
static void
SV_ConnectClient (int clientnum)
{
	edict_t    *ent;
	client_t   *client;
	int         edictnum;
	struct qsocket_s *netconnection;
	int         i;
	float       spawn_parms[NUM_SPAWN_PARMS];

	client = svs.clients + clientnum;

	Sys_MaskPrintf (SYS_dev, "Client %s connected\n",
					client->netconnection->address);

	edictnum = clientnum + 1;

	ent = EDICT_NUM (&sv_pr_state, edictnum);

	// set up the client_t
	netconnection = client->netconnection;

	if (sv.loadgame)
		memcpy (spawn_parms, client->spawn_parms, sizeof (spawn_parms));
	memset (client, 0, sizeof (*client));
	client->netconnection = netconnection;

	strcpy (client->name, "unconnected");
	client->active = true;
	client->spawned = false;
	client->edict = ent;
	client->message.data = client->msgbuf;
	client->message.maxsize = sizeof (client->msgbuf);
	client->message.allowoverflow = true;	// we can catch it

	client->privileged = false;

	if (sv.loadgame)
		memcpy (client->spawn_parms, spawn_parms, sizeof (spawn_parms));
	else {
		// call the progs to get default spawn parms for the new client
		PR_ExecuteProgram (&sv_pr_state, sv_funcs.SetNewParms);
		for (i = 0; i < NUM_SPAWN_PARMS; i++)
			client->spawn_parms[i] = sv_globals.parms[i];
	}

	SV_SendServerinfo (client);
}

void
SV_CheckForNewClients (void)
{
	unsigned    i;
	struct qsocket_s *ret;

	// check for new connections
	while (1) {
		ret = NET_CheckNewConnections ();
		if (!ret)
			break;

		// init a new client structure
		for (i = 0; i < svs.maxclients; i++)
			if (!svs.clients[i].active)
				break;
		if (i == svs.maxclients)
			Sys_Error ("Host_CheckForNewClients: no free clients");

		svs.clients[i].netconnection = ret;
		SV_ConnectClient (i);

		net_activeconnections++;
	}
}

// FRAME UPDATES ==============================================================

void
SV_ClearDatagram (void)
{
	SZ_Clear (&sv.datagram);
}

/*
  The PVS must include a small area around the client to allow head bobbing
  or other small motion on the client side.  Otherwise, a bob might cause an
  entity that should be visible to not show up, especially when the bob
  crosses a waterline.
*/

static set_t *fatpvs;

static void
SV_AddToFatPVS (vec3_t org, mnode_t *node)
{
	float       d;
	plane_t    *plane;

	while (1) {
		// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0) {
			if (node->contents != CONTENTS_SOLID) {
				set_union (fatpvs, Mod_LeafPVS ((mleaf_t *) node,
												sv.worldmodel));
			}
			return;
		}

		plane = node->plane;
		d = DotProduct (org, plane->normal) - plane->dist;
		if (d > 8)
			node = node->children[0];
		else if (d < -8)
			node = node->children[1];
		else {							// go down both
			SV_AddToFatPVS (org, node->children[0]);
			node = node->children[1];
		}
	}
}

/*
  SV_FatPVS

  Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
  given point.
*/
static set_t *
SV_FatPVS (vec3_t org)
{
	if (!fatpvs) {
		fatpvs = set_new_size (sv.worldmodel->brush.visleafs);
	}
	set_expand (fatpvs, sv.worldmodel->brush.visleafs);
	set_empty (fatpvs);
	SV_AddToFatPVS (org, sv.worldmodel->brush.nodes);
	return fatpvs;
}

//=============================================================================

static void
SV_WriteEntitiesToClient (edict_t *clent, sizebuf_t *msg)
{
	pr_uint_t   bits, e;
	set_t      *pvs;
	float       miss;
	vec3_t      org;
	edict_t    *ent;
	entity_state_t *baseline;
	edict_leaf_t *el;

	// find the client's PVS
	VectorAdd (SVvector (clent, origin), SVvector (clent, view_ofs), org);
	pvs = SV_FatPVS (org);

	// send over all entities (excpet the client) that touch the pvs
	ent = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, ent = NEXT_EDICT (&sv_pr_state, ent)) {
		baseline = &SVdata (ent)->state;

		// ignore if not touching a PV leaf
		if (ent != clent) {				// clent is ALWAYS sent
			// ignore ents without visible models
			if (!SVfloat (ent, modelindex) ||
				!*PR_GetString (&sv_pr_state, SVstring (ent, model)))
				continue;

			// don't send model > 255 for protocol 15
			if (sv.protocol == PROTOCOL_NETQUAKE
				&& (int) SVfloat (ent, modelindex) & 0xFF00)
				continue;

			for (el = SVdata (ent)->leafs; el; el = el->next) {
				if (set_is_member (pvs, el->leafnum))
					break;
			}

			if (!el)
				continue;				// not visible
		}

		if (msg->cursize + 24 > msg->maxsize) {
			Sys_Printf ("packet overflow\n");
			return;
		}
		// send an update
		bits = 0;


		for (int i = 0; i < 3; i++) {
			miss = SVvector (ent, origin)[i] - baseline->origin[i];
			if (miss < -0.1 || miss > 0.1)
				bits |= U_ORIGIN1 << i;
		}

		if (SVvector (ent, angles)[0] != baseline->angles[0])
			bits |= U_ANGLE1;

		if (SVvector (ent, angles)[1] != baseline->angles[1])
			bits |= U_ANGLE2;

		if (SVvector (ent, angles)[2] != baseline->angles[2])
			bits |= U_ANGLE3;

		if (SVfloat (ent, movetype) == MOVETYPE_STEP)
			bits |= U_STEP;			// don't mess up the step animation

		if (baseline->colormap != SVfloat (ent, colormap))
			bits |= U_COLORMAP;

		if (baseline->skinnum != SVfloat (ent, skin))
			bits |= U_SKIN;

		if (baseline->frame != SVfloat (ent, frame))
			bits |= U_FRAME;

		if (baseline->effects != SVfloat (ent, effects))
			bits |= U_EFFECTS;

		if (baseline->modelindex != SVfloat (ent, modelindex))
			bits |= U_MODEL;

		if (sv_fields.alpha != -1)
			SVdata (ent)->alpha = ENTALPHA_ENCODE(SVfloat (ent, alpha));

		//don't send invisible entities unless they have effects
		if (SVdata (ent)->alpha == ENTALPHA_ZERO && !SVfloat (ent, effects))
			continue;

		if (sv.protocol != PROTOCOL_NETQUAKE) {
			if (SVdata (ent)->state.alpha != SVdata (ent)->alpha)
				bits |= U_ALPHA;
			if (bits & U_FRAME && (int) SVfloat (ent, frame) & 0xFF00)
				bits |= U_FRAME2;
			if (bits & U_MODEL && (int) SVfloat (ent, modelindex) & 0xFF00)
				bits |= U_MODEL2;
			if (SVdata (ent)->sendinterval)
				bits |= U_LERPFINISH;
			if (bits >= 65536)
				bits |= U_EXTEND1;
			if (bits >= 16777216)
				bits |= U_EXTEND2;
		}


		if (e >= 256)
			bits |= U_LONGENTITY;

		if (bits >= 256)
			bits |= U_MOREBITS;

		// write the message
		MSG_WriteByte (msg, bits | U_SIGNAL);

		if (bits & U_MOREBITS)
			MSG_WriteByte (msg, bits >> 8);
		if (bits & U_EXTEND1)
			MSG_WriteByte (msg, bits >> 16);
		if (bits & U_EXTEND2)
			MSG_WriteByte (msg, bits >> 24);

		if (bits & U_LONGENTITY)
			MSG_WriteShort (msg, e);
		else
			MSG_WriteByte (msg, e);

		if (bits & U_MODEL)
			MSG_WriteByte (msg, SVfloat (ent, modelindex));
		if (bits & U_FRAME)
			MSG_WriteByte (msg, SVfloat (ent, frame));
		if (bits & U_COLORMAP)
			MSG_WriteByte (msg, SVfloat (ent, colormap));
		if (bits & U_SKIN)
			MSG_WriteByte (msg, SVfloat (ent, skin));
		if (bits & U_EFFECTS)
			MSG_WriteByte (msg, SVfloat (ent, effects));
		if (bits & U_ORIGIN1)
			MSG_WriteCoord (msg, SVvector (ent, origin)[0]);
		if (bits & U_ANGLE1)
			MSG_WriteAngle (msg, SVvector (ent, angles)[0]);
		if (bits & U_ORIGIN2)
			MSG_WriteCoord (msg, SVvector (ent, origin)[1]);
		if (bits & U_ANGLE2)
			MSG_WriteAngle (msg, SVvector (ent, angles)[1]);
		if (bits & U_ORIGIN3)
			MSG_WriteCoord (msg, SVvector (ent, origin)[2]);
		if (bits & U_ANGLE3)
			MSG_WriteAngle (msg, SVvector (ent, angles)[2]);

		if (bits & U_ALPHA)
			MSG_WriteByte(msg, SVdata (ent)->alpha);
		if (bits & U_FRAME2)
			MSG_WriteByte(msg, (int) SVfloat (ent, frame) >> 8);
		if (bits & U_MODEL2)
			MSG_WriteByte(msg, (int) SVfloat (ent, modelindex) >> 8);
		if (bits & U_LERPFINISH)
			MSG_WriteByte(msg, rint ((SVfloat (ent, nextthink) - sv.time) * 255));
	}
}

static void
SV_CleanupEnts (void)
{
	pr_uint_t   e;
	edict_t    *ent;

	ent = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, ent = NEXT_EDICT (&sv_pr_state, ent))
		SVfloat (ent, effects) = (int) SVfloat (ent, effects)
			& ~EF_MUZZLEFLASH;
}

void
SV_WriteClientdataToMessage (edict_t *ent, sizebuf_t *msg)
{
	int         bits, items, i;
	vec3_t      v;
	edict_t    *other;
	const char *weaponmodel;

	weaponmodel = PR_GetString (&sv_pr_state, SVstring (ent, weaponmodel));

	// send a damage message
	if (SVfloat (ent, dmg_take) || SVfloat (ent, dmg_save)) {
		other = PROG_TO_EDICT (&sv_pr_state, SVentity (ent, dmg_inflictor));
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, SVfloat (ent, dmg_save));
		MSG_WriteByte (msg, SVfloat (ent, dmg_take));
		VectorBlend (SVvector (other, mins), SVvector (other, maxs), 0.5, v);
		VectorAdd (v, SVvector (other, origin), v);
		MSG_WriteCoordV (msg, v);
		SVfloat (ent, dmg_take) = 0;
		SVfloat (ent, dmg_save) = 0;
	}

	// send the current viewpos offset from the view entity
	SV_SetIdealPitch ();				// how much to look up / down ideally

	// a fixangle might get lost in a dropped packet.  Oh well.
	if (SVfloat (ent, fixangle)) {
		MSG_WriteByte (msg, svc_setangle);
		MSG_WriteAngleV (msg, SVvector (ent, angles));
		SVfloat (ent, fixangle) = 0;
	}

	bits = 0;

	if (SVvector (ent, view_ofs)[2] != DEFAULT_VIEWHEIGHT)
		bits |= SU_VIEWHEIGHT;

	if (SVfloat (ent, idealpitch))
		bits |= SU_IDEALPITCH;

	// stuff the sigil bits into the high bits of items for sbar, or else
	// mix in items2
	if (sv_fields.items2 != -1)
		items = (int) SVfloat (ent, items) | ((int) SVfloat (ent, items2)
											  << 23);
	else
		items = (int) SVfloat (ent, items) | ((int) *sv_globals.serverflags
											  << 28);

	bits |= SU_ITEMS;

	if ((int) SVfloat (ent, flags) & FL_ONGROUND)
		bits |= SU_ONGROUND;

	if (SVfloat (ent, waterlevel) >= 2)
		bits |= SU_INWATER;

	for (i = 0; i < 3; i++) {
		if (SVvector (ent, punchangle)[i])
			bits |= (SU_PUNCH1 << i);
		if (SVvector (ent, velocity)[i])
			bits |= (SU_VELOCITY1 << i);
	}

	if (SVfloat (ent, weaponframe))
		bits |= SU_WEAPONFRAME;

	if (SVfloat (ent, armorvalue))
		bits |= SU_ARMOR;

//	if (SVfloat (ent, weapon))
	bits |= SU_WEAPON;

	if (sv.protocol != PROTOCOL_NETQUAKE) {
		if (bits & SU_WEAPON && SV_ModelIndex(weaponmodel) & 0xFF00)
			bits |= SU_WEAPON2;
		if ((int) SVfloat (ent, armorvalue) & 0xFF00)
			bits |= SU_ARMOR2;
		if ((int) SVfloat (ent, currentammo) & 0xFF00)
			bits |= SU_AMMO2;
		if ((int) SVfloat (ent, ammo_shells) & 0xFF00)
			bits |= SU_SHELLS2;
		if ((int) SVfloat (ent, ammo_nails) & 0xFF00)
			bits |= SU_NAILS2;
		if ((int) SVfloat (ent, ammo_rockets) & 0xFF00)
			bits |= SU_ROCKETS2;
		if ((int) SVfloat (ent, ammo_cells) & 0xFF00)
			bits |= SU_CELLS2;
		if (bits & SU_WEAPONFRAME && (int) SVfloat (ent, weaponframe) & 0xFF00)
			bits |= SU_WEAPONFRAME2;
		if (bits & SU_WEAPON && SVdata (ent)->alpha != ENTALPHA_DEFAULT)
			bits |= SU_WEAPONALPHA; //for now, weaponalpha = client entity alpha
		if (bits >= 65536)
			bits |= SU_EXTEND1;
		if (bits >= 16777216)
			bits |= SU_EXTEND2;
	}

	// send the data

	MSG_WriteByte (msg, svc_clientdata);
	MSG_WriteShort (msg, bits);

	if (bits & SU_EXTEND1)
		MSG_WriteByte(msg, bits>>16);
	if (bits & SU_EXTEND2)
		MSG_WriteByte(msg, bits>>24);

	if (bits & SU_VIEWHEIGHT)
		MSG_WriteByte (msg, SVvector (ent, view_ofs)[2]);

	if (bits & SU_IDEALPITCH)
		MSG_WriteByte (msg, SVfloat (ent, idealpitch));

	for (i = 0; i < 3; i++) {
		if (bits & (SU_PUNCH1 << i))
			MSG_WriteByte (msg, SVvector (ent, punchangle)[i]);
		if (bits & (SU_VELOCITY1 << i))
			MSG_WriteByte (msg, SVvector (ent, velocity)[i] / 16);
	}

//	if (bits & SU_ITEMS) // [always sent]
	MSG_WriteLong (msg, items);

	if (bits & SU_WEAPONFRAME)
		MSG_WriteByte (msg, SVfloat (ent, weaponframe));
	if (bits & SU_ARMOR)
		MSG_WriteByte (msg, SVfloat (ent, armorvalue));
	if (bits & SU_WEAPON)
		MSG_WriteByte (msg, SV_ModelIndex (weaponmodel));

	MSG_WriteShort (msg, SVfloat (ent, health));
	MSG_WriteByte (msg, SVfloat (ent, currentammo));
	MSG_WriteByte (msg, SVfloat (ent, ammo_shells));
	MSG_WriteByte (msg, SVfloat (ent, ammo_nails));
	MSG_WriteByte (msg, SVfloat (ent, ammo_rockets));
	MSG_WriteByte (msg, SVfloat (ent, ammo_cells));

	if (standard_quake) {
		MSG_WriteByte (msg, SVfloat (ent, weapon));
	} else {
		// NOTE: this is abysmally stupid. weapon is being treated as a
		// radio button style bit mask, limiting the available weapons to
		// 32. Sure, that's a lot of weapons, but still...
		//
		// Send the index of the lowest order set bit.
		unsigned    weapon;
		weapon = (unsigned) SVfloat (ent, weapon);
		for (i = 0; i < 32; i++) {
			if (weapon & (1 << i)) {
				MSG_WriteByte (msg, i);
				break;
			}
		}
	}

	if (bits & SU_WEAPON2)
		MSG_WriteByte (msg, SV_ModelIndex(weaponmodel) >> 8);
	if (bits & SU_ARMOR2)
		MSG_WriteByte (msg, (int) SVfloat (ent, armorvalue) >> 8);
	if (bits & SU_AMMO2)
		MSG_WriteByte (msg, (int) SVfloat (ent, currentammo) >> 8);
	if (bits & SU_SHELLS2)
		MSG_WriteByte (msg, (int) SVfloat (ent, ammo_shells) >> 8);
	if (bits & SU_NAILS2)
		MSG_WriteByte (msg, (int) SVfloat (ent, ammo_nails) >> 8);
	if (bits & SU_ROCKETS2)
		MSG_WriteByte (msg, (int) SVfloat (ent, ammo_rockets) >> 8);
	if (bits & SU_CELLS2)
		MSG_WriteByte (msg, (int) SVfloat (ent, ammo_cells) >> 8);
	if (bits & SU_WEAPONFRAME2)
		MSG_WriteByte (msg, (int) SVfloat (ent, weaponframe) >> 8);
	if (bits & SU_WEAPONALPHA)
		MSG_WriteByte (msg, SVdata (ent)->alpha); //for now, weaponalpha = client entity alpha
}

static qboolean
SV_SendClientDatagram (client_t *client)
{
	byte        buf[MAX_DATAGRAM];
	sizebuf_t   msg;

	msg.data = buf;
	msg.maxsize = sizeof (buf);
	msg.cursize = 0;

	if (strcmp (client->netconnection->address, "LOCAL") != 0)
		msg.maxsize = DATAGRAM_MTU;

	MSG_WriteByte (&msg, svc_time);
	MSG_WriteFloat (&msg, sv.time);

	// add the client specific data to the datagram
	SV_WriteClientdataToMessage (client->edict, &msg);

	SV_WriteEntitiesToClient (client->edict, &msg);

	// copy the server datagram if there is space
	if (msg.cursize + sv.datagram.cursize < msg.maxsize)
		SZ_Write (&msg, sv.datagram.data, sv.datagram.cursize);

	// send the datagram
	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1) {
		SV_DropClient (true);		// if the message couldn't send, kick off
		return false;
	}

	return true;
}

static void
SV_UpdateToReliableMessages (void)
{
	unsigned    i, j;
	client_t   *client;

	// check for changes to be sent over the reliable streams
	for (i = 0, host_client = svs.clients; i < svs.maxclients;
		 i++, host_client++) {
		if (host_client->old_frags != (int) SVfloat (host_client->edict,
													 frags)) {
			for (j = 0, client = svs.clients; j < svs.maxclients; j++,
					 client++) {
				if (!client->active)
					continue;
				MSG_WriteByte (&client->message, svc_updatefrags);
				MSG_WriteByte (&client->message, i);
				MSG_WriteShort (&client->message,
								SVfloat (host_client->edict, frags));
			}

			host_client->old_frags = SVfloat (host_client->edict, frags);
		}
	}

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++) {
		if (!client->active)
			continue;
		SZ_Write (&client->message, sv.reliable_datagram.data,
				  sv.reliable_datagram.cursize);
	}

	SZ_Clear (&sv.reliable_datagram);
}

/*
  SV_SendNop

  Send a nop message without trashing or sending the accumulated client
  message buffer
*/
static void
SV_SendNop (client_t *client)
{
	sizebuf_t   msg;
	byte        buf[4];

	msg.data = buf;
	msg.maxsize = sizeof (buf);
	msg.cursize = 0;

	MSG_WriteByte (&msg, svc_nop);

	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
		SV_DropClient (true);		// if the message couldn't send, kick off
	client->last_message = sv.time;
}

void
SV_SendClientMessages (void)
{
	unsigned    i;

	// update frags, names, etc
	SV_UpdateToReliableMessages ();

	// build individual updates
	for (i = 0, host_client = svs.clients; i < svs.maxclients;
		 i++, host_client++) {
		if (!host_client->active)
			continue;

		if (host_client->spawned) {
			if (!SV_SendClientDatagram (host_client))
				continue;
		} else {
			// The player isn't totally in the game yet
			// Send small keepalive messages if too much time has passed
			// Send a full message when the next signon stage has been
			// requested; some other message data (name changes, etc) may
			// accumulate between signon stages
			if (!host_client->sendsignon) {
				if (sv.time - host_client->last_message > 5)
					SV_SendNop (host_client);
				continue;				// don't send out non-signon messages
			}
		}

		// check for an overflowed message.  Should happen only on a very
		// bad up connection that backs up a lot, then changes level
		if (host_client->message.overflowed) {
			SV_DropClient (true);
			host_client->message.overflowed = false;
			continue;
		}

		if (host_client->message.cursize || host_client->dropasap) {
			if (!NET_CanSendMessage (host_client->netconnection)) {
//				I_Printf ("can't write\n");
				continue;
			}

			if (host_client->dropasap)
				SV_DropClient (false);	// went to another level
			else {
				if (NET_SendMessage
					(host_client->netconnection, &host_client->message) == -1)
					SV_DropClient (true);	// if the message couldn't send,
											// kick off
				SZ_Clear (&host_client->message);
				host_client->last_message = sv.time;
				host_client->sendsignon = false;
			}
		}
	}

	// clear muzzle flashes
	SV_CleanupEnts ();
}

// SERVER SPAWNING ============================================================

int
SV_ModelIndex (const char *name)
{
	int         i;

	if (!name || !name[0])
		return 0;

	for (i = 0; i < MAX_MODELS && sv.model_precache[i]; i++)
		if (!strcmp (sv.model_precache[i], name))
			return i;
	if (i == MAX_MODELS || !sv.model_precache[i])
		Sys_Error ("SV_ModelIndex: model %s not precached", name);
	return i;
}

static void
SV_CreateBaseline (void)
{
	pr_uint_t   entnum;
	edict_t    *svent;
	entity_state_t *baseline;
	int         bits;

	for (entnum = 0; entnum < sv.num_edicts; entnum++) {
		// get the current server version
		svent = EDICT_NUM (&sv_pr_state, entnum);
		baseline = &SVdata (svent)->state;

		if (svent->free)
			continue;
		if (entnum > svs.maxclients && !SVfloat (svent, modelindex))
			continue;

		// create entity baseline
		VectorCopy (SVvector (svent, origin), baseline->origin);
		VectorCopy (SVvector (svent, angles), baseline->angles);
		baseline->frame = SVfloat (svent, frame);
		baseline->skinnum = SVfloat (svent, skin);
		if (entnum > 0 && entnum <= svs.maxclients) {
			baseline->colormap = entnum;
			baseline->modelindex = SV_ModelIndex ("progs/player.mdl");
			baseline->alpha = ENTALPHA_DEFAULT;
		} else {
			const char *model;
			model = PR_GetString (&sv_pr_state, SVstring (svent, model));
			baseline->colormap = 0;
			baseline->modelindex = SV_ModelIndex (model);
			baseline->alpha = ENTALPHA_DEFAULT;
		}

		bits = 0;
		if (sv.protocol == PROTOCOL_NETQUAKE) {
			//still want to send baseline in PROTOCOL_NETQUAKE, so reset
			//these values
			if (baseline->modelindex & 0xFF00)
				baseline->modelindex = 0;
			if (baseline->frame & 0xFF00)
				baseline->frame = 0;
			baseline->alpha = ENTALPHA_DEFAULT;
		} else {
			if (baseline->modelindex & 0xFF00)
				bits |= B_LARGEMODEL;
			if (baseline->frame & 0xFF00)
				bits |= B_LARGEFRAME;
			if (baseline->alpha != ENTALPHA_DEFAULT)
				bits |= B_ALPHA;
		}

		// add to the message
		if (bits)
			MSG_WriteByte (&sv.signon, svc_spawnbaseline2);
		else
			MSG_WriteByte (&sv.signon, svc_spawnbaseline);

		MSG_WriteShort (&sv.signon, entnum);

		if (bits)
			MSG_WriteByte (&sv.signon, bits);

		if (bits & B_LARGEMODEL)
			MSG_WriteShort (&sv.signon, baseline->modelindex);
		else
			MSG_WriteByte (&sv.signon, baseline->modelindex);

		if (bits & B_LARGEFRAME)
			MSG_WriteShort (&sv.signon, baseline->frame);
		else
			MSG_WriteByte (&sv.signon, baseline->frame);
		MSG_WriteByte (&sv.signon, baseline->colormap);
		MSG_WriteByte (&sv.signon, baseline->skinnum);

		MSG_WriteCoordAngleV (&sv.signon, (vec_t*)&baseline->origin,//FIXME
							  baseline->angles);

		if (bits & B_ALPHA)
			MSG_WriteByte (&sv.signon, baseline->alpha);
	}
}

/*
  SV_SendReconnect

  Tell all the clients that the server is changing levels
*/
static void
SV_SendReconnect (void)
{
	byte        data[128];
	sizebuf_t   msg;

	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof (data);

	MSG_WriteByte (&msg, svc_stufftext);
	MSG_WriteString (&msg, "reconnect\n");
	NET_SendToAll (&msg, 5.0);

	if (!net_is_dedicated)
		Cmd_ExecuteString ("reconnect\n", src_command);
}

/*
  SV_SaveSpawnparms

  Grabs the current state of each client for saving across the
  transition to another level
*/
void
SV_SaveSpawnparms (void)
{
	unsigned    i, j;

	svs.serverflags = *sv_globals.serverflags;

	for (i = 0, host_client = svs.clients; i < svs.maxclients;
		 i++, host_client++) {
		if (!host_client->active)
			continue;

		// call the progs to get default spawn parms for the new client
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, host_client->edict);
		PR_ExecuteProgram (&sv_pr_state, sv_funcs.SetChangeParms);
		for (j = 0; j < NUM_SPAWN_PARMS; j++)
			host_client->spawn_parms[j] = sv_globals.parms[j];
	}
}

/*
  SV_SpawnServer

  This is called at the start of each level
*/
void
SV_SpawnServer (const char *server)
{
	byte       *buf;
	QFile      *ent_file;
	edict_t    *ent;


	S_BlockSound ();
	// let's not have any servers with no name
	if (hostname->string[0] == 0)
		Cvar_Set (hostname, "UNNAMED");

	Sys_MaskPrintf (SYS_dev, "SpawnServer: %s\n", server);
	svs.changelevel_issued = false;		// now safe to issue another
	svs.phys_client = SV_Physics_Client;

	// tell all connected clients that we are going to a new level
	if (sv.active) {
		SV_SendReconnect ();
	}

	// make cvars consistant
	if (coop->int_val)
		Cvar_SetValue (deathmatch, 0);
	current_skill = skill->int_val;
	if (current_skill < 0)
		current_skill = 0;
	if (current_skill > 3)
		current_skill = 3;

	Cvar_SetValue (skill, (float) current_skill);

	// set up the new server
	Host_ClearMemory ();

	memset (&sv, 0, sizeof (sv));

	strcpy (sv.name, server);

	sv.protocol = sv_protocol;

	// load progs to get entity field count
	sv.max_edicts = bound (MIN_EDICTS, max_edicts->int_val, MAX_EDICTS);
	SV_LoadProgs ();
	SV_FreeAllEdictLeafs ();

	sv.datagram.maxsize = sizeof (sv.datagram_buf);
	sv.datagram.cursize = 0;
	sv.datagram.data = sv.datagram_buf;

	sv.reliable_datagram.maxsize = sizeof (sv.reliable_datagram_buf);
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	sv.signon.maxsize = sizeof (sv.signon_buf);
	sv.signon.cursize = 0;
	sv.signon.data = sv.signon_buf;

	// leave slots at start for only clients
	sv.num_edicts = svs.maxclients + 1;
	for (unsigned i = 0; i < svs.maxclients; i++) {
		ent = EDICT_NUM (&sv_pr_state, i + 1);
		svs.clients[i].edict = ent;
	}

	sv.state = ss_loading;
	sv.paused = false;

	sv.time = 1.0;

	strcpy (sv.name, server);
	snprintf (sv.modelname, sizeof (sv.modelname), "maps/%s.bsp", server);
	sv.worldmodel = Mod_ForName (sv.modelname, false);
	if (!sv.worldmodel) {
		Sys_Printf ("Couldn't spawn server %s\n", sv.modelname);
		sv.active = false;
		S_UnblockSound ();
		return;
	}
	sv.models[1] = sv.worldmodel;

	// clear world interaction links
	SV_ClearWorld ();

	sv.sound_precache[0] = sv_pr_state.pr_strings;

	sv.model_precache[0] = sv_pr_state.pr_strings;
	sv.model_precache[1] = sv.modelname;
	for (unsigned i = 1; i < sv.worldmodel->brush.numsubmodels; i++) {
		sv.model_precache[1 + i] = localmodels[i];
		sv.models[i + 1] = Mod_ForName (localmodels[i], false);
	}

	// load the rest of the entities
	ent = EDICT_NUM (&sv_pr_state, 0);
	memset (&E_fld (ent, 0), 0, sv_pr_state.progs->entityfields * 4);
	ent->free = false;
	SVstring (ent, model) = PR_SetString (&sv_pr_state, sv.worldmodel->path);
	SVfloat (ent, modelindex) = 1;			// world model
	SVfloat (ent, solid) = SOLID_BSP;
	SVfloat (ent, movetype) = MOVETYPE_PUSH;

	if (coop->int_val)
		*sv_globals.coop = coop->int_val;
	else
		*sv_globals.deathmatch = deathmatch->int_val;

	*sv_globals.mapname = PR_SetString (&sv_pr_state, sv.name);

	// serverflags are for cross level information (sigils)
	*sv_globals.serverflags = svs.serverflags;

	*sv_globals.time = sv.time;
	ent_file = QFS_VOpenFile (va (0, "maps/%s.ent", server), 0,
							  sv.worldmodel->vpath);
	if ((buf = QFS_LoadFile (ent_file, 0))) {
		ED_LoadFromFile (&sv_pr_state, (char *) buf);
		free (buf);
	} else {
		ED_LoadFromFile (&sv_pr_state, sv.worldmodel->brush.entities);
	}

	sv.active = true;

	// all setup is completed, any further precache statements are errors
	sv.state = ss_active;

	// run two frames to allow everything to settle
	sv_frametime = host_frametime = 0.1;
	SV_Physics ();
	sv.time += host_frametime;
	SV_Physics ();
	sv.time += host_frametime;

	// create a baseline for more efficient communications
	SV_CreateBaseline ();

	if (sv.signon.cursize > 8000-2)
		Sys_Printf ("%i byte signon buffer exceeds standard limit of 7998.\n",
					sv.signon.cursize);

	// send serverinfo to all connected clients
	for (unsigned i = 0; i < svs.maxclients; i++) {
		host_client = svs.clients + i;
		if (host_client->active) {
			SV_SendServerinfo (host_client);
		}
	}

	Sys_MaskPrintf (SYS_dev, "Server spawned.\n");
	S_UnblockSound ();
}
