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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

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

static const char *net_svc_strings[] = {
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",						// [long] server version
	"svc_setview",						// [short] entity number
	"svc_sound",						// <see code>
	"svc_time",							// [float] server time
	"svc_print",						// [string] null terminated string
	"svc_stufftext",					// [string] stuffed into client's
										// console buffer the string
										// should be \n terminated
	"svc_setangle",						// [vec3] set the view angle to this
										// absolute value
	"svc_serverdata",					// [long] version ...
	"svc_lightstyle",					// [byte] [string]
	"svc_updatename",					// [byte] [string]
	"svc_updatefrags",					// [byte] [short]
	"svc_clientdata",					// <shortbits + data>
	"svc_stopsound",					// <see code>
	"svc_updatecolors",					// [byte] [byte]
	"svc_particle",						// [vec3] <variable>
	"svc_damage",						// [byte] impact [byte] blood [vec3]
										// from
	"svc_spawnstatic",
	"svc_spawnbinary",
	"svc_spawnbaseline",
	"svc_temp_entity",					// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",						// [string] music [string] text
	"svc_cdtrack",						// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_smallkick",					// Quake svc_cutscene
	"svc_bigkick",
	"svc_updateping",
	"svc_updateentertime",
	"svc_updatestatlong",
	"svc_muzzleflash",
	"svc_updateuserinfo",
	"svc_download",
	"svc_playerinfo",
	"svc_nails",
	"svc_chokecount",
	"svc_modellist",
	"svc_soundlist",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_maxspeed",
	"svc_entgravity",
	"svc_setinfo",
	"svc_serverinfo",
	"svc_updatepl",
};

const char *
NET_SVC_GetString (int type)
{
	if (type >= 0 && type < (sizeof (net_svc_strings) / sizeof (const char *)))
		return net_svc_strings[type];
	else
		return "Invalid Block Type";
}


net_status_t
NET_SVC_NOP_Emit (net_svc_nop_t *block, sizebuf_t *buf)
{
	return buf->overflowed;
}

net_status_t
NET_SVC_NOP_Parse (net_svc_nop_t *block, qmsg_t *msg)
{
	return msg->badread;
}

net_status_t
NET_SVC_Disconnect_Emit (net_svc_disconnect_t *block, sizebuf_t *buf)
{
	return buf->overflowed;
}

net_status_t
NET_SVC_Disconnect_Parse (net_svc_disconnect_t *block, qmsg_t *msg)
{
	return msg->badread;
}

net_status_t
NET_SVC_Print_Emit (net_svc_print_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->level);
	MSG_WriteString (buf, block->message);

	return buf->overflowed;
}

net_status_t
NET_SVC_Print_Parse (net_svc_print_t *block, qmsg_t *msg)
{
	block->level = MSG_ReadByte (msg);
	block->message = MSG_ReadString (msg);

	return msg->badread;
}

net_status_t
NET_SVC_Centerprint_Emit (net_svc_centerprint_t *block, sizebuf_t *buf)
{
	MSG_WriteString (buf, block->message);

	return buf->overflowed;
}

net_status_t
NET_SVC_Centerprint_Parse (net_svc_centerprint_t *block, qmsg_t *msg)
{
	block->message = MSG_ReadString (msg);

	return msg->badread;
}

net_status_t
NET_SVC_Stufftext_Emit (net_svc_stufftext_t *block, sizebuf_t *buf)
{
	MSG_WriteString (buf, block->commands);

	return buf->overflowed;
}

net_status_t
NET_SVC_Stufftext_Parse (net_svc_stufftext_t *block, qmsg_t *msg)
{
	block->commands = MSG_ReadString (msg);

	return msg->badread;
}

net_status_t
NET_SVC_Damage_Emit (net_svc_damage_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->armor);
	MSG_WriteByte (buf, block->blood);
	MSG_WriteCoordV (buf, block->from);

	return buf->overflowed;
}

net_status_t
NET_SVC_Damage_Parse (net_svc_damage_t *block, qmsg_t *msg)
{
	int i;

	block->armor = MSG_ReadByte (msg);
	block->blood = MSG_ReadByte (msg);
	block->from = MSG_ReadCoordV (msg);

	return msg->badread;
}

