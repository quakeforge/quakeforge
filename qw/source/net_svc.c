/*
	net_svc.c

	(description)

	Copyright (C) 2001  Adam Olsen

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
#ifndef _WIN32
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
#else
# include <windows.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/msg.h"
#include "QF/sound.h"

#include "compat.h"
#include "msg_ucmd.h" // FIXME
#include "net_svc.h"

qboolean
NET_SVC_Print_Parse (net_svc_print_t *block, msg_t *msg)
{
	block->level = MSG_ReadByte (msg);
	block->message = MSG_ReadString (msg);

	return msg->badread;
}

qboolean
NET_SVC_Damage_Parse (net_svc_damage_t *block, msg_t *msg)
{
	int i;

	block->armor = MSG_ReadByte (msg);
	block->blood = MSG_ReadByte (msg);
	for (i = 0; i < 3; i++)
		block->from[i] = MSG_ReadCoord (msg);

	return msg->badread;
}

qboolean
NET_SVC_ServerData_Parse (net_svc_serverdata_t *block, msg_t *msg)
{
	block->protocolversion = MSG_ReadLong (msg);
	// I could abort now if the version is wrong, but why bother?
	block->servercount = MSG_ReadLong (msg);
	block->gamedir = MSG_ReadString (msg);

	// high bit means spectator
	block->playernum = MSG_ReadByte (msg);
	block->spectator = block->playernum >> 7;
	block->playernum &= ~(1 << 7);

	block->levelname = MSG_ReadString (msg);
	block->movevars.gravity = MSG_ReadFloat (msg);
	block->movevars.stopspeed = MSG_ReadFloat (msg);
	block->movevars.maxspeed = MSG_ReadFloat (msg);
	block->movevars.spectatormaxspeed = MSG_ReadFloat (msg);
	block->movevars.accelerate = MSG_ReadFloat (msg);
	block->movevars.airaccelerate = MSG_ReadFloat (msg);
	block->movevars.wateraccelerate = MSG_ReadFloat (msg);
	block->movevars.friction = MSG_ReadFloat (msg);
	block->movevars.waterfriction = MSG_ReadFloat (msg);
	block->movevars.entgravity = MSG_ReadFloat (msg);

	return msg->badread;
}

qboolean
NET_SVC_Sound_Parse (net_svc_sound_t *block, msg_t *msg)
{
	int i, header;

	header = MSG_ReadShort (msg);
	if (header & SND_VOLUME)
		block->volume = MSG_ReadByte (msg) / 255.0;
	else
		block->volume = DEFAULT_SOUND_PACKET_VOLUME / 255.0;

	if (header & SND_ATTENUATION)
		block->attenuation = MSG_ReadByte (msg) / 64.0;
	else
		block->attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	block->sound_num = MSG_ReadByte (msg);

	for (i = 0; i < 3; i++)
		block->position[i] = MSG_ReadCoord (msg);

	block->entity = (header >> 3) & 1023;
	block->channel = header & 7;

	return msg->badread;
}

qboolean
NET_SVC_SpawnBaseline_Parse (net_svc_spawnbaseline_t *block, msg_t *msg)
{
	int i;

	block->num = MSG_ReadShort (msg);
	block->modelindex = MSG_ReadByte (msg);
	block->frame = MSG_ReadByte (msg);
	block->colormap = MSG_ReadByte (msg);
	block->skinnum = MSG_ReadByte (msg);

	// these are interlaced?  bad drugs...
	for (i = 0; i < 3; i ++) {
		block->origin[i] = MSG_ReadCoord (msg);
		block->angles[i] = MSG_ReadAngle (msg);
	}

	return msg->badread;
}

qboolean
NET_SVC_SpawnStatic_Parse (net_svc_spawnstatic_t *block, msg_t *msg)
{
	int i;

	block->modelindex = MSG_ReadByte (msg);
	block->frame = MSG_ReadByte (msg);
	block->colormap = MSG_ReadByte (msg);
	block->skinnum = MSG_ReadByte (msg);

	// these are interlaced?  bad drugs...
	for (i = 0; i < 3; i ++) {
		block->origin[i] = MSG_ReadCoord (msg);
		block->angles[i] = MSG_ReadAngle (msg);
	}

	return msg->badread;
}

qboolean
NET_SVC_TempEntity_Parse (net_svc_tempentity_t *block, msg_t *msg)
{
	int i;

	block->type = MSG_ReadByte (msg);
	switch (block->type) {
		case TE_WIZSPIKE:
		case TE_KNIGHTSPIKE:
		case TE_SPIKE:
		case TE_SUPERSPIKE:
		case TE_EXPLOSION:
		case TE_TAREXPLOSION:
		case TE_LAVASPLASH:
		case TE_TELEPORT:
		case TE_LIGHTNINGBLOOD:
			for (i = 0; i < 3; i++)
				block->position[i] = MSG_ReadCoord (msg);
			break;
		case TE_LIGHTNING1:
		case TE_LIGHTNING2:
		case TE_LIGHTNING3:
		case TE_BEAM:
			block->beamentity = MSG_ReadShort (msg);
			for (i = 0; i < 3; i++)
				block->position[i] = MSG_ReadCoord (msg);
			for (i = 0; i < 3; i++)
				block->beamend[i] = MSG_ReadCoord (msg);
			break;
		case TE_EXPLOSION2:
			for (i = 0; i < 3; i++)
				block->position[i] = MSG_ReadCoord (msg);
			block->colorstart = MSG_ReadByte (msg);
			block->colorlength = MSG_ReadByte (msg);
			break;
		case TE_GUNSHOT:
		case TE_BLOOD:
			block->gunshotcount = MSG_ReadByte (msg);
			for (i = 0; i < 3; i++)
				block->position[i] = MSG_ReadCoord (msg);
			break;
	}

	return msg->badread;
}

qboolean
NET_SVC_SpawnStaticSound_Parse (net_svc_spawnstaticsound_t *block,
								msg_t *msg)
{
	int i;

	for (i = 0; i < 3; i++)
		block->position[i] = MSG_ReadCoord (msg);
	block->sound_num = MSG_ReadByte (msg);
	block->volume = MSG_ReadByte (msg);
	block->attenuation = MSG_ReadByte (msg);

	return msg->badread;
}

qboolean
NET_SVC_UpdateUserInfo_Parse (net_svc_updateuserinfo_t *block,
							  msg_t *msg)
{
	block->slot = MSG_ReadByte (msg);
	block->userid = MSG_ReadLong (msg);
	block->userinfo = MSG_ReadString (msg);

	return msg->badread;
}

qboolean
NET_SVC_SetInfo_Parse (net_svc_setinfo_t *block, msg_t *msg)
{
	block->slot = MSG_ReadByte (msg);
	block->key = MSG_ReadString (msg);
	block->value = MSG_ReadString (msg);

	return msg->badread;
}

qboolean
NET_SVC_ServerInfo_Parse (net_svc_serverinfo_t *block, msg_t *msg)
{
	block->key = MSG_ReadString (msg);
	block->value = MSG_ReadString (msg);

	return msg->badread;
}

qboolean
NET_SVC_Download_Parse (net_svc_download_t *block, msg_t *msg)
{
	block->size = MSG_ReadShort (msg);
	block->percent = MSG_ReadByte (msg);
	block->name = block->data = 0;

	if (block->size == -2)
		block->name = MSG_ReadString (msg);
	else if (block->size > 0) {
		// FIXME: this should really be a MSG function
		if (msg->readcount + block->size <= msg->message->cursize) {
			block->data = msg->message->data + msg->readcount;
			msg->readcount += block->size;
		} else {
			// size was beyond the end of the packet
			msg->readcount = msg->message->cursize;
			msg->badread = true;
			block->size = 0;
		}
	}

	return msg->badread;
}

qboolean
NET_SVC_Playerinfo_Parse (net_svc_playerinfo_t *block, msg_t *msg)
{
	int i;

	block->playernum = MSG_ReadByte (msg);
	block->flags = MSG_ReadShort (msg);
	for (i = 0; i < 3; i++)
		block->origin[i] = MSG_ReadCoord (msg);
	block->frame = MSG_ReadByte (msg);

	if (block->flags & PF_MSEC)
		block->msec = MSG_ReadByte (msg);
	else
		block->msec = 0;

	// eww, FIXME
	if (block->flags & PF_COMMAND)
		MSG_ReadDeltaUsercmd (&nullcmd, &block->usercmd);

	for (i = 0; i < 3; i++) {
		if (block->flags & (PF_VELOCITY1 << i))
			block->velocity[i] = MSG_ReadShort (msg);
		else
			block->velocity[i] = 0;
	}

	if (block->flags & PF_MODEL)
		block->modelindex = MSG_ReadByte (msg);
	else
		block->modelindex = 0; // suboptimal, but oh well

	if (block->flags & PF_SKINNUM)
		block->skinnum = MSG_ReadByte (msg);
	else
		block->skinnum = 0;

	if (block->flags & PF_EFFECTS)
		block->effects = MSG_ReadByte (msg);
	else
		block->effects = 0;

	if (block->flags & PF_WEAPONFRAME)
		block->weaponframe = MSG_ReadByte (msg);
	else
		block->weaponframe = 0;

	return msg->badread;
}

qboolean
NET_SVC_Nails_Parse (net_svc_nails_t *block, msg_t *msg)
{
	int		i, j;
	byte	bits[6];

	block->numnails = MSG_ReadByte (msg);
	for (i = 0; i < block->numnails; i++) {
		for (j = 0; j < 6; j++)
			bits[j] = MSG_ReadByte (msg);

		if (i >= MAX_PROJECTILES)
			continue;

		// [48 bits] xyzpy 12 12 12 4 8
		// format is 12 bits per origin coord, 4 for angles[0],
		// 8 for angles[1], and 0 for angles[2]
		block->nails[i].origin[0] = ((bits[0] + ((bits[1] & 15) << 8)) << 1) - 4096;
		block->nails[i].origin[1] = (((bits[1] >> 4) + (bits[2] << 4)) << 1) - 4096;
		block->nails[i].origin[2] = ((bits[3] + ((bits[4] & 15) << 8)) << 1) - 4096;
		block->nails[i].angles[0] = 360 * (bits[4] >> 4) / 16;
		block->nails[i].angles[1] = 360 * bits[5] / 256;
		block->nails[i].angles[2] = 0;
	}

	if (block->numnails > MAX_PROJECTILES)
		block->numnails = MAX_PROJECTILES;

	return msg->badread;
}

qboolean
NET_SVC_Modellist_Parse (net_svc_modellist_t *block, msg_t *msg)
{
	int i;

	block->startmodel = MSG_ReadByte (msg);

	for (i = 0; i < MAX_MODELS; i++) {
		block->models[i] = MSG_ReadString (msg);
		if (!*block->models[i])
			break;
	}
	// this is a bit redundant, but I think the robustness is a good thing
	block->models[MAX_MODELS] = "";

	block->nextmodel = MSG_ReadByte (msg);

	return msg->badread;
}

qboolean
NET_SVC_Soundlist_Parse (net_svc_soundlist_t *block, msg_t *msg)
{
	int i;

	block->startsound = MSG_ReadByte (msg);

	for (i = 0; i < MAX_MODELS; i++) {
		block->sounds[i] = MSG_ReadString (msg);
		if (!*block->sounds[i])
			break;
	}
	// this is a bit redundant, but I think the robustness is a good thing
	block->sounds[MAX_SOUNDS] = "";

	block->nextsound = MSG_ReadByte (msg);

	return msg->badread;
}

// this is a sub-block, not a real block
void
NET_SVC_Delta_Parse (entity_state_t *es, unsigned int bits, msg_t *msg)
{
	// bytes of bits: [EXT2][EXT1][ORIG][MORE]
	es->number = bits & 511;
	bits &= ~511;

	es->frame = 0;
	es->effects = 0;

	if (bits & U_MOREBITS)
		bits |= MSG_ReadByte (msg);
	if (bits & U_EXTEND1) {
		bits |= MSG_ReadByte (msg) << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte (msg) << 24;
	}

	es->flags = bits;

	if (bits & U_MODEL)
		es->modelindex = MSG_ReadByte (msg);
	if (bits & U_FRAME)
		es->frame = MSG_ReadByte (msg);
	if (bits & U_COLORMAP)
		es->colormap = MSG_ReadByte (msg);
	if (bits & U_SKIN)
		es->skinnum = MSG_ReadByte (msg);
	if (bits & U_EFFECTS)
		es->effects = MSG_ReadByte (msg);

	if (bits & U_ORIGIN1)
		es->origin[0] = MSG_ReadCoord (msg);
	if (bits & U_ANGLE1)
		es->angles[0] = MSG_ReadAngle (msg);
	if (bits & U_ORIGIN2)
		es->origin[1] = MSG_ReadCoord (msg);
	if (bits & U_ANGLE2)
		es->angles[1] = MSG_ReadAngle (msg);
	if (bits & U_ORIGIN3)
		es->origin[2] = MSG_ReadCoord (msg);
	if (bits & U_ANGLE3)
		es->angles[2] = MSG_ReadAngle (msg);

	if (bits & U_ALPHA)
		es->alpha = MSG_ReadByte (msg);
	if (bits & U_SCALE)
		es->scale = MSG_ReadByte (msg);
	if (bits & U_EFFECTS2) {
		if (bits & U_EFFECTS)
			es->effects = (es->effects & 0xFF) | (MSG_ReadByte (msg) << 8);
		else
			es->effects = MSG_ReadByte (msg) << 8;
	}
	if (bits & U_GLOWSIZE)
		es->glow_size = MSG_ReadByte (msg);
	if (bits & U_GLOWCOLOR)
		es->glow_color = MSG_ReadByte (msg);
	if (bits & U_COLORMOD)
		es->colormod = MSG_ReadByte (msg);
	if (bits & U_FRAME2) {
		if (bits & U_FRAME)
			es->frame = (es->frame & 0xFF) | (MSG_ReadByte (msg) << 8);
		else
			es->frame = MSG_ReadByte (msg) << 8;
	}

	if (bits & U_SOLID) {
		// FIXME
	}
}


qboolean
NET_SVC_PacketEntities_Parse (net_svc_packetentities_t *block, msg_t *msg)
{
	int				word, delta;
	unsigned short	bits;

	for (word = 0, delta = 0; !msg->badread; word++) {
		if (word >= MAX_PACKET_ENTITIES)
			return 1; // FIXME: differentiate from short packet
		bits = (unsigned short) MSG_ReadShort (msg);
		block->words[word] = bits;
		if (!bits)
			break;
		if (!(bits & U_REMOVE)) {
			if (delta >= MAX_PACKET_ENTITIES)
				return 1;
			NET_SVC_Delta_Parse (&block->deltas[delta], bits, msg);
			delta++;
		}
	}

	block->words[word] = 0;
	block->numwords = word;
	block->numdeltas = delta;

	return msg->badread;
}

qboolean
NET_SVC_DeltaPacketEntities_Parse (net_svc_deltapacketentities_t *block,
								   msg_t *msg)
{
	int				word, delta;
	unsigned short	bits;

	block->from = MSG_ReadByte (msg);
	for (word = 0, delta = 0; !msg->badread; word++) {
		if (word >= MAX_PACKET_ENTITIES)
			return 1; // FIXME: differentiate from short packet
		bits = (unsigned short) MSG_ReadShort (msg);
		block->words[word] = bits;
		if (!bits)
			break;
		if (!(bits & U_REMOVE)) {
			if (delta >= MAX_PACKET_ENTITIES)
				return 1;
			NET_SVC_Delta_Parse (&block->deltas[delta], bits, msg);
			delta++;
		}
	}

	block->words[word] = 0;
	block->numwords = word;
	block->numdeltas = delta;

	return msg->badread;
}
