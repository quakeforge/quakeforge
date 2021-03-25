/*
	cl_ents.h

	Client entity definitions

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

#ifndef _CL_ENTS_H
#define _CL_ENTS_H

#include "QF/qtypes.h"
#include "QF/render.h"

void CL_SetSolidPlayers (int playernum);
void CL_ClearPredict (void);
void CL_SetUpPlayerPrediction(qboolean dopred);
void CL_ClearEnts (void);
void CL_EmitEntities (void);
void CL_ClearProjectiles (void);
void CL_ParsePacketEntities (qboolean delta);
void CL_SetSolidEntities (void);
void CL_ParsePlayerinfo (void);
void CL_Ents_Init (void);

extern struct cvar_s   *cl_deadbodyfilter;
extern struct cvar_s   *cl_gibfilter;

extern entity_t cl_player_ents[];
extern entity_t cl_flag_ents[];
extern entity_t cl_packet_ents[];

#endif