net_status_t
NET_SVC_ServerData_Emit (net_svc_serverdata_t *block, sizebuf_t *buf)
{
	MSG_WriteLong (buf, block->protocolversion);
	MSG_WriteLong (buf, block->servercount);
	MSG_WriteString (buf, block->gamedir);
	MSG_WriteByte (buf, block->playernum | (block->spectator ? 128 : 0));
	MSG_WriteString (buf, block->levelname);

	MSG_WriteFloat (buf, block->movevars.gravity);
	MSG_WriteFloat (buf, block->movevars.stopspeed);
	MSG_WriteFloat (buf, block->movevars.maxspeed);
	MSG_WriteFloat (buf, block->movevars.spectatormaxspeed);
	MSG_WriteFloat (buf, block->movevars.accelerate);
	MSG_WriteFloat (buf, block->movevars.airaccelerate);
	MSG_WriteFloat (buf, block->movevars.wateraccelerate);
	MSG_WriteFloat (buf, block->movevars.friction);
	MSG_WriteFloat (buf, block->movevars.waterfriction);
	MSG_WriteFloat (buf, block->movevars.entgravity);

	return buf->overflowed;
}

net_status_t
NET_SVC_ServerData_Parse (net_svc_serverdata_t *block, qmsg_t *msg)
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

net_status_t
NET_SVC_SetAngle_Emit (net_svc_setangle_t *block, sizebuf_t *buf)
{
	MSG_WriteAngleV (buf, block->angles);

	return buf->overflowed;
}

net_status_t
NET_SVC_SetAngle_Parse (net_svc_setangle_t *block, qmsg_t *msg)
{
	block->angles = MSG_ReadAngleV (msg);

	return msg->badread;
}

net_status_t
NET_SVC_LightStyle_Emit (net_svc_lightstyle_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->stylenum);
	MSG_WriteString (buf, block->map);

	return buf->overflowed;
}

net_status_t
NET_SVC_LightStyle_Parse (net_svc_lightstyle_t *block, qmsg_t *msg)
{
	block->stylenum = MSG_ReadByte (msg);
	block->map = MSG_ReadString (msg);

	return msg->badread;
}

net_status_t
NET_SVC_Sound_Emit (net_svc_sound_t *block, sizebuf_t *buf)
{
	int i, header;

	header = block->channel;
	header |= block->entity << 3;
	if (block->volume != DEFAULT_SOUND_PACKET_VOLUME)
		header |= SND_VOLUME;
	if (block->attenuation != DEFAULT_SOUND_PACKET_VOLUME)
		header |= SND_ATTENUATION;
	
	MSG_WriteShort (buf, header);
	if (header & SND_VOLUME)
		MSG_WriteByte (buf, block->volume * 255);
	if (header & SND_ATTENUATION)
		MSG_WriteByte (buf, block->attenuation * 64);
	MSG_WriteByte (buf, block->sound_num);
	for (i = 0; i < 3; i++)
		MSG_WriteCoord (buf, block->position[i]);

	return buf->overflowed;
}

net_status_t
NET_SVC_Sound_Parse (net_svc_sound_t *block, qmsg_t *msg)
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

net_status_t
NET_SVC_StopSound_Emit (net_svc_stopsound_t *block, sizebuf_t *buf)
{
	MSG_WriteShort (buf, (block->entity << 3) & (block->channel & 7));

	return buf->overflowed;
}

net_status_t
NET_SVC_StopSound_Parse (net_svc_stopsound_t *block, qmsg_t *msg)
{
	int i = MSG_ReadShort (msg);
	block->entity = i >> 3;
	block->channel = i & 7;

	return msg->badread;
}

net_status_t
NET_SVC_UpdateFrags_Emit (net_svc_updatefrags_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->player);
	MSG_WriteShort (buf, block->frags);

	return buf->overflowed;
}

net_status_t
NET_SVC_UpdateFrags_Parse (net_svc_updatefrags_t *block, qmsg_t *msg)
{
	block->player = MSG_ReadByte (msg);
	block->frags = MSG_ReadShort (msg);

	return msg->badread;
}

net_status_t
NET_SVC_UpdatePing_Emit (net_svc_updateping_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->player);
	MSG_WriteShort (buf, block->ping);

	return buf->overflowed;
}

