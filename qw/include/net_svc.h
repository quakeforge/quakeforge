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


typedef struct net_svc_nop_s
{
} net_svc_nop_t;

typedef struct net_svc_disconnect_s
{
} net_svc_disconnect_t;

typedef struct net_svc_print_s
{
	byte		level;
	const char *message;
} net_svc_print_t;

typedef struct net_svc_centerprint_s
{
	const char *message;
} net_svc_centerprint_t;

typedef struct net_svc_stufftext_s
{
	const char *commands;
} net_svc_stufftext_t;

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

typedef struct net_svc_setangle_s
{
	vec3_t		angles;
} net_svc_setangle_t;

typedef struct net_svc_lightstyle_s
{
	byte		stylenum;
	const char *map;
} net_svc_lightstyle_t;

typedef struct net_svc_sound_s
{
	short		channel;
	float		volume;
	float		attenuation;
	byte		sound_num;
	vec3_t		position;
	int			entity;
} net_svc_sound_t;

typedef struct net_svc_stopsound_s
{
	int			entity;
	int			channel;
} net_svc_stopsound_t;

typedef struct net_svc_updatefrags_s
{
	byte		player;
	short		frags;
} net_svc_updatefrags_t;

typedef struct net_svc_updateping_s
{
	byte		player;
	short		ping;
} net_svc_updateping_t;

typedef struct net_svc_updatepl_s
{
	byte		player;
	byte		packetloss;
} net_svc_updatepl_t;

typedef struct net_svc_updateentertime_s
{
	byte		player;
	float		secondsago;
} net_svc_updateentertime_t;

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

typedef struct net_svc_killedmonster_s
{
} net_svc_killedmonster_t;

typedef struct net_svc_foundsecret_s
{
} net_svc_foundsecret_t;

typedef struct net_svc_spawnstaticsound_s
{
	vec3_t		position;
	byte		sound_num;
	byte		volume;
	byte		attenuation; // FIXME: should be a float (convert usages)
} net_svc_spawnstaticsound_t;

typedef struct net_svc_updatestat_s
{
	byte		stat;
	byte		value;
} net_svc_updatestat_t;

typedef struct net_svc_updatestatlong_s
{
	byte		stat;
	int			value;
} net_svc_updatestatlong_t;

typedef struct net_svc_cdtrack_s
{
	byte		cdtrack;
} net_svc_cdtrack_t;

typedef struct net_svc_intermission_s
{
	vec3_t		origin;
	vec3_t		angles;
} net_svc_intermission_t;

typedef struct net_svc_finale_s
{
	const char *message;
} net_svc_finale_t;

typedef struct net_svc_sellscreen_s
{
} net_svc_sellscreen_t;

typedef struct net_svc_smallkick_s
{
} net_svc_smallkick_t;

typedef struct net_svc_bigkick_s
{
} net_svc_bigkick_t;

typedef struct net_svc_muzzleflash_s
{
	short		player;
} net_svc_muzzleflash_t;

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

typedef struct net_svc_chokecount_s
{
	byte		count;
} net_svc_chokecount_t;

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

typedef struct net_svc_maxspeed_s
{
	float		maxspeed;
} net_svc_maxspeed_t;

typedef struct net_svc_entgravity_s
{
	float		gravity;
} net_svc_entgravity_t;

typedef struct net_svc_setpause_s
{
	byte		paused;
} net_svc_setpause_t;

