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

typedef struct net_svc_print_s
{
	byte		level;
	const char *message;
} net_svc_print_t;

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

typedef struct net_svc_download_s
{
	short		size;
	byte		percent;
	const char *name; // only one of name or data will be set
	const byte *data;
} net_svc_download_t;

typedef struct net_svc_soundlist_s
{
	byte		startsound;
	const char *sounds[MAX_SOUNDS + 1]; // space left for terminating empty string
	byte		nextsound;
} net_svc_soundlist_t;

typedef struct net_svc_modellist_s
{
	byte		startmodel;
	const char *models[MAX_MODELS + 1]; // space left for terminating empty string
	byte		nextmodel;
} net_svc_modellist_t;

void NET_SVC_Print_Parse (net_svc_print_t *print, msg_t *message);
void NET_SVC_UpdateUserInfo_Parse (net_svc_updateuserinfo_t *updateuserinfo,
								   msg_t *message);
void NET_SVC_SetInfo_Parse (net_svc_setinfo_t *setinfo, msg_t *message);
void NET_SVC_Download_Parse (net_svc_download_t *download, msg_t *message);
void NET_SVC_Soundlist_Parse (net_svc_soundlist_t *soundlist, msg_t *message);
void NET_SVC_Modellist_Parse (net_svc_modellist_t *modellist, msg_t *message);

#endif // NET_SVC_H