net_status_t
NET_SVC_UpdatePing_Parse (net_svc_updateping_t *block, qmsg_t *msg)
{
	block->player = MSG_ReadByte (msg);
	block->ping = MSG_ReadShort (msg);

	return msg->badread;
}

net_status_t
NET_SVC_UpdatePL_Emit (net_svc_updatepl_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->player);
	MSG_WriteByte (buf, block->packetloss);

	return buf->overflowed;
}

net_status_t
NET_SVC_UpdatePL_Parse (net_svc_updatepl_t *block, qmsg_t *msg)
{
	block->player = MSG_ReadByte (msg);
	block->packetloss = MSG_ReadByte (msg);

	return msg->badread;
}

net_status_t
NET_SVC_UpdateEnterTime_Emit (net_svc_updateentertime_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->player);
	MSG_WriteFloat (buf, block->secondsago);

	return buf->overflowed;
}

net_status_t
NET_SVC_UpdateEnterTime_Parse (net_svc_updateentertime_t *block, qmsg_t *msg)
{
	block->player = MSG_ReadByte (msg);
	block->secondsago = MSG_ReadFloat (msg);

	return msg->badread;
}

net_status_t
NET_SVC_SpawnBaseline_Emit (net_svc_spawnbaseline_t *block, sizebuf_t *buf)
{
	int i;

	MSG_WriteShort (buf, block->num);
	MSG_WriteByte (buf, block->modelindex);
	MSG_WriteByte (buf, block->frame);
	MSG_WriteByte (buf, block->colormap);
	MSG_WriteByte (buf, block->skinnum);

	MSG_WriteCoordAngleV (buf, block->origin, block->angles);

	return buf->overflowed;
}

net_status_t
NET_SVC_SpawnBaseline_Parse (net_svc_spawnbaseline_t *block, qmsg_t *msg)
{
	block->num = MSG_ReadShort (msg);
	block->modelindex = MSG_ReadByte (msg);
	block->frame = MSG_ReadByte (msg);
	block->colormap = MSG_ReadByte (msg);
	block->skinnum = MSG_ReadByte (msg);

	MSG_ReadCoordAngleV (msg, block->origin, block->angles);

	return msg->badread;
}

net_status_t
NET_SVC_SpawnStatic_Emit (net_svc_spawnstatic_t *block, sizebuf_t *buf)
{
	int i;

	MSG_WriteByte (buf, block->modelindex);
	MSG_WriteByte (buf, block->frame);
	MSG_WriteByte (buf, block->colormap);
	MSG_WriteByte (buf, block->skinnum);
	MSG_WriteCoordAngleV (buf, block->origin, block->angles);

	return buf->overflowed;
}

net_status_t
NET_SVC_SpawnStatic_Parse (net_svc_spawnstatic_t *block, qmsg_t *msg)
{
	int i;

	block->modelindex = MSG_ReadByte (msg);
	block->frame = MSG_ReadByte (msg);
	block->colormap = MSG_ReadByte (msg);
	block->skinnum = MSG_ReadByte (msg);

	MSG_ReadCoordAngleV (msg, block->origin, block->angles);

	return msg->badread;
}

net_status_t
NET_SVC_TempEntity_Emit (net_svc_tempentity_t *block, sizebuf_t *buf)
{
	int i;

	MSG_WriteByte (buf, block->type);
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
			MSG_WriteCoordV (buf, block->position);
			break;
		case TE_LIGHTNING1:
		case TE_LIGHTNING2:
		case TE_LIGHTNING3:
		case TE_BEAM:
			MSG_WriteShort (buf, block->beamentity);
			for (i = 0; i < 3; i++)
				MSG_WriteShort (buf, block->position[i]);
			MSG_WriteCoordV (buf, block->beamend);
			break;
		case TE_EXPLOSION2:
			MSG_WriteCoordV (buf, block->position);
			MSG_WriteByte (buf, block->colorstart);
			MSG_WriteByte (buf, block->colorlength);
			break;
		case TE_GUNSHOT:
		case TE_BLOOD:
			MSG_WriteByte (buf, block->gunshotcount);
			MSG_WriteCoordV (buf, block->position);
			break;
		default:
			return NET_ERROR;
	}

	return buf->overflowed;
}

