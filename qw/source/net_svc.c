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
#include "net_svc.h"

qboolean
NET_SVC_Print_Parse (net_svc_print_t *print, msg_t *message)
{
	print->level = MSG_ReadByte (message);
	print->message = MSG_ReadString (message);

	return message->badread;
}

qboolean
NET_SVC_Damage_Parse (net_svc_damage_t *damage, msg_t *message)
{
	int i;

	damage->armor = MSG_ReadByte (message);
	damage->blood = MSG_ReadByte (message);
	for (i = 0; i < 3; i++)
		damage->from[i] = MSG_ReadCoord (message);

	return message->badread;
}

qboolean
NET_SVC_ServerData_Parse (net_svc_serverdata_t *serverdata, msg_t *message)
{
	serverdata->protocolversion = MSG_ReadLong (message);
	// I could abort now if the version is wrong, but why bother?
	serverdata->servercount = MSG_ReadLong (message);
	serverdata->gamedir = MSG_ReadString (message);

	// high bit means spectator
	serverdata->playernum = MSG_ReadByte (message);
	serverdata->spectator = serverdata->playernum >> 7;
	serverdata->playernum &= ~(1 << 7);

	serverdata->levelname = MSG_ReadString (message);
	serverdata->movevars.gravity = MSG_ReadFloat (message);
	serverdata->movevars.stopspeed = MSG_ReadFloat (message);
	serverdata->movevars.maxspeed = MSG_ReadFloat (message);
	serverdata->movevars.spectatormaxspeed = MSG_ReadFloat (message);
	serverdata->movevars.accelerate = MSG_ReadFloat (message);
	serverdata->movevars.airaccelerate = MSG_ReadFloat (message);
	serverdata->movevars.wateraccelerate = MSG_ReadFloat (message);
	serverdata->movevars.friction = MSG_ReadFloat (message);
	serverdata->movevars.waterfriction = MSG_ReadFloat (message);
	serverdata->movevars.entgravity = MSG_ReadFloat (message);

	return message->badread;
}

qboolean
NET_SVC_Sound_Parse (net_svc_sound_t *sound, msg_t *message)
{
	int i;

	sound->channel = MSG_ReadShort (message);
	if (sound->channel & SND_VOLUME)
		sound->volume = MSG_ReadByte (message) / 255.0;
	else
		sound->volume = DEFAULT_SOUND_PACKET_VOLUME / 255.0;

	if (sound->channel & SND_ATTENUATION)
		sound->attenuation = MSG_ReadByte (message) / 64.0;
	else
		sound->attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	sound->sound_num = MSG_ReadByte (message);

	for (i = 0; i < 3; i++)
		sound->position[i] = MSG_ReadCoord (message);

	sound->entity = (sound->channel >> 3) & 1023;
	sound->channel &= 7;

	return message->badread;
}

qboolean
NET_SVC_UpdateUserInfo_Parse (net_svc_updateuserinfo_t *updateuserinfo,
							  msg_t *message)
{
	updateuserinfo->slot = MSG_ReadByte (message);
	updateuserinfo->userid = MSG_ReadLong (message);
	updateuserinfo->userinfo = MSG_ReadString (message);

	return message->badread;
}

qboolean
NET_SVC_SetInfo_Parse (net_svc_setinfo_t *setinfo, msg_t *message)
{
	setinfo->slot = MSG_ReadByte (message);
	setinfo->key = MSG_ReadString (message);
	setinfo->value = MSG_ReadString (message);

	return message->badread;
}

qboolean
NET_SVC_Download_Parse (net_svc_download_t *download, msg_t *message)
{
	download->size = MSG_ReadShort (message);
	download->percent = MSG_ReadByte (message);
	download->name = download->data = 0;

	if (download->size == -2)
		download->name = MSG_ReadString (message);
	else if (download->size > 0) {
		// FIXME: this should really be a MSG function
		if (message->readcount + download->size <= message->message->cursize) {
			download->data = message->message->data + message->readcount;
			message->readcount += download->size;
		} else {
			// size was beyond the end of the packet
			message->readcount = message->message->cursize;
			message->badread = true;
			download->size = 0;
		}
	}

	return message->badread;
}

qboolean
NET_SVC_Soundlist_Parse (net_svc_soundlist_t *soundlist, msg_t *message)
{
	int i;

	soundlist->startsound = MSG_ReadByte (message);

	for (i = 0; i < MAX_MODELS; i++) {
		soundlist->sounds[i] = MSG_ReadString (message);
		if (!*soundlist->sounds[i])
			break;
	}
	// this is a bit redundant, but I think the robustness is a good thing
	soundlist->sounds[MAX_SOUNDS] = "";

	soundlist->nextsound = MSG_ReadByte (message);

	return message->badread;
}

qboolean
NET_SVC_Modellist_Parse (net_svc_modellist_t *modellist, msg_t *message)
{
	int i;

	modellist->startmodel = MSG_ReadByte (message);

	for (i = 0; i < MAX_MODELS; i++) {
		modellist->models[i] = MSG_ReadString (message);
		if (!*modellist->models[i])
			break;
	}
	// this is a bit redundant, but I think the robustness is a good thing
	modellist->models[MAX_MODELS] = "";

	modellist->nextmodel = MSG_ReadByte (message);

	return message->badread;
}

