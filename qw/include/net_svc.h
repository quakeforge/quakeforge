/*
	net_svc.h

	(description)

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

	$Id$
*/

#ifndef NET_SVC_H
#define NET_SVC_H

#include "QF/mathlib.h"
#include "QF/msg.h"

#include "bothdefs.h"
#include "pmove.h"

typedef enum
{
	NET_OK,
	NET_SHORT,
	NET_ERROR
} net_status_t;

typedef struct net_svc_print_s
{
	byte		level;
	const char *message;
} net_svc_print_t;

typedef struct net_svc_damage_s
{
	byte		armor;
	byte		blood;
	vec3_t		from;
} net_svc_damage_t;

typedef struct net_svc_serverdata_s
{
	int			protocolversion;
	int			servercount; // FIXME: rename this
	const char *gamedir;
	byte		playernum;
	qboolean	spectator;
	const char *levelname;
	movevars_t	movevars;
} net_svc_serverdata_t;

typedef struct net_svc_sound_s
{
	short		channel;
	float		volume;
	float		attenuation;
	byte		sound_num;
	vec3_t		position;
	int			entity;
} net_svc_sound_t;

typedef struct net_svc_spawnbaseline_s
{
	short		num;
	byte		modelindex;
	byte		frame;
	byte		colormap;
	byte		skinnum;
	vec3_t		origin;
	vec3_t		angles;
} net_svc_spawnbaseline_t;

typedef struct net_svc_spawnstatic_s
{
	byte		modelindex;
	byte		frame;
	byte		colormap;
	byte		skinnum;
	vec3_t		origin;
	vec3_t		angles;
} net_svc_spawnstatic_t;

typedef struct net_svc_tempentity_s
{
	byte		type;
	vec3_t		position;
	byte		gunshotcount; // gunshot sparks
	byte		colorstart; // palette start (I think?)
	byte		colorlength; // palette length
	vec3_t		beamend; // beam endpos
	short		beamentity; // beam entity
} net_svc_tempentity_t;

typedef struct net_svc_spawnstaticsound_s
{
	vec3_t		position;
	byte		sound_num;
	byte		volume;
	byte		attenuation; // FIXME: should be a float (convert usages)
} net_svc_spawnstaticsound_t;

typedef struct net_svc_updateuserinfo_s
{
	byte		slot;
	int			userid;
	const char *userinfo;
} net_svc_updateuserinfo_t;

typedef struct net_svc_setinfo_s
{
	byte		slot;
	const char *key;
	const char *value;
} net_svc_setinfo_t;

typedef struct net_svc_serverinfo_s
{
	const char *key;
	const char *value;
} net_svc_serverinfo_t;

typedef struct net_svc_download_s
{
	short		size;
	byte		percent;
	const char *name; // only one of name or data will be set
	const byte *data;
} net_svc_download_t;

typedef struct net_svc_playerinfo_s
{
	byte		playernum;
	int			flags;
	vec3_t		origin;
	byte		frame;
	byte		msec;
	usercmd_t	usercmd;
	vec3_t		velocity;
	byte		modelindex;
	byte		skinnum;
	byte		effects;
	byte		weaponframe;
} net_svc_playerinfo_t;

typedef struct net_svc_nails_s
{
	byte		numnails;
	struct {
		vec3_t	origin;
		vec3_t	angles;
	} nails[MAX_PROJECTILES];
} net_svc_nails_t;

typedef struct net_svc_modellist_s
{
	byte        startmodel;
	const char *models[MAX_MODELS + 1]; // space left for terminating
										// empty string
	byte        nextmodel;
} net_svc_modellist_t;

typedef struct net_svc_soundlist_s
{
	byte		startsound;
	const char *sounds[MAX_SOUNDS + 1]; // space left for terminating
										// empty string
	byte		nextsound;
} net_svc_soundlist_t;

typedef struct net_svc_packetentities_s
{
	int				numwords, numdeltas;
	unsigned int	words[MAX_PACKET_ENTITIES * 2 + 1];
	entity_state_t	deltas[MAX_PACKET_ENTITIES];
} net_svc_packetentities_t;

typedef struct net_svc_deltapacketentities_s
{
	int				numwords, numdeltas;
	byte			from;
	unsigned int	words[MAX_PACKET_ENTITIES * 2 + 1];
	entity_state_t	deltas[MAX_PACKET_ENTITIES];
} net_svc_deltapacketentities_t;

net_status_t NET_SVC_Print_Emit (net_svc_print_t *block, sizebuf_t *buf);
net_status_t NET_SVC_Print_Parse (net_svc_print_t *block, msg_t *msg);
net_status_t NET_SVC_Damage_Emit (net_svc_damage_t *block, sizebuf_t *buf);
net_status_t NET_SVC_Damage_Parse (net_svc_damage_t *block, msg_t *msg);
net_status_t NET_SVC_ServerData_Emit (net_svc_serverdata_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_ServerData_Parse (net_svc_serverdata_t *block, msg_t *msg);
net_status_t NET_SVC_Sound_Emit (net_svc_sound_t *block, sizebuf_t *buf);
net_status_t NET_SVC_Sound_Parse (net_svc_sound_t *block, msg_t *msg);
net_status_t NET_SVC_SpawnBaseline_Emit (net_svc_spawnbaseline_t *block,
										 sizebuf_t *buf);
net_status_t NET_SVC_SpawnBaseline_Parse (net_svc_spawnbaseline_t *block,
									  msg_t *msg);
net_status_t NET_SVC_SpawnStatic_Emit (net_svc_spawnstatic_t *block,
									   sizebuf_t *buf);
net_status_t NET_SVC_SpawnStatic_Parse (net_svc_spawnstatic_t *block,
										msg_t *msg);
net_status_t NET_SVC_TempEntity_Emit (net_svc_tempentity_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_TempEntity_Parse (net_svc_tempentity_t *block, msg_t *msg);
net_status_t NET_SVC_SpawnStaticSound_Parse (net_svc_spawnstaticsound_t *block,
										 msg_t *msg);
net_status_t NET_SVC_UpdateUserInfo_Parse (net_svc_updateuserinfo_t *block,
									   msg_t *msg);
net_status_t NET_SVC_SetInfo_Parse (net_svc_setinfo_t *block, msg_t *msg);
net_status_t NET_SVC_ServerInfo_Parse (net_svc_serverinfo_t *block, msg_t *msg);
net_status_t NET_SVC_Download_Parse (net_svc_download_t *block, msg_t *msg);
net_status_t NET_SVC_Playerinfo_Parse (net_svc_playerinfo_t *block, msg_t *msg);
net_status_t NET_SVC_Nails_Parse (net_svc_nails_t *block, msg_t *msg);
net_status_t NET_SVC_Modellist_Parse (net_svc_modellist_t *block, msg_t *msg);
net_status_t NET_SVC_Soundlist_Parse (net_svc_soundlist_t *block, msg_t *msg);
net_status_t NET_SVC_PacketEntities_Parse (net_svc_packetentities_t *block,
									   msg_t *msg);
net_status_t NET_SVC_DeltaPacketEntities_Parse (net_svc_deltapacketentities_t *block,
											msg_t *msg);

#endif // NET_SVC_H