net_status_t
NET_SVC_TempEntity_Parse (net_svc_tempentity_t *block, qmsg_t *msg)
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
			MSG_ReadCoordV (msg, block->position);
			break;
		case TE_LIGHTNING1:
		case TE_LIGHTNING2:
		case TE_LIGHTNING3:
		case TE_BEAM:
			block->beamentity = MSG_ReadShort (msg);
			MSG_ReadCoordV (msg, block->position);
			MSG_ReadCoordV (msg, block->beamend);
			break;
		case TE_EXPLOSION2:
			MSG_ReadCoordV (msg, block->position);
			block->colorstart = MSG_ReadByte (msg);
			block->colorlength = MSG_ReadByte (msg);
			break;
		case TE_GUNSHOT:
		case TE_BLOOD:
			block->gunshotcount = MSG_ReadByte (msg);
			MSG_ReadCoordV (msg, block->position);
			break;
		default:
			return NET_ERROR;
	}

	return msg->badread;
}

net_status_t
NET_SVC_KilledMonster_Emit (net_svc_killedmonster_t *block, sizebuf_t *buf)
{
	return buf->overflowed;
}

net_status_t
NET_SVC_KilledMonster_Parse (net_svc_killedmonster_t *block, qmsg_t *msg)
{
	return msg->badread;
}

net_status_t
NET_SVC_FoundSecret_Emit (net_svc_foundsecret_t *block, sizebuf_t *buf)
{
	return buf->overflowed;
}

net_status_t
NET_SVC_FoundSecret_Parse (net_svc_foundsecret_t *block, qmsg_t *msg)
{
	return msg->badread;
}

net_status_t
NET_SVC_SpawnStaticSound_Emit (net_svc_spawnstaticsound_t *block,
							   sizebuf_t *buf)
{
	MSG_WriteCoordV (buf, block->position);
	MSG_WriteByte (buf, block->sound_num);
	MSG_WriteByte (buf, block->volume);
	MSG_WriteByte (buf, block->attenuation);

	return buf->overflowed;
}

net_status_t
NET_SVC_SpawnStaticSound_Parse (net_svc_spawnstaticsound_t *block,
								qmsg_t *msg)
{
	MSG_ReadCoordV (msg, block->position);
	block->sound_num = MSG_ReadByte (msg);
	block->volume = MSG_ReadByte (msg);
	block->attenuation = MSG_ReadByte (msg);

	return msg->badread;
}

net_status_t
NET_SVC_UpdateStat_Emit (net_svc_updatestat_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->stat);
	MSG_WriteByte (buf, block->value);

	return buf->overflowed;
}

net_status_t
NET_SVC_UpdateStat_Parse (net_svc_updatestat_t *block, qmsg_t *msg)
{
	block->stat = MSG_ReadByte (msg);
	block->value = MSG_ReadByte (msg);

	return msg->badread;
}

net_status_t
NET_SVC_UpdateStatLong_Emit (net_svc_updatestatlong_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->stat);
	MSG_WriteLong (buf, block->value);

	return buf->overflowed;
}

net_status_t
NET_SVC_UpdateStatLong_Parse (net_svc_updatestatlong_t *block, qmsg_t *msg)
{
	block->stat = MSG_ReadByte (msg);
	block->value = MSG_ReadLong (msg);

	return msg->badread;
}

net_status_t
NET_SVC_CDTrack_Emit (net_svc_cdtrack_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->cdtrack);

	return buf->overflowed;
}

net_status_t
NET_SVC_CDTrack_Parse (net_svc_cdtrack_t *block, qmsg_t *msg)
{
	block->cdtrack = MSG_ReadByte (msg);

	return msg->badread;
}

net_status_t
NET_SVC_Intermission_Emit (net_svc_intermission_t *block, sizebuf_t *buf)
{
	MSG_WriteCoordV (buf, block->origin);
	MSG_WriteAngleV (buf, block->angles);

	return buf->overflowed;
}

net_status_t
NET_SVC_Intermission_Parse (net_svc_intermission_t *block, qmsg_t *msg)
{
	MSG_ReadCoordV (msg, block->origin);
	MSG_ReadAngleV (msg, block->angles);

	return msg->badread;
}

