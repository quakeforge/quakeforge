/*
	teamplay.h

	Teamplay enhancements ("proxy features")

	Copyright (C) 2000       Anton Gavrilov (tonik@quake.ru)

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

#ifndef __QF_teamplay_h
#define __QF_teamplay_h

extern struct cvar_s	*cl_parsesay;
extern struct cvar_s	*cl_nofake;
extern struct cvar_s	*cl_freply;

typedef const char *(*ffunc_t) (char *args);
typedef struct freply_s {
	const char *name;
	ffunc_t func;
	float lasttime;
} freply_t;

void Team_Init_Cvars (void);
void Team_BestWeaponImpulse (void);
void Team_Dead (void);
void Team_NewMap (void);
const char *Team_ParseSay (struct dstring_s *buf, const char *);
void Locs_Init (void);
void Team_ParseChat (const char *string);
void Team_ResetTimers (void);
#endif//__QF_teamplay_h
