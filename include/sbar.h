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

void Sbar_Init (void);

struct cvar_s;
void Sbar_DMO_Init_f (void *data, const struct cvar_s *var);

typedef enum {
	sbc_ammo,
	sbc_armor,
	sbc_frags,
	sbc_health,
	sbc_info,
	sbc_items,
	sbc_weapon,
} sbar_changed;

void Sbar_Changed (sbar_changed change);
// call whenever any of the client stats represented on the sbar changes

void Sbar_Draw (void);
// called every frame by screen

void Sbar_IntermissionOverlay (void);
// called each frame after the level has been completed

void Sbar_FinaleOverlay (void);
void Sbar_DrawCenterPrint (void);
void Sbar_CenterPrint (const char *str);

void Sbar_LogFrags (void);

#endif