net_status_t
NET_SVC_Finale_Emit (net_svc_finale_t *block, sizebuf_t *buf)
{
	MSG_WriteString (buf, block->message);

	return buf->overflowed;
}

net_status_t
NET_SVC_Finale_Parse (net_svc_finale_t *block, qmsg_t *msg)
{
	block->message = MSG_ReadString (msg);

	return msg->badread;
}

net_status_t
NET_SVC_SellScreen_Emit (net_svc_sellscreen_t *block, sizebuf_t *buf)
{
	return buf->overflowed;
}

net_status_t
NET_SVC_SellScreen_Parse (net_svc_sellscreen_t *block, qmsg_t *msg)
{
	return msg->badread;
}

net_status_t
NET_SVC_SmallKick_Emit (net_svc_smallkick_t *block, sizebuf_t *buf)
{
	return buf->overflowed;
}

net_status_t
NET_SVC_SmallKick_Parse (net_svc_smallkick_t *block, qmsg_t *msg)
{
	return msg->badread;
}

net_status_t
NET_SVC_BigKick_Emit (net_svc_bigkick_t *block, sizebuf_t *buf)
{
	return buf->overflowed;
}

net_status_t
NET_SVC_BigKick_Parse (net_svc_bigkick_t *block, qmsg_t *msg)
{
	return msg->badread;
}

net_status_t
NET_SVC_MuzzleFlash_Emit (net_svc_muzzleflash_t *block, sizebuf_t *buf)
{
	MSG_WriteShort (buf, block->player);

	return buf->overflowed;
}

net_status_t
NET_SVC_MuzzleFlash_Parse (net_svc_muzzleflash_t *block, qmsg_t *msg)
{
	block->player = MSG_ReadShort (msg);

	return msg->badread;
}

net_status_t
NET_SVC_UpdateUserInfo_Emit (net_svc_updateuserinfo_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->slot);
	MSG_WriteLong (buf, block->userid);
	MSG_WriteString (buf, block->userinfo);

	return buf->overflowed;
}

net_status_t
NET_SVC_UpdateUserInfo_Parse (net_svc_updateuserinfo_t *block, qmsg_t *msg)
{
	block->slot = MSG_ReadByte (msg);
	block->userid = MSG_ReadLong (msg);
	block->userinfo = MSG_ReadString (msg);

	return msg->badread;
}

net_status_t
NET_SVC_SetInfo_Emit (net_svc_setinfo_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->slot);
	MSG_WriteString (buf, block->key);
	MSG_WriteString (buf, block->value);

	return buf->overflowed;
}

net_status_t
NET_SVC_SetInfo_Parse (net_svc_setinfo_t *block, qmsg_t *msg)
{
	block->slot = MSG_ReadByte (msg);
	block->key = MSG_ReadString (msg);
	block->value = MSG_ReadString (msg);

	return msg->badread;
}

net_status_t
NET_SVC_ServerInfo_Emit (net_svc_serverinfo_t *block, sizebuf_t *buf)
{
	MSG_WriteString (buf, block->key);
	MSG_WriteString (buf, block->value);

	return buf->overflowed;
}

net_status_t
NET_SVC_ServerInfo_Parse (net_svc_serverinfo_t *block, qmsg_t *msg)
{
	block->key = MSG_ReadString (msg);
	block->value = MSG_ReadString (msg);

	return msg->badread;
}

net_status_t
NET_SVC_Download_Emit (net_svc_download_t *block, sizebuf_t *buf)
{
	MSG_WriteShort (buf, block->size);
	MSG_WriteByte (buf, block->percent);

	if (block->size == -2)
		MSG_WriteString (buf, block->name);
	else if (block->size > 0)
		SZ_Write (buf, block->data, block->size); // FIXME: should be a MSG_* function

	return buf->overflowed;
}

net_status_t
NET_SVC_Download_Parse (net_svc_download_t *block, qmsg_t *msg)
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

