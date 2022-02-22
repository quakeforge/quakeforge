/*
	chase.h

	@description@

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

#ifndef __client_chase_h
#define __client_chase_h

typedef struct chasestate_s {
	struct model_s *worldmodel;
	struct viewstate_s *viewstate;
} chasestate_t;

extern	struct cvar_s	*chase_active;

void Chase_Init_Cvars (void);
void Chase_Reset (void);
void Chase_Update (chasestate_t *cs);

#endif // __client_chase_h