const char *NET_SVC_GetString (int type);
net_status_t NET_SVC_NOP_Emit (net_svc_nop_t *block, sizebuf_t *buf);
net_status_t NET_SVC_NOP_Parse (net_svc_nop_t *block, qmsg_t *msg);
net_status_t NET_SVC_Disconnect_Emit (net_svc_disconnect_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_Disconnect_Parse (net_svc_disconnect_t *block, qmsg_t *msg);
net_status_t NET_SVC_Print_Emit (net_svc_print_t *block, sizebuf_t *buf);
net_status_t NET_SVC_Print_Parse (net_svc_print_t *block, qmsg_t *msg);
net_status_t NET_SVC_Centerprint_Emit (net_svc_centerprint_t *block,
									   sizebuf_t *buf);
net_status_t NET_SVC_Centerprint_Parse (net_svc_centerprint_t *block,
										qmsg_t *msg);
net_status_t NET_SVC_Stufftext_Emit (net_svc_stufftext_t *block,
									 sizebuf_t *buf);
net_status_t NET_SVC_Stufftext_Parse (net_svc_stufftext_t *block, qmsg_t *msg);
net_status_t NET_SVC_Damage_Emit (net_svc_damage_t *block, sizebuf_t *buf);
net_status_t NET_SVC_Damage_Parse (net_svc_damage_t *block, qmsg_t *msg);
net_status_t NET_SVC_ServerData_Emit (net_svc_serverdata_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_ServerData_Parse (net_svc_serverdata_t *block, qmsg_t *msg);
net_status_t NET_SVC_SetAngle_Emit (net_svc_setangle_t *block, sizebuf_t *buf);
net_status_t NET_SVC_SetAngle_Parse (net_svc_setangle_t *block, qmsg_t *msg);
net_status_t NET_SVC_LightStyle_Emit (net_svc_lightstyle_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_LightStyle_Parse (net_svc_lightstyle_t *block, qmsg_t *msg);
net_status_t NET_SVC_Sound_Emit (net_svc_sound_t *block, sizebuf_t *buf);
net_status_t NET_SVC_Sound_Parse (net_svc_sound_t *block, qmsg_t *msg);
net_status_t NET_SVC_StopSound_Emit (net_svc_stopsound_t *block,
									 sizebuf_t *buf);
net_status_t NET_SVC_StopSound_Parse (net_svc_stopsound_t *block, qmsg_t *msg);
net_status_t NET_SVC_UpdateFrags_Emit (net_svc_updatefrags_t *block,
									   sizebuf_t *buf);
net_status_t NET_SVC_UpdateFrags_Parse (net_svc_updatefrags_t *block,
										qmsg_t *msg);
net_status_t NET_SVC_UpdatePing_Emit (net_svc_updateping_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_UpdatePing_Parse (net_svc_updateping_t *block, qmsg_t *msg);
net_status_t NET_SVC_UpdatePL_Emit (net_svc_updatepl_t *block, sizebuf_t *buf);
net_status_t NET_SVC_UpdatePL_Parse (net_svc_updatepl_t *block, qmsg_t *msg);
net_status_t NET_SVC_UpdateEnterTime_Emit (net_svc_updateentertime_t *block,
										   sizebuf_t *buf);
net_status_t NET_SVC_UpdateEnterTime_Parse (net_svc_updateentertime_t *block,
											qmsg_t *msg);
net_status_t NET_SVC_SpawnBaseline_Emit (net_svc_spawnbaseline_t *block,
										 sizebuf_t *buf);
net_status_t NET_SVC_SpawnBaseline_Parse (net_svc_spawnbaseline_t *block,
									  qmsg_t *msg);
net_status_t NET_SVC_SpawnStatic_Emit (net_svc_spawnstatic_t *block,
									   sizebuf_t *buf);
net_status_t NET_SVC_SpawnStatic_Parse (net_svc_spawnstatic_t *block,
										qmsg_t *msg);
net_status_t NET_SVC_TempEntity_Emit (net_svc_tempentity_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_TempEntity_Parse (net_svc_tempentity_t *block, qmsg_t *msg);
net_status_t NET_SVC_KilledMonster_Emit (net_svc_killedmonster_t *block,
										 sizebuf_t *buf);
net_status_t NET_SVC_KilledMonster_Parse (net_svc_killedmonster_t *block,
										  qmsg_t *msg);
net_status_t NET_SVC_FoundSecret_Emit (net_svc_foundsecret_t *block,
									   sizebuf_t *buf);
net_status_t NET_SVC_FoundSecret_Parse (net_svc_foundsecret_t *block,
										qmsg_t *msg);
net_status_t NET_SVC_SpawnStaticSound_Emit (net_svc_spawnstaticsound_t *block,
											sizebuf_t *buf);
net_status_t NET_SVC_SpawnStaticSound_Parse (net_svc_spawnstaticsound_t *block,
										 qmsg_t *msg);
net_status_t NET_SVC_UpdateStat_Emit (net_svc_updatestat_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_UpdateStat_Parse (net_svc_updatestat_t *block, qmsg_t *msg);
net_status_t NET_SVC_UpdateStatLong_Emit (net_svc_updatestatlong_t *block,
										  sizebuf_t *buf);
net_status_t NET_SVC_UpdateStatLong_Parse (net_svc_updatestatlong_t *block,
										   qmsg_t *msg);
net_status_t NET_SVC_CDTrack_Emit (net_svc_cdtrack_t *block, sizebuf_t *buf);
net_status_t NET_SVC_CDTrack_Parse (net_svc_cdtrack_t *block, qmsg_t *msg);
net_status_t NET_SVC_Intermission_Emit (net_svc_intermission_t *block,
										sizebuf_t *buf);
net_status_t NET_SVC_Intermission_Parse (net_svc_intermission_t *block,
										 qmsg_t *msg);
net_status_t NET_SVC_Finale_Emit (net_svc_finale_t *block, sizebuf_t *buf);
net_status_t NET_SVC_Finale_Parse (net_svc_finale_t *block, qmsg_t *msg);
net_status_t NET_SVC_SellScreen_Emit (net_svc_sellscreen_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_SellScreen_Parse (net_svc_sellscreen_t *block, qmsg_t *msg);
net_status_t NET_SVC_SmallKick_Emit (net_svc_smallkick_t *block,
									 sizebuf_t *buf);
net_status_t NET_SVC_SmallKick_Parse (net_svc_smallkick_t *block, qmsg_t *msg);
net_status_t NET_SVC_BigKick_Emit (net_svc_bigkick_t *block, sizebuf_t *buf);
net_status_t NET_SVC_BigKick_Parse (net_svc_bigkick_t *block, qmsg_t *msg);
net_status_t NET_SVC_MuzzleFlash_Emit (net_svc_muzzleflash_t *block,
									   sizebuf_t *buf);
net_status_t NET_SVC_MuzzleFlash_Parse (net_svc_muzzleflash_t *block,
										qmsg_t *msg);
net_status_t NET_SVC_UpdateUserInfo_Emit (net_svc_updateuserinfo_t *block,
										  sizebuf_t *buf);
net_status_t NET_SVC_UpdateUserInfo_Parse (net_svc_updateuserinfo_t *block,
									   qmsg_t *msg);
net_status_t NET_SVC_SetInfo_Emit (net_svc_setinfo_t *block, sizebuf_t *buf);
net_status_t NET_SVC_SetInfo_Parse (net_svc_setinfo_t *block, qmsg_t *msg);
net_status_t NET_SVC_ServerInfo_Emit (net_svc_serverinfo_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_ServerInfo_Parse (net_svc_serverinfo_t *block, qmsg_t *msg);
net_status_t NET_SVC_Download_Emit (net_svc_download_t *block, sizebuf_t *buf);
net_status_t NET_SVC_Download_Parse (net_svc_download_t *block, qmsg_t *msg);
net_status_t NET_SVC_Playerinfo_Emit (net_svc_playerinfo_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_Playerinfo_Parse (net_svc_playerinfo_t *block, qmsg_t *msg);
net_status_t NET_SVC_Nails_Emit (net_svc_nails_t *block, sizebuf_t *buf);
net_status_t NET_SVC_Nails_Parse (net_svc_nails_t *block, qmsg_t *msg);
net_status_t NET_SVC_ChokeCount_Emit (net_svc_chokecount_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_ChokeCount_Parse (net_svc_chokecount_t *block, qmsg_t *msg);
net_status_t NET_SVC_Modellist_Emit (net_svc_modellist_t *block,
									 sizebuf_t *buf);
net_status_t NET_SVC_Modellist_Parse (net_svc_modellist_t *block, qmsg_t *msg);
net_status_t NET_SVC_Soundlist_Emit (net_svc_soundlist_t *block,
									 sizebuf_t *buf);
net_status_t NET_SVC_Soundlist_Parse (net_svc_soundlist_t *block, qmsg_t *msg);
net_status_t NET_SVC_PacketEntities_Emit (net_svc_packetentities_t *block,
										  sizebuf_t *buf);
net_status_t NET_SVC_PacketEntities_Parse (net_svc_packetentities_t *block,
										   qmsg_t *msg);
net_status_t NET_SVC_DeltaPacketEntities_Emit (net_svc_deltapacketentities_t *block,
											   sizebuf_t *buf);
net_status_t NET_SVC_DeltaPacketEntities_Parse (net_svc_deltapacketentities_t *block,
												qmsg_t *msg);
net_status_t NET_SVC_MaxSpeed_Emit (net_svc_maxspeed_t *block, sizebuf_t *buf);
net_status_t NET_SVC_MaxSpeed_Parse (net_svc_maxspeed_t *block, qmsg_t *msg);
net_status_t NET_SVC_EntGravity_Emit (net_svc_entgravity_t *block,
									  sizebuf_t *buf);
net_status_t NET_SVC_EntGravity_Parse (net_svc_entgravity_t *block, qmsg_t *msg);
net_status_t NET_SVC_SetPause_Emit (net_svc_setpause_t *block, sizebuf_t *buf);
net_status_t NET_SVC_SetPause_Parse (net_svc_setpause_t *block, qmsg_t *msg);

#endif // NET_SVC_H