net_status_t
NET_SVC_Playerinfo_Emit (net_svc_playerinfo_t *block, sizebuf_t *buf)
{
	int i;

	MSG_WriteByte (buf, block->playernum);
	MSG_WriteShort (buf, block->flags);

	MSG_WriteCoordV (buf, block->origin);
	MSG_WriteByte (buf, block->frame);

	if (block->flags & PF_MSEC)
		MSG_WriteByte (buf, block->msec);
	if (block->flags & PF_COMMAND)
		MSG_WriteDeltaUsercmd (buf, &nullcmd, &block->usercmd); // FIXME
	for (i = 0; i < 3; i++)
		if (block->flags & (PF_VELOCITY1 << i))
			MSG_WriteShort (buf, block->velocity[i]);
	if (block->flags & PF_MODEL)
		MSG_WriteByte (buf, block->modelindex);
	if (block->flags & PF_SKINNUM)
		MSG_WriteByte (buf, block->skinnum);
	if (block->flags & PF_EFFECTS)
		MSG_WriteByte (buf, block->effects);
	if (block->flags & PF_WEAPONFRAME)
		MSG_WriteByte (buf, block->weaponframe);

	return buf->overflowed;
}

net_status_t
NET_SVC_Playerinfo_Parse (net_svc_playerinfo_t *block, qmsg_t *msg)
{
	int i;

	block->playernum = MSG_ReadByte (msg);
	block->flags = MSG_ReadShort (msg);
	MSG_ReadCoordV (msg, block->origin);
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

net_status_t
NET_SVC_Nails_Emit (net_svc_nails_t *block, sizebuf_t *buf)
{
	int		i, j;
	int		x, y, z, p, yaw;
	byte   *msg; // [48 bits] xyzpy 12 12 12 4 8

	if (block->numnails > MAX_PROJECTILES)
		return NET_ERROR;

	msg = SZ_GetSpace (buf, block->numnails * 6 + 1);
	*msg++ = block->numnails;

	for (i = 0; i < block->numnails; i++) {
		x = ((int) (block->nails[i].origin[0] + 4096 + 1) >> 1) & 4095;
		y = ((int) (block->nails[i].origin[1] + 4096 + 1) >> 1) & 4095;
		z = ((int) (block->nails[i].origin[2] + 4096 + 1) >> 1) & 4095;
		p = (int) (block->nails[i].angles[0] * (16.0 / 360.0)) & 15;
		yaw = (int) (block->nails[i].angles[1] * (256.0 / 360.0)) & 255;

		*msg++ = x;
		*msg++= (x >> 8) | (y << 4);
		*msg++ = (y >> 4);
		*msg++ = z;
		*msg++ = (z >> 8) | (p << 4);
		*msg++ = yaw;
	}

	return buf->overflowed;
}

net_status_t
NET_SVC_Nails_Parse (net_svc_nails_t *block, qmsg_t *msg)
{
	int		i, j;
	byte	bits[6];

	block->numnails = MSG_ReadByte (msg);
	for (i = 0; i < block->numnails; i++) {
		for (j = 0; j < 6; j++)
			bits[j] = MSG_ReadByte (msg);

		if (i >= MAX_PROJECTILES)
			return NET_ERROR;

		// [48 bits] xyzpy 12 12 12 4 8
		// format is 12 bits per origin coord, 4 for angles[0],
		// 8 for angles[1], and 0 for angles[2]
		block->nails[i].origin[0] = ((bits[0] + ((bits[1] & 15) << 8)) << 1) -
			4096;
		block->nails[i].origin[1] = (((bits[1] >> 4) + (bits[2] << 4)) << 1) -
			4096;
		block->nails[i].origin[2] = ((bits[3] + ((bits[4] & 15) << 8)) << 1) -
			4096;
		block->nails[i].angles[0] = 360 * (bits[4] >> 4) / 16;
		block->nails[i].angles[1] = 360 * bits[5] / 256;
		block->nails[i].angles[2] = 0;
	}

	return msg->badread;
}

net_status_t
NET_SVC_ChokeCount_Emit (net_svc_chokecount_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->count);

	return buf->overflowed;
}

net_status_t
NET_SVC_ChokeCount_Parse (net_svc_chokecount_t *block, qmsg_t *msg)
{
	block->count = MSG_ReadByte (msg);

	return msg->badread;
}

net_status_t
NET_SVC_Modellist_Emit (net_svc_modellist_t *block, sizebuf_t *buf)
{
	int i = 0;

	MSG_WriteByte (buf, block->startmodel);
	do
		MSG_WriteString (buf, block->models[i]);
	while (*block->models[i++]);
	MSG_WriteByte (buf, block->nextmodel);

	return buf->overflowed;
}

