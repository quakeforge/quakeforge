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

#include "compat.h"
#include "net_svc.h"

void
NET_SVC_Print_Parse (net_svc_print_t *print, msg_t *message)
{
	print->level = MSG_ReadByte (message);
	print->message = MSG_ReadString (message);
}

void
NET_SVC_UpdateUserInfo_Parse (net_svc_updateuserinfo_t *updateuserinfo,
							  msg_t *message)
{
	updateuserinfo->slot = MSG_ReadByte (message);
	updateuserinfo->userid = MSG_ReadLong (message);
	updateuserinfo->userinfo = MSG_ReadString (message);
}

void
NET_SVC_Download_Parse (net_svc_download_t *download, msg_t *message)
{
	download->size = MSG_ReadShort (message);
	download->percent = MSG_ReadByte (message);

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
			download->size = 0; // FIXME: CL_ParseDownload doesn't handle this
		}
	}
}

void
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
}

void
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
}

