/*
	cl_cam.h

	Client camera definitions

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

#ifndef _CL_CAM_H
#define _CL_CAM_H

// since all headers are circular-protected with #ifdef _xxx_H
// try to get them self-sufficient by including whatever other
// headers they might need


#include "qw/protocol.h"

#define CAM_NONE	0
#define CAM_TRACK	1

extern	int	autocam;
extern	int	spec_track; // player# of who we are tracking
extern	int ideal_track;

void Cam_Lock (int playernum);
int Cam_TrackNum (void) __attribute__((pure));
qboolean Cam_DrawViewModel(void) __attribute__((pure));
qboolean Cam_DrawPlayer(int playernum) __attribute__((pure));
void Cam_Track(usercmd_t *cmd);
void Cam_FinishMove(usercmd_t *cmd);
void Cam_Reset(void);
void CL_Cam_Init(void);
void CL_Cam_Init_Cvars(void);

void CL_ParseEntityLump(const char *entdata);

#endif // _CL_CAM_H
