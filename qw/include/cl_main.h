/*
	client.h

	Client definitions

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

#ifndef _CL_MAIN_H
#define _CL_MAIN_H

#include "client.h"
#include "QF/qtypes.h"
#include "QF/render.h"

void CL_Init (void);
void Host_WriteConfiguration (void);
int Host_ReadConfiguration (const char *cfg_name);

void CL_EstablishConnection (const char *host);

void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_NextDemo (void);
qboolean CL_DemoBehind(void);

void CL_BeginServerConnect(void);

#define emodel_name "emodel"
#define pmodel_name "pmodel"
#define prespawn_name "prespawn %i 0 %i"
#define modellist_name "modellist %i %i"
#define soundlist_name "soundlist %i %i"


extern int cl_predict_players;
extern int cl_solid_players;
extern int cl_autoexec;
extern int cl_cshift_bonus;
extern int cl_cshift_contents;
extern int cl_cshift_damage;
extern int cl_cshift_powerup;

extern struct gib_event_s *cl_player_health_e, *cl_chat_e;

#endif // _CL_MAIN_H