net_status_t
NET_SVC_Modellist_Parse (net_svc_modellist_t *block, qmsg_t *msg)
{
	int i;

	block->startmodel = MSG_ReadByte (msg);

	for (i = 0;; i++) {
		block->models[i] = MSG_ReadString (msg);
		if (!*block->models[i])
			break;
		if (i >= MAX_MODELS)
			return NET_ERROR;
	}

	block->nextmodel = MSG_ReadByte (msg);

	return msg->badread;
}

net_status_t
NET_SVC_Soundlist_Emit (net_svc_soundlist_t *block, sizebuf_t *buf)
{
	int i = 0;

	MSG_WriteByte (buf, block->startsound);
	do
		MSG_WriteString (buf, block->sounds[i]);
	while (*block->sounds[i++]);
	MSG_WriteByte (buf, block->nextsound);

	return buf->overflowed;
}

net_status_t
NET_SVC_Soundlist_Parse (net_svc_soundlist_t *block, qmsg_t *msg)
{
	int i;

	block->startsound = MSG_ReadByte (msg);

	for (i = 0;; i++) {
		block->sounds[i] = MSG_ReadString (msg);
		if (!*block->sounds[i])
			break;
		if (i >= MAX_SOUNDS)
			return NET_ERROR;
	}

	block->nextsound = MSG_ReadByte (msg);

	return msg->badread;
}

// this is a sub-block, not a real block
void
NET_SVC_Delta_Emit (entity_state_t *es, unsigned int bits, sizebuf_t *buf)
{
	// bytes of bits: [EXT2][EXT1][ORIG][MORE]
	bits = es->flags;

	if (bits & U_MOREBITS)
		MSG_WriteByte (buf, bits & 255);
	if (bits & U_EXTEND1) {
		MSG_WriteByte (buf, (bits >> 16) & 255);
		if (bits & U_EXTEND2)
			MSG_WriteByte (buf, (bits >> 24) & 255);
	}

	if (bits & U_MODEL)
		MSG_WriteByte (buf, es->modelindex);
	if (bits & U_FRAME)
		MSG_WriteByte (buf, es->frame);
	if (bits & U_COLORMAP)
		MSG_WriteByte (buf, es->colormap);
	if (bits & U_SKIN)
		MSG_WriteByte (buf, es->skinnum);
	if (bits & U_EFFECTS)
		MSG_WriteByte (buf, es->effects);

	if (bits & U_ORIGIN1)
		MSG_WriteCoord (buf, es->origin[0]);
	if (bits & U_ANGLE1)
		MSG_WriteAngle (buf, es->angles[0]);
	if (bits & U_ORIGIN2)
		MSG_WriteCoord (buf, es->origin[1]);
	if (bits & U_ANGLE2)
		MSG_WriteAngle (buf, es->angles[1]);
	if (bits & U_ORIGIN3)
		MSG_WriteCoord (buf, es->origin[2]);
	if (bits & U_ANGLE3)
		MSG_WriteAngle (buf, es->angles[2]);

	if (bits & U_ALPHA)
		MSG_WriteByte (buf, es->alpha);
	if (bits & U_SCALE)
		MSG_WriteByte (buf, es->scale);
	if (bits & U_EFFECTS2)
		MSG_WriteByte (buf, es->effects >> 8);
	if (bits & U_GLOWSIZE)
		MSG_WriteByte (buf, es->glow_size);
	if (bits & U_GLOWCOLOR)
		MSG_WriteByte (buf, es->glow_color);
	if (bits & U_COLORMOD)
		MSG_WriteByte (buf, es->colormod);
	if (bits & U_FRAME2)
		MSG_WriteByte (buf, es->frame >> 8);

	if (bits & U_SOLID) {
		// FIXME
	}
}

// this is a sub-block, not a real block
static void
NET_SVC_Delta_Parse (entity_state_t *es, unsigned int bits, qmsg_t *msg)
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

