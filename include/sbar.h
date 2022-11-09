/*
	sbar.h

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

*/

// the status bar is redrawn only if something has changed, but if anything
// does, the entire thing will be redrawn for the next vid.numpages frames.

#ifndef _SBAR_H
#define _SBAR_H

#define	SBAR_HEIGHT		24

//extern int sb_lines;	// scan lines to draw

typedef enum {
	sbc_ammo,
	sbc_armor,
	sbc_frags,
	sbc_health,
	sbc_info,
	sbc_items,
	sbc_weapon,
	sbc_server,

	sbc_num_changed
} sbar_changed;

struct player_info_s;
void Sbar_Init (int *stats, float *item_gettime);
void Sbar_SetPlayers (struct player_info_s *players, int maxplayers);
void Sbar_SetLevelName (const char *levelname, const char *servername);
void Sbar_SetPlayerNum (int playernum, int spectator);
void Sbar_SetViewEntity (int viewentity);
void Sbar_SetTeamplay (int teamplay);
void Sbar_SetActive (int active);

void Sbar_Update (double time);
void Sbar_UpdatePings (void);
void Sbar_UpdateFrags (int playernum);
void Sbar_UpdateInfo (int playernum);
void Sbar_UpdateStats (int stat);
void Sbar_Damage (double time);

// call whenever any of the client stats represented on the sbar changes

void Sbar_Intermission (int mode, double completed_time);
// called each frame after the level has been completed

void Sbar_FinaleOverlay (void);
void Sbar_DrawCenterPrint (void);
void Sbar_CenterPrint (const char *str);

void Sbar_LogFrags (double time);

#endif