net_status_t
NET_SVC_PacketEntities_Emit (net_svc_packetentities_t *block, sizebuf_t *buf)
{
	int				word, delta;
	unsigned short	bits;

	for (word = 0, delta = 0; !buf->overflowed; word++) {
		if (word > MAX_PACKET_ENTITIES * 2)
			return NET_ERROR;
		bits = block->words[word];
		MSG_WriteShort (buf, bits);
		if (!bits)
			break;
		if (!(bits & U_REMOVE)) {
			if (delta >= MAX_PACKET_ENTITIES)
				return NET_ERROR;
			NET_SVC_Delta_Emit (&block->deltas[delta], bits, buf);
			delta++;
		}
	}

	return buf->overflowed;
}

net_status_t
NET_SVC_PacketEntities_Parse (net_svc_packetentities_t *block, qmsg_t *msg)
{
	int				word, delta;
	unsigned short	bits;

	for (word = 0, delta = 0; !msg->badread; word++) {
		if (word > MAX_PACKET_ENTITIES * 2)
			return NET_ERROR;
		bits = (unsigned short) MSG_ReadShort (msg);
		block->words[word] = bits;
		if (!bits)
			break;
		if (!(bits & U_REMOVE)) {
			if (delta >= MAX_PACKET_ENTITIES)
				return NET_ERROR;
			NET_SVC_Delta_Parse (&block->deltas[delta], bits, msg);
			delta++;
		}
	}

	block->numwords = word;
	block->numdeltas = delta;

	return msg->badread;
}

net_status_t
NET_SVC_DeltaPacketEntities_Emit (net_svc_deltapacketentities_t *block,
								  sizebuf_t *buf)
{
	int				word, delta;
	unsigned short	bits;

	MSG_WriteByte (buf, block->from);
	for (word = 0, delta = 0; !buf->overflowed; word++) {
		if (word > MAX_PACKET_ENTITIES * 2)
			return NET_ERROR;
		bits = block->words[word];
		MSG_WriteShort (buf, bits);
		if (!bits)
			break;
		if (!(bits & U_REMOVE)) {
			if (delta >= MAX_PACKET_ENTITIES)
				return NET_ERROR;
			NET_SVC_Delta_Emit (&block->deltas[delta], bits, buf);
			delta++;
		}
	}

	return buf->overflowed;
}

net_status_t
NET_SVC_DeltaPacketEntities_Parse (net_svc_deltapacketentities_t *block,
								   qmsg_t *msg)
{
	int				word, delta;
	unsigned short	bits;

	block->from = MSG_ReadByte (msg);
	for (word = 0, delta = 0; !msg->badread; word++) {
		if (word > MAX_PACKET_ENTITIES * 2)
			return NET_ERROR;
		bits = (unsigned short) MSG_ReadShort (msg);
		block->words[word] = bits;
		if (!bits)
			break;
		if (!(bits & U_REMOVE)) {
			if (delta >= MAX_PACKET_ENTITIES)
				return NET_ERROR;
			NET_SVC_Delta_Parse (&block->deltas[delta], bits, msg);
			delta++;
		}
	}

	block->numwords = word;
	block->numdeltas = delta;

	return msg->badread;
}

net_status_t
NET_SVC_MaxSpeed_Emit (net_svc_maxspeed_t *block, sizebuf_t *buf)
{
	MSG_WriteFloat (buf, block->maxspeed);

	return buf->overflowed;
}

net_status_t
NET_SVC_MaxSpeed_Parse (net_svc_maxspeed_t *block, qmsg_t *msg)
{
	block->maxspeed = MSG_ReadFloat (msg);

	return msg->badread;
}

net_status_t
NET_SVC_EntGravity_Emit (net_svc_entgravity_t *block, sizebuf_t *buf)
{
	MSG_WriteFloat (buf, block->gravity);

	return buf->overflowed;
}

net_status_t
NET_SVC_EntGravity_Parse (net_svc_entgravity_t *block, qmsg_t *msg)
{
	block->gravity = MSG_ReadFloat (msg);

	return msg->badread;
}

net_status_t
NET_SVC_SetPause_Emit (net_svc_setpause_t *block, sizebuf_t *buf)
{
	MSG_WriteByte (buf, block->paused);

	return buf->overflowed;
}

net_status_t
NET_SVC_SetPause_Parse (net_svc_setpause_t *block, qmsg_t *msg)
{
	block->paused = MSG_ReadByte (msg);

	return msg->badread;
}
